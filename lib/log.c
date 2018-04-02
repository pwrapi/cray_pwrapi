/*
 * Copyright (c) 2017-2018, Cray Inc.
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * This file contains the functions necessary for interprocess communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <glib.h>

#include "log.h"

#undef  LOG_INTERNAL_DEBUG
#ifdef  LOG_INTERNAL_DEBUG
#define DBG(fmt, args...)       _dbg("[%s:%d] " fmt "\n", __func__, __LINE__, ##args)
#else
#define DBG(f, a...)
#endif

// Ring states
#define STATE_IDLE              0
#define STATE_ACTIVE            1
#define STATE_LOCKED            2
#define STATE_WRTHRU            3

// Internal limits

/*
 * Message size must be limited to 4096 bytes to protect log integrity when
 * logging to a network-based FS, like DVS. However, the pre-formatted messages
 * that are placed in this message text buffer are augmented at file-write-time
 * by some header characters, including a human-readable timestamp and other
 * information, which must be included in this 4096 limit. We've arbitrarily set
 * this header-space reservation to 128 bytes, which is more than sufficient for
 * the information that needs to be added. In practice, no log message will ever
 * be anywhere near 4096 bytes.
 */
#define MAX_MSG_SIZE            (4096 - 128)
#define MSG_HDR_SIZE            (sizeof(msgblk_t) - MAX_MSG_SIZE)
#define WRTHRU_SIZE             (MAX_MSG_SIZE*100)
#define TIME_WRITE_FMT          "%Y/%m/%d-%H:%M:%S"
#define TIME_PARSE_FMT          "%d/%d/%d-%d:%d:%d.%d"

// Forward reference
typedef struct log_context_s log_context_t;

/*
 * Message structure. This should be a multiple of 8 bytes, for alignment.
 * Actual message size should not be larger than 4096 bytes to avoid data
 * sharding with DVS and other network file systems.
 */
// Message structure
typedef struct {
	uint32_t msglen;                // bytes in this structure
	uint32_t txtlen;                // bytes in message[]
	uint32_t msgtype;               // message type value
	uint32_t tid;                   // thread identifier
	struct timeval tv;              // timestamp of message
	char message[MAX_MSG_SIZE];     // message buffer
} msgblk_t;

/*
 * Ring buffer structure. Ring buffers should be a multiple of 8 bytes, for
 * alignment. Rings do not need to be an integral multiple of any message size,
 * which is a multiple of 8, but otherwise variable.
 */
// Ring buffer structure
typedef struct {
	uint32_t size;                  // ring size in bytes
	uint32_t volatile state;        // ring state
	uint32_t volatile wridx;        // writes start here
	uint32_t volatile rdidx;        // reads start here
	char *buf;                      // ring buffer itself
} ringbuf_t;

/*
 * Write thread data.
 *
 * The operational parameters are set by the user-thread during initialization,
 * and may be modified by the write thread at startup. They do not change after
 * that.
 *
 * The communication parameters are used for information exchange between the
 * write thread and the user-thread. Generally, the user-thread will set a flag,
 * then block and wait for the flag to clear, indicating that the write thread
 * has completed the task.
 *
 * The private parameters are generally for the exclusive use of the write
 * thread, and not by user-threads.
 */
// Write thread data
typedef struct {
	// write-thread operational parameters
	log_context_t *ctx;             // logging context
	char *file_path;                // path to file
	uint64_t max_size;              // maximum file size before rotation
	uint64_t max_files;             // maximum files in rotation
	GThread *self;                  // write thread pointer

	// write-thread/user-thread communication
	uint64_t volatile write_errs;   // cumulative total of write errors
	uint64_t volatile rotations;    // cumulative total of rotations
	int volatile errcode;           // write thread "errno" value
	bool volatile terminate;        // set by user-thread to terminate
	bool volatile sync;             // set by user-thread to sync
	bool volatile rotate;           // set by user-thread to force rotation
	bool volatile autoflush_enable; // set by user-thread to autosync messages
	bool volatile logwrt_enable;    // set by user-thread to log to log file

	// write-thread private
	FILE *ctlfd;                    // control file stream descriptor
	FILE *logfd;                    // log file stream descriptor
	ino_t logino;                   // log file inode
	uint64_t written;               // bytes written (rotation check)
} thread_data_t;

/*
 * Logging context data.
 *
 * Each context is effectively a separate logging system, going to its own log
 * file, using its own ring buffers and write thread, from within a single
 * process.
 */
struct log_context_s {
	/*
	 * There is a single mutex, and (really) two conditions, idle and work.
	 *
	 * The user-thread signals the work condition when it has delivered work
	 * for the write thread to perform. The write thread signals the idle
	 * condition when it is done with work.
	 *
	 * The thrucond is a special form of idlecond, which the write thread
	 * signals when it has made a little space in the write-through ring.
	 */
	GMutex mutex;                   // used by all threads
	GCond thrucond;                 // wait by user-thread, signal by write thread
	GCond idlecond;                 // wait by user-thread, signal by write thread
	GCond workcond;                 // wait by write thread, signal by user-thread

	/*
	 * Rings, queues, and pointers.
	 */
	ringbuf_t *rings;               // array of ring structures
	uint64_t num_rings;             // number of normal rings (excl write-through)
	GQueue *lockqp;                 // queue of locked rings
	GQueue *idleqp;                 // queue of idle rings
	ringbuf_t *activering;          // pointer to current active ring
	ringbuf_t *wrthruring;          // pointer to current write-through ring

	/*
	 * The global write thread data pointer. The pointer itself is declared
	 * volatile, not all of its contents, since it is used as a flag to
	 * indicate whether the write thread is running, so its change from NULL
	 * to non-NULL could occur at any moment.
	 */
	thread_data_t *volatile wrthread_data;  // write thread data

	/*
	 * Allow init/term calls to be nested, allowing user-threads to freely
	 * start and terminate the logging.
	 */
	uint64_t volatile init_count;   // initialization nesting

	/*
	 * This mask is used to deliver messages to stderr.
	 */
	uint64_t volatile stderr_mask;  // set to log messages to stderr

	/*
	 * Cached information, set during logging initialization. The fork
	 * operation can cause these values to change, which is why logging
	 * should be terminated before, and restarted after, forking.
	 */
	char appname[16];               // application name (from /proc/self/comm)
	pid_t apppid;                   // application PID (from getpid())
};

/*
 * The GOnce variable to allow atomic, one-time initialization of mutexes, etc.
 */
static GOnce _init_once = G_ONCE_INIT;
#define	GLOBALINIT()	g_once(&_init_once, _global_init, NULL)

static GMutex _global_mutex;            // mutex for managing context list
static GList *_context_list = NULL;     // queue for new contexts

/*
 * When PMLOG_NOCOMPILE Is defined, logging defaults to disabled.
 */
#ifdef	PMLOG_NOCOMPILE
static int _log_enable = LOG_ENABLE_NONE;
#else
static int _log_enable = LOG_ENABLE_DEFAULT;
#endif

/*
 * Place to hold old signal handlers. Internal support for the first 32 signals.
 */
#define	MAX_SIG	32
static struct sigaction _old_sigact[MAX_SIG];

/*
 * Forward references.
 */
static void __attribute__((__format__(__printf__, 1, 2), unused)) \
		_dbg(const char *fmt, ...);
static void _pmlog_pre_fork(void);
static void _pmlog_post_fork_parent(void);
static void _pmlog_post_fork_child(void);

static void *_pmlog_write_thread(void *data);

/**
 * Use syscall() to get the thread identifier. We cache this in private thread
 * space for performance.
 *
 * @return pid_t - thread identifier as actual PID in Linux
 */
static inline pid_t __attribute__((unused))
gettid(void)
{
	return syscall(SYS_gettid);
}

/**
 * Fetch the environment variable PMLOG_ENABLE, and return the evaluated logging
 * enable code.
 *
 * Note that if PMLOG_NOCOMPILE is defined, this always returns LOG_ENABLE_NONE.
 *
 * @return int - LOG_ENABLE_* code
 */
static inline int
_get_log_enable(void)
{
#ifdef	PMLOG_NOCOMPILE
	return LOG_ENABLE_NONE;
#else
	const char *enable;
	enable = getenv("PMLOG_ENABLE");
	if (enable) {
		if (!strcasecmp(enable, "none"))
			return LOG_ENABLE_NONE;
		if (!strcasecmp(enable, "full"))
			return LOG_ENABLE_FULL;
	}
	return LOG_ENABLE_DEFAULT;
#endif
}

/**
 * Set logging enable code to the specified value, and return the old value.
 *
 * Note that if PMLOG_NOCOMPILE is defined, this does not change the default of
 * LOG_ENABLE_NONE, and returns the old value of LOG_ENABLE_NONE.
 *
 * @param enable - LOG_ENABLE_* code
 *
 * @return int - old enable code, or -1 if 'enable' is invalid
 */
static int
_set_log_enable(int enable)
{
#ifdef	PMLOG_NOCOMPILE
	return LOG_ENABLE_NONE;
#else
	bool old = _log_enable;
	_log_enable = enable;
	if (old != LOG_ENABLE_NONE && _log_enable == LOG_ENABLE_NONE)
		pmlog_term_all();
	return old;
#endif
}

/**
 * Do a safe check for a numeric environment variable value.
 *
 * This returns zero if the environment variable does not exist, or if it exists
 * but contains malformed (non-numeric) data. This will accept all standard
 * numeric formats.
 *
 * @param name - environment variable name
 *
 * @return int64_t - value of environment variable
 */
int64_t
getenvzero(const char *name)
{
	int64_t retval = 0;
	const char *var;

	if ((var = getenv(name)) != NULL) {
		char *p;
		retval = strtol(var, &p, 0);
		if (*p != 0)
			retval = 0;
	}
	return retval;
}

/* --------------------------------------------------------------------
 * The following section contains global context manipulation routines.
 */

/**
 * Pull the first context off the context list.
 *
 * Abstracted so that the underlying list can change.
 *
 * _global_mutex MUST BE LOCKED.
 *
 * @return log_context_t* - first context on the list
 */
static inline log_context_t *
_glb_first_context(void)
{
	log_context_t *ctx = NULL;
	if (_context_list) {
		ctx = (log_context_t *)_context_list->data;
	} else {
		errno = ENOMEM;
	}
	return ctx;
}

/**
 * Append a new context to the context list.
 *
 * Abstracted so that the underlying list can change.
 *
 * _global_mutex MUST BE LOCKED.
 *
 * @return log_context_t* - newly-created context
 */
static inline log_context_t *
_glb_new_context(void)
{
	log_context_t *ctx = calloc(1, sizeof(log_context_t));
	if (ctx) {
		g_cond_init(&ctx->thrucond);
		g_cond_init(&ctx->idlecond);
		g_cond_init(&ctx->workcond);
		_context_list = g_list_append(_context_list, ctx);
	}
	return ctx;
}

/**
 * Return the context supplied, or if ctxp is NULL, the first context on the
 * context list.
 *
 * This should always return a non-NULL context. The context may be active or
 * inactive.
 *
 * @param ctxp - desired context, or NULL for default (first)
 *
 * @return log_context_t* - a valid logging context
 */
static log_context_t *
_get_context(void *ctxp)
{
	log_context_t *ctx = (log_context_t *)ctxp;

	if (_log_enable != LOG_ENABLE_NONE && !ctx) {
		g_mutex_lock(&_global_mutex);
		ctx = _glb_first_context();
		g_mutex_unlock(&_global_mutex);
	}

	return ctx;
}

/**
 * Create a new context.
 *
 * This should always return a non-NULL context. The context will be inactive.
 *
 * @return log_context_t* - a valid logging context
 */
static log_context_t *
_new_context(void)
{
	log_context_t *ctx = NULL;
	GList *item = NULL;

	g_mutex_lock(&_global_mutex);
	// See if there is an inactive context we can reuse
	if (_context_list) {
		// Skip the default context, it's special
		for (item = _context_list->next; item; item = item->next) {
			ctx = (log_context_t *)item->data;
			if (!ctx->wrthread_data)
				break;
		}
	}
	// If all contexts are active, create a new one
	if (!item)
		ctx = _glb_new_context();
	g_mutex_unlock(&_global_mutex);

	return ctx;
}

/**
 * Add a new handler for a fatal signal that we want to catch.
 *
 * @param sig - signal number
 * @param hnd - sigaction handler
 *
 * @return int - 0 on success, -1 on failure
 */
static inline int
_sigaddhandler(int sig, void (*hnd)(int, siginfo_t *, void *))
{
	int retval = -1;
	if (sig < MAX_SIG) {
		struct sigaction sigact;
		memset(&sigact, 0, sizeof(sigact));
		sigact.sa_sigaction = hnd;
		sigact.sa_flags = SA_SIGINFO;
		retval = sigaction(sig, &sigact, &_old_sigact[sig]);
	}
	return retval;
}

/**
 * General fatal signal handler. This posts a FAULT message, then flushes the
 * ring buffers and terminating the logging system before restoring the previous
 * signal handler and re-raising the signal.
 *
 * Bitter comments about signals in Linux. There are no good solutions to this
 * for library code. What we want to do is to chain signals: that is, we do our
 * thing, then pass the signal up to the previous handler. If other libraries
 * also observe this behavior, we have good chaining.
 *
 * The problem is the legacy handlers, which are not chainable. Specifically,
 * the only way to invoke SIG_DFL behavior is to restore the SIG_DFL handler,
 * then re-raise the signal. However, the re-raised signal is not handled until
 * after our handler exits; it is deferred until we return to the user context.
 * If the default behavior is not fatal, we never get a chance to re-install our
 * own handler in the chain, so SIG_DFL behavior is ALWAYS to break the chain,
 * regardless of what else it might do.
 *
 * We presume that only fatal signals will be trapped, and in fact, only one is
 * of particular interest: SIGSEGV.
 *
 * This is why we fully terminate the logging, rather than merely flushing.
 *
 * @param sig - signal caught
 * @param siginfo - formal parameter (unused)
 * @param context - formal parameter (unused)
 */
static void
_sigfatal_handler(int sig, siginfo_t *siginfo, void *context)
{
	// Flush the fatal signal
	pmlog_message(LOG_TYPE_FAULT, "Signal %s caught", strsignal(sig));
	pmlog_term_all();
	// Restore the old handler and re-raise signal
	sigaction(sig, &_old_sigact[sig], NULL);
	raise(sig);
}

/**
 * Initialize the static memory for threads.
 *
 * This is called exactly once (for the process) in a thread-safe way, using
 * g_once(). It must be called by each exported API function (including the test
 * functions) using the GLOBALINIT() macro.
 *
 * @param data - arbitrary data
 *
 * @return void* - data used to initialize the GOnce structure
 */
static void *
_global_init(void *data)
{
	// Need this to manipulate the contexts
	g_mutex_init(&_global_mutex);
	_glb_new_context();

	// Flush all rings and clean up on exit
	atexit(pmlog_term_all);

	// Add a signal handler for segment violations
	memset(_old_sigact, 0, sizeof(_old_sigact));
	_sigaddhandler(SIGSEGV, _sigfatal_handler);

	// Add fork handlers
	pthread_atfork(_pmlog_pre_fork, _pmlog_post_fork_parent, _pmlog_post_fork_child);

	// Set a default value for logging enable from the env variable
	_log_enable = _get_log_enable();

	return data;
}

/* --------------------------------------------------------------------
 * The following section contains utility routines that are re-entrant and
 * inherently thread-safe.
 */

/**
 * This is purely for testing. It "bloats" the timing of the code, making the
 * period of holding and releasing mutexes longer, and also yielding the
 * processor to another thread, to help expose naked race conditions.
 */
static bool _bloat_enable = false;
static inline void
_bloat(void)
{
	struct timespec ns = { 0, 500 };
	if (_bloat_enable) {
		nanosleep(&ns, NULL);
	}
}

/**
 * Format a message for logging.
 *
 * This call takes the timestamp snapshot, and marks the instant in time that
 * is logged.
 *
 * The return value is the actual number of bytes in the buffer, excluding the
 * final NUL character, even if the message was truncated. Thus, it can never be
 * returned as a value greater than siz-1.
 *
 * The 'fmt' parameter can be specified as NULL, in which case this will
 * generate only the header.
 *
 * This does NOT append a linefeed to the formatted string.
 *
 * @param msg - target message block
 * @param msgtype - message type value
 * @param fmt - printf-like format specifier, or NULL to generate hdr only
 * @param args - va_list arguments to printf
 *
 * @return ssize_t - bytes written to the message buffer, -1 on error
 */
static ssize_t
_vmsgformat(msgblk_t *msg, int msgtype, const char *fmt, va_list args)
{
	ssize_t len = 0;
	int errnum = errno;     // preserve entry value
	// Capture essential information
	// Snapshot of time as close as possible to the call
	gettimeofday(&msg->tv, NULL);
	msg->tid = gettid();
	msg->msgtype = msgtype;
	errno = errnum;         // restore, for %m format descriptor
	// Generate the message text
	if (fmt != NULL) {
		len = vsnprintf(msg->message, MAX_MSG_SIZE, fmt, args);
		if (len < 0)
			return -1;
		if (len > MAX_MSG_SIZE - 1)
			len = MAX_MSG_SIZE - 1;
	}
	// Record the length
	msg->txtlen = len;
	// Pad with NUL to an 8-byte boundary
	msg->msglen = 1 + len + MSG_HDR_SIZE;
	while ((msg->msglen & 0x7)) {
		msg->message[msg->msglen++] = 0;
	}
	// Return the length
	return len;
}

/**
 * This list must match the order of message types in log.h
 */
const char *_typetxt[] = {
	"CONS",
	"LOGR",
	"MESG",
	"CRIT",
	"WARN",
	"FAIL",
	"TRC1",
	"TRC2",
	"TRC3",
	"DBG1",
	"DBG2"
};

/**
 * Convert the message type number into a string.
 *
 * @param msgtype - message type value
 * @param buf - buffer to use if value is not known
 * @param siz - size of the buffer
 *
 * @return const char*
 */
static const char *
_fmttype(int msgtype, char *buf, size_t siz)
{
	if (msgtype < NUM_LOG_TYPES)
		return _typetxt[msgtype];
	snprintf(buf, siz, "USR%d", msgtype);
	return (const char *)buf;
}

/**
 * Convert the message type string into an integer.
 *
 * @param txt - message type text
 *
 * @return int - message type value
 */
static int
_parsetype(const char *txt)
{
	int i;
	for (i = 0; i < NUM_LOG_TYPES; i++) {
		if (strcmp(txt, _typetxt[i]) == 0)
			return i;
	}
	if (sscanf(txt, "USR%d", &i) == 1)
		return i;
	return -1;
}

/**
 * Render the message and write to a file stream.
 *
 * @param ctx - logging context, or NULL to ignore
 * @param msg - message block to write
 * @param fid - file stream to write to
 *
 * @return int - characters written to the file, or -1 on error
 */
static int
_msgwrite(log_context_t *ctx, msgblk_t *msg, FILE *fid)
{
	int retval = -1;
	char buf[64];
	char txt[16];
	struct tm tm;
	const char *appname = (ctx) ? ctx->appname : "no-app";
	pid_t apppid = (ctx) ? ctx->apppid : getpid();

	localtime_r(&msg->tv.tv_sec, &tm);
	strftime(buf, sizeof(buf), TIME_WRITE_FMT, &tm);
	retval = fprintf(fid, "%s.%06ld %s %d %d %s %s",
			buf, msg->tv.tv_usec, appname, apppid, msg->tid,
			_fmttype(msg->msgtype, txt, sizeof(txt)), msg->message);
	fflush(fid);
	return retval;
}

/**
 * Write a message to the console device.
 *
 * @param ctx - logging context, or NULL to ignore
 * @param msg - message block to write
 *
 * @return int - characters written to the console, or -1 on error
 */
static int
_consolewrite(log_context_t *ctx, msgblk_t *msg)
{
	int retval = -1;
	FILE *fid;
	if ((fid = fopen("/dev/console", "w")) != NULL) {
		retval = _msgwrite(ctx, msg, fid);
		fclose(fid);
	}
	return retval;
}

/**
 * Deliver a formatted log message to stdout. This is used for debugging the log
 * code itself. It should never be used in any other situation, and should be
 * disabled in production, since these library calls should never produce output
 * to stdout.
 *
 * @param fmt - printf-like format specifier
 */
static void
_dbg(const char *fmt, ...)
{
	msgblk_t local;
	va_list args;
	va_start(args, fmt);
	if (_vmsgformat(&local, 0, fmt, args) >= 0) {
		_msgwrite(NULL, &local, stdout);
	} else {
		fprintf(stderr, "Bad format: '%s'\n", fmt);
		fprintf(stderr, "exit(1)\n");
		exit(1);
	}
	va_end(args);
}

/**
 * Parse a formatted message line.
 *
 * This does not modify the original message, and the returned pointer is a
 * position within this message string. Any argument can be passed as NULL (or
 * zero) to not pass back that value.
 *
 * This returns NULL if the record does not parse properly.
 *
 * @param msg - message to parse
 * @param tv - pointer to timeval structure, or NULL to ignore
 * @param appname - pointer to char[16], or NULL to ignore
 * @param pid - pointer to receive PID, or NULL to ignore
 * @param tid - pointer to receive TID, or NULL to ignore
 * @param msgtype - pointer to receive message type value, or NULL to ignore
 *
 * @return char* - pointer to message text, or NULL on error
 */
static char *
_msgparse(char *msg, struct timeval *tv, char *appname,
		pid_t *pid, pid_t *tid, int *msgtype)
{
	char *p = msg;
	char lappname[16];
	char lmsgtxt[16];
	pid_t lpid;
	pid_t ltid;
	struct tm ltm;
	int lusec;
	int i;

	// We want a pointer to the message text, which begins at
	// the sixth space-separated token. So skip the first five tokens
	// (timestamp, appname, pid, tid, msgtype).
	for (i = 0; i < 5; i++) {
		while (*p && *p != ' ')
			p++;
		if (!*p)
			return NULL;
		p++;
	}
	// Parse the header fields
	if (11 != sscanf(msg, TIME_PARSE_FMT " %15s %d %d %15s ",
					&ltm.tm_year,
					&ltm.tm_mon,
					&ltm.tm_mday,
					&ltm.tm_hour,
					&ltm.tm_min,
					&ltm.tm_sec,
					&lusec,
					lappname,
					&lpid,
					&ltid,
					lmsgtxt)) {
		return NULL;
	}
	// Return values
	if (tv) {
		tv->tv_sec = mktime(&ltm);
		tv->tv_usec = lusec;
	}
	if (appname)
		strcpy(appname, lappname);
	if (pid)
		*pid = lpid;
	if (tid)
		*tid = ltid;
	if (msgtype)
		*msgtype = _parsetype(lmsgtxt);
	// Return pointer to the message text
	return p;
}

/**
 * Join with the specified thread to clean up thread resources. This can be
 * safely called with a NULL pointer, in which case it is a no-op.
 *
 * @param thd - thread pointer, or NULL
 */
static inline void
_join_wrthread(GThread *thd)
{
	if (thd) {
		thread_data_t *td = g_thread_join(thd);
		if (td) {
			free(td->file_path);
			free(td);
		}
	}
}

/**
 * Create the write thread data.
 *
 * @param log_path - path to the logging file
 *
 * @return thread_data_t* - pointer to data structure, or NULL on failure
 */
static thread_data_t *
_init_wrthread_data(const char *log_path)
{
	thread_data_t *td;

	if (!(td = calloc(1, sizeof(thread_data_t)))) {
		return NULL;
	}
	if (!(td->file_path = strdup(log_path))) {
		free(td);
		return NULL;
	}
	return td;
}

/**
 * Convert debug and trace levels to a message mask.
 *
 * @param msk - current mask
 * @param D_level - debug level to set, or -1 to ignore
 * @param T_level - trace level to set, or -1 to ignore
 *
 * @return uint64_t
 */
uint64_t
_level_to_mask(uint64_t msk, int D_level, int T_level)
{
	// If D_level and T_level are both zero, turn off everything
	if (D_level == 0 && T_level == 0)
		return 0;
	// Otherwise, turn on everything except debug and trace
	msk |= ~(LOG_MASK_TRACE | LOG_MASK_DEBUG);
	// Clear bits if setting any bits
	if (D_level >= 0)       // if -1, ignore
		msk &= ~LOG_MASK_DEBUG;
	if (T_level >= 0)       // if -1, ignore
		msk &= ~LOG_MASK_TRACE;
	// Add bits according to level
	if (D_level >= 2)
		msk |= (1 << LOG_TYPE_DEBUG2);
	if (D_level >= 1)
		msk |= (1 << LOG_TYPE_DEBUG1);
	if (T_level >= 3)
		msk |= (1 << LOG_TYPE_TRACE3);
	if (T_level >= 2)
		msk |= (1 << LOG_TYPE_TRACE2);
	if (T_level >= 1)
		msk |= (1 << LOG_TYPE_TRACE1);
	return msk;
}

/* --------------------------------------------------------------------
 * The next section contains functions that require that the mutex be locked
 * before calling them.
 */

/**
 * Initialize a single ring.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * The ring structure itself must exist, this merely fills it in and allocates
 * the buffer space.
 *
 * The specified size is rounded up to a multiple of the maximum message size,
 * which is 4K. Because messages are variable-length (a multiple of 8 bytes),
 * there is no particular significance to this exact rounding: however, the ring
 * should be a multiple of 8 bytes.
 *
 * Rings are always initialized in the STATE_IDLE state.
 *
 * @param ring - ring to initialize
 * @param size - size of the ring in bytes
 *
 * @return bool - true if successful, false otherwise
 */
static bool
_mtx_init_ring(ringbuf_t *ring, uint64_t size)
{
	ring->state = STATE_IDLE;
	ring->wridx = 0;
	ring->rdidx = 0;
	ring->size = size;
	ring->buf = malloc(size);
	return (ring->buf != NULL);
}

/**
 * Free a single ring.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * @param ring - ring to free
 */
static void
_mtx_free_ring(ringbuf_t *ring)
{
	if (ring) {
		free(ring->buf);
	}
}

/**
 * Utility function used to measure the amount of empty space in a ring.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * @param ring - pointer to ring to evaluate
 *
 * @return uint32_t - bytes available for a new message
 */
static inline uint32_t
_mtx_ringspace(ringbuf_t *ring)
{
	uint32_t old_wridx = ring->wridx;
	uint32_t old_rdidx = ring->rdidx;
	if (old_wridx < old_rdidx)
		old_wridx += ring->size;
	return ring->size - (old_wridx - old_rdidx);
}

/**
 * Retrieve a message from a ring buffer.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * A NULL pointer can be passed as 'msg', in which case the rdidx pointer will
 * be advanced without a memcpy() operation, effectively discarding one message.
 *
 * @param ring - ring to use
 * @param msg - pointer to a message structure, or NULL to discard
 *
 * @return bool - true if a message is read
 */
static bool
_mtx_getmessage(ringbuf_t *ring, msgblk_t *msg)
{
	bool retval = false;
	if (ring->wridx != ring->rdidx) {
		msgblk_t *msgp = (msgblk_t *)&ring->buf[ring->rdidx];
		uint32_t msglen = msgp->msglen;
		void *msgdst = (void *)msg;

		// If message reaches or crosses end of ring, copy to end of ring
		// This wraps the rdidx pointer to zero
		if (ring->rdidx + msglen >= ring->size) {
			uint32_t len = ring->size - ring->rdidx;
			if (msgdst) {
				memcpy(msgdst, &ring->buf[ring->rdidx], len);
				msgdst += len;
			}
			msglen -= len;
			ring->rdidx = 0;
		}
		// Copy anything left (msglen may be zero)
		if (msgdst)
			memcpy(msgdst, &ring->buf[ring->rdidx], msglen);
		ring->rdidx += msglen;
		retval = true;
	}
	return retval;
}

/**
 * Put a message into a ring buffer.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * This will overwrite old messages in the ring to make space for the new.
 *
 * @param ring - ring to use
 * @param msg - message to write
 */
static void
_mtx_putmessage(ringbuf_t *ring, msgblk_t *msg)
{
	uint32_t msglen = msg->msglen;
	void *msgsrc = (void *)msg;

	// If no ring, this is a no-op
	if (!ring || !msg)
		return;

	// Discard messages until there is space for the new message
	while (_mtx_ringspace(ring) <= msglen) {
		_mtx_getmessage(ring, NULL);
	}

	// If message reaches or crosses end of ring, copy to end of ring
	// This wraps the wridx to zero
	if (ring->wridx + msglen >= ring->size) {
		uint32_t len = ring->size - ring->wridx;
		memcpy(&ring->buf[ring->wridx], msgsrc, len);
		msgsrc += len;
		msglen -= len;
		ring->wridx = 0;
	}
	// Copy anything left (msglen may be zero)
	memcpy(&ring->buf[ring->wridx], msgsrc, msglen);
	ring->wridx += msglen;
}

/**
 * Get the write-through ring.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * This blocks until there is enough space in the write-through ring to
 * accommodate a message of 'msglen' bytes.
 *
 * @param ctx - logging context
 * @param msglen - number of bytes we need to write
 *
 * @return ringbuf_t* - pointer to the write-through ring
 */
static ringbuf_t *
_mtx_getwrthruring(log_context_t *ctx, uint32_t msglen)
{
	// Spin until there is space to put the message
	while (_mtx_ringspace(ctx->wrthruring) <= msglen) {
		DBG("blocking");
		g_cond_wait(&ctx->thrucond, &ctx->mutex);
		DBG("resuming");
	}
	DBG("return wrthru ring");
	return ctx->wrthruring;
}

/**
 * Get the active ring.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * This is a lazy implementation. The system generally does not designate an
 * "active" ring until it actually needs one, at which time it pulls one from
 * the idle pool and makes it active. If the idle pool is empty, this blocks
 * until an idle ring is available in the idle pool.
 *
 * In the special case of no rings, this will return NULL. Callers should be
 * equipped to have this return NULL, in which case there is not, and will never
 * be, an active ring.
 *
 * @param ctx - logging context
 *
 * @return ringbuf_t* - pointer to the (possibly new) active ring, or NULL
 */
static ringbuf_t *
_mtx_getactivering(log_context_t *ctx)
{
	/*
	 * If there are no normal rings, this returns NULL
	 */
	if (ctx->num_rings == 0)
		return NULL;

	/*
	 * Spin until there is an active ring or an idle ring available.
	 */
	while (!ctx->activering && g_queue_is_empty(ctx->idleqp)) {
		DBG("blocking");
		g_cond_wait(&ctx->idlecond, &ctx->mutex);
		DBG("resuming");
	}
	/*
	 * If there is no active ring, there must be an idle ring, or we'd still
	 * be spinning. So activate the first idle ring.
	 */
	if (!ctx->activering) {
		DBG("pop idle ring and activate");
		ctx->activering = g_queue_pop_head(ctx->idleqp);
		ctx->activering->state = STATE_ACTIVE;
	}
	DBG("return active ring");
	return ctx->activering;
}

/**
 * Flush the specified ring.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * @param ctx - logging context
 * @param ring - pointer to the ring to flush (active or wrthru)
 */
static void
_mtx_flushring(log_context_t *ctx, ringbuf_t *ring)
{
	/*
	 * If there is no ring, or no context, this is a no-op.
	 */
	if (!ctx || !ring)
		return;

	/*
	 * If we are flushing the active ring, we first put it into the locked
	 * state, and push it onto the lock queue. This effectively freezes its
	 * current content, large or small. We clobber the active ring, so that
	 * the next thread that tries to log will switch to an idle ring.
	 */
	if (ring->state == STATE_ACTIVE) {
		DBG("push active ring onto lock queue");
		ring->state = STATE_LOCKED;
		g_queue_push_tail(ctx->lockqp, ring);
		ctx->activering = NULL;
	}

	/*
	 * Whether we are flushing the active ring or the write-through ring,
	 * the write thread has some work to do, so wake it up.
	 */
	DBG("signal write thread");
	_bloat();
	g_cond_signal(&ctx->workcond);
}

/**
 * Clear the specified ring without flushing.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * @param ring - pointer to the ring to flush (active or wrthru)
 */
static inline void
_mtx_clearring(ringbuf_t *ring)
{
	if (ring)
		ring->wridx = ring->rdidx = 0;
}

/**
 * Sync with the write thread.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * This kicks the write thread, and waits for it to get back to a blocking
 * condition. This is needed when initializing or terminating the logging.
 *
 * Note that we lose the mutex while waiting, so the thread could be killed
 * while we're blocked.
 *
 * @param ctx - logging context
 */
static void
_mtx_sync(log_context_t *ctx)
{
	if (ctx->wrthread_data) {
		// Set the sync flag
		ctx->wrthread_data->sync = true;
		// Kick the write thread
		_bloat();
		g_cond_signal(&ctx->workcond);
		// Wait for the write thread to clear the sync flag
		while (ctx->wrthread_data && ctx->wrthread_data->sync) {
			g_cond_wait(&ctx->idlecond, &ctx->mutex);
		}
	}
}

/**
 * Determine if the log file is open.
 *
 * This blocks and waits for the write thread (if any) to reach an idle state,
 * then returns true if the log file is open, or false with errno set if the log
 * file is not open.
 *
 * Note that we lose the mutex while waiting, so the thread could be killed
 * while we're blocked.
 *
 * @param ctx - logging context
 *
 * @return bool - true if log file is open, false otherwise
 */
static bool
_mtx_file_open_errcode(log_context_t *ctx)
{
	_mtx_sync(ctx);    // lose mutex momentarily
	if (ctx->wrthread_data == NULL) {
		errno = ECHILD;
		return false;
	}
	if (ctx->wrthread_data->logfd == NULL) {
		errno = ctx->wrthread_data->errcode;
		return false;
	}
	return true;
}

/**
 * Terminate the logging system.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * This will trigger termination of the write thread, if there is one running,
 * but will not join with that thread; it instead returns the terminated thread
 * pointer. The caller must join the defunct thread after releasing the mutex.
 *
 * This can be safely called even if the context is already terminated.
 *
 * @param ctx - logging context
 * @param thdp - returned write thread pointer, or NULL if none running
 * @param force - if true, ignore the reference count and terminate
 */
static void
_mtx_pmlog_term(log_context_t *ctx, GThread **thdp, bool force)
{
	/*
	 * Clear the returned write thread pointer, so that we can take an early
	 * exit.
	 */
	*thdp = NULL;

	/*
	 * Observe the initialization reference count, and only terminate when
	 * this reaches zero. Note that the remainder of the code is safe to
	 * execute if the reference count is already zero.
	 */
	if (!force && ctx->init_count != 0 && --ctx->init_count != 0) {
		return;
	}

	// In case force was specified
	ctx->init_count = 0;

	/*
	 * Clobber the global write thread and data pointers, set the terminate
	 * field to true, signal the write thread. The write thread will detect
	 * the terminate field when we release the mutex, and will stop running.
	 * We return a pointer to the old thread, so that we can join it and
	 * free its resources, OUTSIDE the mutex.
	 *
	 * Note that the write thread is guaranteed to not access any global
	 * resources once the td->terminate flag is set, so it is safe to go
	 * ahead and delete these.
	 */

	// Stop the write thread
	if (ctx->wrthread_data) {
		DBG("stop thread");
		*thdp = ctx->wrthread_data->self;           // return thread pointer
		ctx->wrthread_data->terminate = true;       // tell thread to stop
		ctx->wrthread_data = NULL;                  // don't need this any more
		g_cond_signal(&ctx->workcond);              // signal the write thread

		// write thread blocked until we release the mutex
	}

	// Free the queues -- g_queue_free() is not NULL-safe
	if (ctx->lockqp) {
		DBG("free lockqp");
		g_queue_free(ctx->lockqp);
		ctx->lockqp = NULL;
	}
	if (ctx->idleqp) {
		DBG("free idleqp");
		g_queue_free(ctx->idleqp);
		ctx->idleqp = NULL;
	}

	// Free the ring memory
	if (ctx->rings) {
		int i;
		for (i = 0; i < ctx->num_rings + 1; i++) {
			DBG("free ring %d", i);
			_mtx_free_ring(&ctx->rings[i]);
		}
		DBG("free rings");
		free(ctx->rings);
	}
	ctx->num_rings = 0;
	ctx->rings = NULL;

	// Miscellaneous cleanup
	ctx->activering = NULL;
	ctx->wrthruring = NULL;
}

/**
 * Handle parent process fork
 *
 * The following three functions handle a fork by the parent process.
 * These are necessary so that we know the state of the various
 * mutexes before and after the fork.
 *
 * The pre-fork function gets the global mutex and then walks the
 * context list and gets each context mutex and flushes the log. This
 * puts all of the mutexes in an acquired state before the fork.
 *
 * The post-fork-parent function runs in the parent after a fork. All
 * it needs to do is release the mutexes acquired pre-fork.
 *
 * The post-fork-child function runs in the child after a fork. It
 * needs to destroy any writer thread state and release the mutexes.
 * Writer threads are not forked and remain with the parent.
 *
 * These functions are set up using the pthread_atfork function. There
 * is no corresponding glib function that does the same thing.
 * Luckily, with Linux glib uses pthreads underneath so this isn't a
 * problem.
 */
static void
_pmlog_pre_fork(void)
{
	GList *item;

	g_mutex_lock(&_global_mutex);

	for (item = _context_list; item; item = item->next) {
		log_context_t *ctx = item->data;

		g_mutex_lock(&ctx->mutex);
		_mtx_sync(ctx);
	}
}

static void
_pmlog_post_fork_parent(void)
{
	GList *item;

	for (item = _context_list; item; item = item->next) {
		log_context_t *ctx = item->data;

		g_mutex_unlock(&ctx->mutex);
	}

	g_mutex_unlock(&_global_mutex);
}

static void
_pmlog_post_fork_child(void)
{
	GList *item;

	for (item = _context_list; item; item = item->next) {
		log_context_t *ctx = item->data;
		thread_data_t *td = ctx->wrthread_data;

		if (td) {
			if (td->logfd) {
				fclose(td->logfd);
			}
			td->logfd = NULL;

			/* Create a new write thread. */
			/* Threads do not survive a fork. */
			td->self = g_thread_try_new("pmlog write thread",
					_pmlog_write_thread, td, NULL);
			if (td->self) {
				_mtx_sync(ctx);
			}
		}

		g_mutex_unlock(&ctx->mutex);
	}

	g_mutex_unlock(&_global_mutex);
}

/**
 * Initialize the logging system.
 *
 * SINGLE-THREADED, mutex must be held
 *
 * This can fail on bad parameters, OOM conditions, and failure to start the
 * write thread.
 *
 * If (*thdp) is returned as a non-NULL thread pointer, then the caller needs to
 * join this thread after releasing the mutex.
 *
 * @param ctx - logging context
 * @param log_path - log file path, or NULL for default
 * @param max_size - maximum file size, 0 for default, -1 for unlimited growth
 * @param max_files - maximum file count, 0 for default, -1 to disable rotation
 * @param num_rings - number of ring buffers, 0 for default, -1 to disable
 * @param ring_size - log file ring buffer size in bytes, or 0 for default
 * @param thdp - returned write thread pointer to clean up
 *
 * @return bool - true on success, false on error
 */
static bool
_mtx_pmlog_init(log_context_t *ctx, const char *log_path,
		int64_t max_size, int64_t max_files,
		int64_t num_rings, int64_t ring_size,
		GThread **thdp)
{
	bool retval = false;
	thread_data_t *td;
	FILE *fid;
	uint64_t i;
	int D_level = 0;
	int T_level = 0;
	int errcode = 0;

	// For early error exits
	*thdp = NULL;

	// Override zero parameters with environment parameters
	if (log_path == NULL)
		log_path = getenv("PMLOG_FILE_PATH");
	if (max_size == 0)
		max_size = getenvzero("PMLOG_MAX_FILE_SIZE");
	if (max_files == 0)
		max_files = getenvzero("PMLOG_MAX_FILE_COUNT");
	if (num_rings == 0)
		num_rings = getenvzero("PMLOG_NUM_RINGS");
	if (ring_size == 0)
		ring_size = getenvzero("PMLOG_RING_SIZE");
	D_level = getenvzero("PMLOG_DEBUG_LEVEL");
	T_level = getenvzero("PMLOG_TRACE_LEVEL");

	// Still zero means use the hardcoded default values
	if (log_path == NULL || *log_path == 0)
		log_path = LOG_FILE_PATH_DFL;
	if (max_size == 0)
		max_size = LOG_FILE_SIZE_DFL;
	if (max_files == 0)
		max_files = LOG_FILE_COUNT_DFL;
	if (num_rings == 0)
		num_rings = LOG_NUM_RINGS_DFL;
	if (ring_size == 0)
		ring_size = LOG_RING_SIZE_DFL;

	// Negative values mean disable entirely
	if (max_size < 0)
		max_size = 0;           // log grows without limit
	if (max_files < 0)
		max_files = 1;          // rotation forbidden
	if (num_rings < 0)
		num_rings = 0;          // only messages allowed
	if (ring_size < 4096)
		ring_size = 4096;       // at least one message

	// If we've already been initialized, we're good
	DBG("Init count = %ld", ctx->init_count);
	if (ctx->init_count++ != 0) {
		DBG("Early exit");
		retval = true;
		goto done;
	}
	DBG("Initializing");
	DBG("max_size = %ld", max_size);
	DBG("max_files = %ld", max_files);
	DBG("num_rings = %ld", num_rings);
	DBG("ring_size = %ld", ring_size);

	// Round up on sizes
	max_size = (max_size + 1023) & ~1023;   // multiple of 1024
	ring_size = (ring_size + 7) & ~7;       // multiple of 8

	// Idle and locked queues
	ctx->lockqp = g_queue_new();
	ctx->idleqp = g_queue_new();
	if (!ctx->lockqp || !ctx->idleqp) {
		errcode = ENOMEM;
		goto done;
	}

	// Create the ring space
	ctx->num_rings = num_rings;
	ctx->rings = malloc((ctx->num_rings + 1) * sizeof(ringbuf_t));
	if (!ctx->rings) {
		errcode = errno;
		DBG("no ctx->rings: %d, %s", errcode, strerror(errcode));
		goto done;
	}
	for (i = 0; i < ctx->num_rings; i++) {
		if (!_mtx_init_ring(&ctx->rings[i], ring_size)) {
			errcode = errno;
			DBG("no ring[%ld]: %d, %s", i, errcode, strerror(errcode));
			goto done;
		}
		g_queue_push_tail(ctx->idleqp, &ctx->rings[i]);
	}
	// Plus the write-through ring
	if (!_mtx_init_ring(&ctx->rings[ctx->num_rings], WRTHRU_SIZE)) {
		errcode = errno;
		DBG("no ring[%ld]: %d, %s", ctx->num_rings, errcode, strerror(errcode));
		goto done;
	}
	ctx->wrthruring = &ctx->rings[ctx->num_rings];
	ctx->wrthruring->state = STATE_WRTHRU;

	// We want to put the app name into the log entries
	memset(ctx->appname, 0, sizeof(ctx->appname));
	if ((fid = fopen("/proc/self/comm", "r"))) {
		if (fgets(ctx->appname, sizeof(ctx->appname), fid)) {
			char *p = strchr(ctx->appname, '\n');
			if (p)
				*p = 0;
		}
		fclose(fid);
	}
	if (ctx->appname[0] == 0) {
		snprintf(ctx->appname, sizeof(ctx->appname), "(unknown)");
	}

	// We cache the current PID as well
	ctx->apppid = getpid();

	// Set the initial debug mask for the context
	ctx->stderr_mask = _level_to_mask(0, D_level, T_level);

	// Set up the write parameters for the write thread
	td = _init_wrthread_data(log_path);
	if (!td) {
		errcode = errno;
		DBG("no td: %d, %s", errcode, strerror(errcode));
		goto done;
	}
	td->ctx = ctx;
	td->max_size = max_size;
	td->max_files = max_files;
	td->logwrt_enable = true;

	// Start the write thread
	DBG("Start write thread");
	td->self = g_thread_try_new("pmlog write thread", _pmlog_write_thread, td, NULL);
	if (!td->self) {
		// Total failure to start, clean up td
		errcode = ECHILD;
		DBG("no thread: %d, %s", errcode, strerror(errcode));
		free(td->file_path);
		free(td);
		goto done;
	}

	// We're running the thread, globalize the data pointer
	ctx->wrthread_data = td;

	// Success
	retval = true;

done:
	// Clean up after failure
	if (!retval) {
		/*
		 * If there was an error, we need to undo everything we've done
		 * above, to return the system to a clean state.
		 */
		_mtx_pmlog_term(ctx, thdp, false);
		// Set errno for return
		DBG("return errno = %d, %m", errno);
		errno = errcode;
	}

	return retval;
}

/* --------------------------------------------------------------------
 * The following section contains thread functions. These may or may not have
 * the mutex locked when called, but they are all thread-safe because they are
 * called by only one thread, the write thread.
 */

/**
 * Write a single message to log file and/or stderr.
 *
 * SINGLE-THREADED (write thread), mutex NOT held
 *
 * @param td - thread data pointer
 * @param msg - message to write
 */
static void
_thr_write_message(thread_data_t *td, msgblk_t *msg)
{
	log_context_t *ctx = td->ctx;

	// Write to the log file
	if (td->logwrt_enable && td->logfd != NULL) {
		if (_msgwrite(ctx, msg, td->logfd) == -1)
			td->write_errs++;
	}
	// Accumulate bytes written
	td->written += msg->msglen;
}

/**
 * Format and write directly into the log file, bypassing rings.
 *
 * SINGLE-THREADED (write thread), mutex NOT held
 *
 * This is intended for messages coming from the write thread itself. We don't
 * want to feed it through the rings, because this may be reporting conditions
 * that pertain to the logging system itself not working properly, or doing
 * something that will affect the logs, like rotation.
 *
 * This does NOT automatically append a line-feed to the message.
 *
 * @param td - thread data pointer
 * @param fmt - printf-like format descriptor
 */
static void __attribute__((__format__(__printf__, 3, 4)))
_thr_msg(thread_data_t *td, int type, const char *fmt, ...)
{
	msgblk_t local;
	va_list args;
	va_start(args, fmt);
	if (_vmsgformat(&local, type, fmt, args) >= 0) {
		_thr_write_message(td, &local);
	} else {
		fprintf(stderr, "Bad format: '%s'\n", fmt);
		fprintf(stderr, "exit(1)");
		exit(1);
	}
	va_end(args);
}

/*
 * The macros DO append a line-feed. Use them. This is a minimal set.
 */
#define	_thr_log(td,typ,fmt,args...)	_thr_msg(td, typ, "[%s:%d] " fmt "\n", __func__, __LINE__, ##args)
#define _thr_internal(td,fmt,args...)   _thr_log(td, LOG_TYPE_INTERNAL, fmt, ##args)
#define _thr_warning(td,fmt,args...)    _thr_log(td, LOG_TYPE_WARNING, fmt, ##args)

/**
 * Close the control file.
 *
 * SINGLE-THREADED (write thread), mutex NOT held
 *
 * @param td - thread data
 */
static void
_thr_close_ctl_file(thread_data_t *td)
{
	if (td->ctlfd) {
		fclose(td->ctlfd);
		td->ctlfd = NULL;
	}
}

/**
 * Open the control file.
 *
 * SINGLE-THREADED (write thread), mutex NOT held
 *
 * The control file serves as an interprocess mutex, and a means of sharing log
 * rotation parameters among multiple concurrent applications that share the
 * same log file path.
 *
 * This must be opened before opening the log file, and closed afterwards.
 *
 * @param td - thread data
 *
 * @return bool - true if file is opened, false otherwise
 */
static bool
_thr_open_ctl_file(thread_data_t *td)
{
	bool retval = false;
	char path[PATH_MAX];
	struct flock lk;
	int fn;

	snprintf(path, sizeof(path), "%s.ctl", td->file_path);
	fn = open(path, O_CREAT | O_RDWR, 0666);
	if (fn == -1) {
		td->errcode = errno;
		goto done;
	}
	memset(&lk, 0, sizeof(lk));
	lk.l_type = F_WRLCK;
	lk.l_whence = SEEK_SET;
	lk.l_start = 0;
	lk.l_len = 0;
	if (fcntl(fn, F_SETLKW, &lk) == -1) {
		td->errcode = errno;
		close(fn);
		goto done;
	}
	td->ctlfd = fdopen(fn, "r+");
	if (!td->ctlfd) {
		td->errcode = errno;
		close(fn);
		goto done;
	}
	retval = true;

done:
	return retval;
}

/**
 * Close the log file.
 *
 * SINGLE-THREADED (write thread), mutex NOT held
 *
 * @param td - thread data
 */
static void
_thr_close_log_file(thread_data_t *td)
{
	if (td->logfd != NULL) {
		fclose(td->logfd);
		td->logfd = NULL;
	}
}

/**
 * Open the log file for writing.
 *
 * SINGLE-THREADED (write thread), mutex NOT held
 *
 * The control file must be opened before attempting this, and should be closed
 * after this is done. The control file lock serves as an interprocess mutex.
 *
 * The log file remains open until logging or application terminates, with a
 * shared POSIX file lock.
 *
 * @param td - thread data
 * @param rotating - true if rotating files, false otherwise
 *
 * @return bool - true if this is the master writer, false otherwise
 */
static bool
_thr_open_log_file(thread_data_t *td, bool rotating)
{
	bool retval = false;
	struct flock lk;
	struct stat sb;
	int logfn;

	// Close the current log file if it is open
	_thr_close_log_file(td);

	// Open the log file
	logfn = open(td->file_path, O_CREAT | O_RDWR | O_APPEND, 0666);
	if (logfn == -1) {
		td->errcode = errno;
		goto done;
	}
	if (fstat(logfn, &sb) == -1) {
		// This should NEVER happen
		td->errcode = errno;
		close(logfn);
		goto done;
	}
	td->logino = sb.st_ino;
	td->logfd = fdopen(logfn, "a");
	if (td->logfd == NULL) {
		// This should NEVER happen
		td->errcode = errno;
		close(logfn);
		goto done;
	}

	/*
	 * If we reopen the log file during rotation, we don't need to do any of
	 * the parameter checks below.
	 */
	if (rotating) {
		goto done;
	}

	/*
	 * This requires some explanation. The parameters are used to rotate the
	 * logs. We have distributed the log rotation, meaning that logs will
	 * rotate whether there is one process or many. However, if the
	 * applications are sharing the same log file, but rotating it at
	 * different rates, we get into chaos. So we want to make sure that all
	 * concurrently-running processes use the same log rotation parameters.
	 *
	 * At this point, the control file has been opened with an exclusive
	 * lock, so all other processes trying to run this code are blocked.
	 *
	 * This makes an attempt to gain a non-blocking exclusive lock on the
	 * log file, which we're going to relinguish in a moment. If we get that
	 * lock, we are the "first" of potentially many log users, and so we get
	 * to write OUR parameters to the control file.
	 *
	 * If we fail to get that exclusive lock, then there are other processes
	 * out there using this log file, and one of them was first, and wrote
	 * its parameters to the file. We downgrade to a shared lock, and hold
	 * that lock for the duration of the logging. We then read back the
	 * rotation parameters. Note that these will be the same values we just
	 * wrote, if we wrote them.
	 *
	 * Note that we are using fcntl POSIX locks, exploiting the atomic
	 * transition from exclusive to shared lock. This should work with NFS
	 * files. Downside of the Posix locks are that they aren't thread-sane
	 * (the first thread to unlock or close releases the lock), which
	 * doesn't concern us, because there is only one thread doing this, the
	 * write thread.
	 *
	 * There are no reasons for us to block on getting the shared lock,
	 * since we are protected by an exclusive lock on the control file,
	 * which should prevent any other application from being in this code,
	 * and the only failure conditions involve system errors. If we do fail
	 * to get the shared lock, we ignore the shared parameters and use our
	 * own.
	 */
	memset(&lk, 0, sizeof(lk));
	lk.l_type = F_WRLCK;
	lk.l_whence = SEEK_SET;
	lk.l_start = 0;
	lk.l_len = 0;
	// We expect this next to fail for all but the first process
	if (fcntl(logfn, F_SETLK, &lk) == 0) {
		// Rewrite the control file
		if (td->ctlfd && ftruncate(fileno(td->ctlfd), 0) == 0) {
			rewind(td->ctlfd);
			fprintf(td->ctlfd, "%ld,%ld\n", td->max_size, td->max_files);
			fflush(td->ctlfd);
			retval = true;
		}
	}
	// We expect this to succeed
	lk.l_type = F_RDLCK;
	if (fcntl(logfn, F_SETLKW, &lk) == 0) {
		if (td->ctlfd) {
			uint64_t siz, cnt;
			rewind(td->ctlfd);
			if (fscanf(td->ctlfd, "%ld,%ld\n", &siz, &cnt) == 2) {
				if (td->max_size != siz || td->max_files != cnt)
					_thr_internal(td, "Use max_size=%ld, max_files=%ld",
							td->max_size, td->max_files);
				td->max_size = siz;
				td->max_files = cnt;
			} else {
				_thr_warning(td, "Control file is corrupt\n");
				_thr_warning(td, "Use max_size=%ld, max_files=%ld\n",
						td->max_size, td->max_files);
			}
		} else {
			_thr_warning(td, "Control file is not open\n");
			_thr_warning(td, "Use max_size=%ld, max_files=%ld\n",
					td->max_size, td->max_files);
		}
	} else {
		_thr_warning(td, "Control file shared lock failed: %m\n");
		_thr_warning(td, "Use max_size=%ld, max_files=%ld\n",
				td->max_size, td->max_files);
	}

done:
	return retval;
}

/**
 * Purge excess log files.
 *
 * This starts at archive td->max_files, and keeps incrementing the number until
 * the unlink() fails for any reason, typically because we ran out of log files
 * to delete.
 *
 * @param td - thread data pointer
 */
static void
_thr_purge_logs(thread_data_t *td)
{
	char src[PATH_MAX];
	int i;

	// All rotation is prohibited
	if (td->max_files <= 1)
		return;
	for (i = td->max_files; true; i++) {
		pmlog_path(src, sizeof(src), td->file_path, i);
		if (unlink(src) == -1)
			break;
		_thr_internal(td, "Purged log file %s", src);
	}
}

/**
 * Rotate the log files if forced, or necessary.
 *
 * SINGLE-THREADED (write thread), mutex NOT held
 *
 * @param td - thread data
 * @param force - true to force an immediate log rotation
 */
static void
_thr_rotate_logs(thread_data_t *td, bool force)
{
	char src[PATH_MAX];
	char dst[PATH_MAX];
	int i;

	if (td->max_files <= 1) {
		/*
		 * Only allowed to have one file, do not allow rotation.
		 */
		DBG("Rotation prohibited");
		goto done;
	} else if (force) {
		/*
		 * Forced request takes priority.
		 *
		 * Acquire an exclusive lock on the control file. This is to
		 * protect our rotation from other processes that may be trying
		 * to do the same thing. Only one process will get the exclusive
		 * lock, the others will block here and will have to wait their
		 * turn. An outright failure to open the control file will
		 * prevent log rotation.
		 *
		 * Note that if two processes or threads force rotation, the
		 * rotation will occur twice in succession.
		 */
		if (!_thr_open_ctl_file(td)) {
			DBG("Failed to open control file");
			goto done;
		}

		_thr_internal(td, "Rotating log files (forced)");
	} else if (td->max_size == 0) {
		/*
		 * Automatic rotation prohibited.
		 *
		 * All processes share the same parameters, so if we're not
		 * supposed to rotate, NO ONE should be rotating.
		 */
		DBG("Infinite size, never rotate");
		goto done;
	} else if (td->written < 1024) {
		/*
		 * The lseek() below isn't terribly expensive, but it is
		 * measurable, especially on many short messages. So we
		 * accumulate bytes written and defer checking the file size
		 * until we've written at least 1K. If there are N applications
		 * running, the file may grow to up to N*1024 bytes bigger than
		 * the maximum file size before rotating.
		 */
		goto done;
	} else {
		/*
		 * Normal rotation.
		 */
		struct stat sb;
		off_t pos;

		/*
		 * Zero the write count, to reset the delay before the next
		 * lseek().
		 */
		td->written = 0;

		/*
		 * Three-stage test to see if we should rotate logs.
		 *
		 * Stage one, see if we're on the lean side of the max_size
		 * value. If so, don't worry about it.
		 */
		if ((pos = lseek(fileno(td->logfd), 0, SEEK_END)) < td->max_size) {
			goto done;
		}
		DBG("pos = %ld >= %ld, checking inodes", pos, td->max_size);

		/*
		 * Stage two, do stat() to check the inode. If it changed, then
		 * someone else rotated the logs, and we're logging to the wrong
		 * file. This is very fast, compared to stage three.
		 *
		 * Note that the stat() could fail, if (for instance) the
		 * rotating process has just moved the old file, but has not yet
		 * opened the new. Or someone may have deleted the log file.
		 * Either way, we don't trust this test, and instead pass to
		 * stage three.
		 */
		if (stat(td->file_path, &sb) == 0 && td->logino != sb.st_ino) {
			DBG("rotation started or done");
			_thr_open_log_file(td, true);
			goto done;
		}
		DBG("Contend to rotate");

		/*
		 * Stage three, contend to be the rotating process. This starts
		 * with a blocking exclusive open of the control file, which
		 * serves as a process-level mutex. Once we return from this, we
		 * are guaranteed that no other process can proceed until we
		 * finish the rotation.
		 */
		if (!_thr_open_ctl_file(td)) {
			DBG("Failed to open control file");
			goto done;
		}

		/*
		 * Check inodes again. We just checked this, but without any
		 * lock on the control file, which means another process could
		 * have rotated between the check and the control file open
		 * above. Odds are good that we still have current file, and if
		 * we do, we know that no one else is going to try to rotate.
		 */
		if (stat(td->file_path, &sb) == 0 && td->logino != sb.st_ino) {
			DBG("rotation started or done");
			_thr_open_log_file(td, true);
			goto done;
		}

		_thr_internal(td, "Rotating log files");
	}

	/*
	 * We won the rotation contest. Do the rotation.
	 */

	// Assume td->max_files == 5 as an example
	// Discard file.4
	pmlog_path(src, sizeof(src), td->file_path, td->max_files - 1);
	if (unlink(src) == 0) {
		// Successful, continue
		DBG("Unlinked %s", src);
	} else if (errno == ENOENT) {
		// No file, continue
		DBG("Unlink %s, does not exist", src);
	} else {
		/*
		 * There was an error trying to unlink file.4. The most
		 * likely issue is that we don't have write permission
		 * to the directory, which is going to make rotation
		 * impossible for this process. So we stop trying. Some
		 * other process will have to do this.
		 */
		_thr_warning(td, "unlink(%s) failed: %m, rotate halted\n", src);
		goto done;
	}
	// Move file.3 -> file.4, file.2 -> file.3, file.1 -> file.2, file -> file.1
	for (i = td->max_files - 2; i >= 0; i--) {
		strcpy(dst, src);
		pmlog_path(src, sizeof(src), td->file_path, i);
		if (rename(src, dst) == 0) {
			// Successful, continue
			DBG("Renamed %s -> %s", src, dst);
		} else if (errno == ENOENT) {
			// Failed because src doesn't (yet) exist, continue
			DBG("File %s doesn't yet exist", src);
		} else {
			/*
			 * Some problem moving this file, perhaps (again) a
			 * permission error. We give up.
			 */
			_thr_warning(td, "rename(%s,%s) failed: %m, rotate halted\n",
					src, dst);
			goto done;
		}
	}

	/*
	 * The original file no longer exists as a dentry, so when we reopen it,
	 * we create a new (empty) file. Note that no other applications have
	 * any idea we have done this rotation: they continue to write to the
	 * same file (inode) they've been writing to, potentially including the
	 * file that we've unlinked. They will eventually hit the rotation
	 * condition themselves, see that the inode changed, and reopen the
	 * active log file.
	 */
	_thr_open_log_file(td, true);

	/*
	 * Record the rotation count, for white-box testing.
	 */
	td->rotations++;
done:

	/*
	 * Close the control file, if it was opened, releasing the file lock.
	 */
	_thr_close_ctl_file(td);
}

/**
 * Write thread, started by initialization, runs until terminated.
 *
 * This thread has a couple of rules. The most important is that it is NOT
 * ALLOWED to self-terminate. It has to wait for a user-threads to detect
 * problems, and terminate this thread by setting td->terminate to true.
 *
 * @param data - pointer to an initialized thread_data_t structure
 *
 * @return void* - same as the data pointer
 */
static void *
_pmlog_write_thread(void *data)
{
	thread_data_t *td = (thread_data_t *)data;
	log_context_t *ctx = td->ctx;
	ringbuf_t *ring;
	bool didwork;
	msgblk_t local;
	sigset_t sigset;

	DBG("starting, data = %p", data);

	// Block all signals in this thread, defer until this thread blocks
	sigfillset(&sigset);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

	// Open (exclusive lock) the control file
	DBG("log file opening...");
	if (_thr_open_ctl_file(td)) {
		// Open the log file
		if (_thr_open_log_file(td, false)) {
			// Purge excess log files
			_thr_purge_logs(td);
		}
		// Close (release lock) the control file
		_thr_close_ctl_file(td);
	}
	DBG("log file opened");

	// Thread unlocks the mutex at certain points
	DBG("locking...");
	g_mutex_lock(&ctx->mutex); _bloat();
	DBG("locked");

	// Pretend we did some work, so that we'll look for more before blocking
	didwork = true;
	/*
	 * We stop this thread by setting td->terminate to true in a user
	 * thread. Note that all inner loops check this value on every
	 * iteration, so the thread will stop immediately after completing any
	 * operation already in progress. It will never reference a global value
	 * if this flag is set.
	 */
	while (!td->terminate) {

		// This will not pause until it gets through a no-work pass
		if (!didwork) {
			// Clear the sync flag and signal we are idle
			td->sync = false;
			_bloat();
			g_cond_broadcast(&ctx->idlecond);

			// Block until signalled
			DBG("blocking");
			g_cond_wait(&ctx->workcond, &ctx->mutex);
			DBG("resuming");
		}

		// Assume this will be a no-work pass
		didwork = false;

		// Rotate logs if requested
		if (td->rotate) {
			DBG("forced rotation");
			g_mutex_unlock(&ctx->mutex);
			_thr_rotate_logs(td, true);
			g_mutex_lock(&ctx->mutex); _bloat();
			td->rotate = false;
			didwork = true;
		}

		// Pop locked rings off the lock queue and process them.
		while (!td->terminate &&
				(ring = g_queue_pop_head(ctx->lockqp))) {
			// Pull messages off the ring and display them
			DBG("popped locked ring");
			while (!td->terminate &&
					_mtx_getmessage(ring, &local)) {
				DBG("write message len == %d", local.txtlen);
				g_mutex_unlock(&ctx->mutex);
				// Check size and rotate as necessary
				_thr_rotate_logs(td, false);
				_thr_write_message(td, &local);
				g_mutex_lock(&ctx->mutex); _bloat();
			}

			/*
			 * We are done with this ring, put it in the idle state
			 * and wake up all threads that might be blocked,
			 * waiting for an active ring.
			 */
			DBG("push ring onto idle queue");
			ring->state = STATE_IDLE;
			g_queue_push_tail(ctx->idleqp, ring);
			didwork = true;
		}

		/*
		 * Pull individual messages off the write-through ring, and
		 * write them out. As soon as this frees some space in the ring,
		 * threads can be unblocked to (potentially) write more.
		 */
		while (!td->terminate &&
				_mtx_getmessage(ctx->wrthruring, &local)) {
			DBG("write wrthru message len == %d", local.txtlen);
			/*
			 * Threads can now compete to use the space we just
			 * freed in the ring. We broadcast rather than signal,
			 * because we could have freed a large message, and
			 * there could be many threads waiting to write small
			 * messages, all of which could run, now.
			 */
			_bloat();
			g_cond_broadcast(&ctx->thrucond);

			/*
			 * We don't want the mutex locked for this.
			 */
			g_mutex_unlock(&ctx->mutex);
			// Check size and rotate as necessary
			_thr_rotate_logs(td, false);
			_thr_write_message(td, &local);
			g_mutex_lock(&ctx->mutex); _bloat();
			didwork = true;
		}
	}
	// Signal write thread has terminated
	td->sync = false;
	_bloat();
	g_cond_broadcast(&ctx->idlecond);

	DBG("unlocking");
	g_mutex_unlock(&ctx->mutex);

	// Close the log file outside the critical region
	_thr_close_log_file(td);

	DBG("terminating, return %p", data);
	return data;
}

/* -------------------------------------------------------------------
 * Logging API (exported functions).
 */

/**
 * Utility function to generate a log file path name.
 *
 * When N == 0, this simply copies the supplied path into the target buffer.
 *
 * When N > 0, this appends ".N" to the supplied path.
 *
 * @param buf - target buffer
 * @param siz - size of target buffer in bytes
 * @param pth - base path name for the log file
 * @param N - historical archive number, or 0
 *
 * @return char* - returns the supplied buf pointer
 */
char *
pmlog_path(char *buf, size_t siz, const char *pth, int N)
{
	if (N == 0) {
		snprintf(buf, siz, "%s", pth);
	} else {
		snprintf(buf, siz, "%s.%d", pth, N);
	}
	return buf;
}

/**
 * Exported copy of _msgparse(). See _msgparse().
 *
 * @param msg - message to parse
 * @param tv - pointer to timeval structure, or NULL to ignore
 * @param appname - pointer to char[16], or NULL to ignore
 * @param pid - pointer to receive PID, or NULL to ignore
 * @param tid - pointer to receive TID, or NULL to ignore
 * @param msgtype - pointer to receive message type value, or NULL to ignore
 *
 * @return char* - pointer to message beyond the line header fields
 */
char *
pmlog_parse(char *msg, struct timeval *tv, char *appname,
		pid_t *pid, pid_t *tid, int *msgtype)
{
	return _msgparse(msg, tv, appname, pid, tid, msgtype);
}

/**
 * Log a message.
 *
 * This will initialize the logging system with default values if it has not
 * already been initialized.
 *
 * This can fail if trying to initialize the system, or if the write thread has
 * somehow died due to a fatal file system error (e.g. unable to open a log
 * file after rotation).
 *
 * The errno value is set on error return, with the following values:
 *
 * - ENOBUFS - logging is disabled
 * - ENOMEM  - memory allocation failed
 * - ECHILD  - write thread insufficient resources
 * - other   - any file system error associated with opening the log file
 *
 * @param ctxp - logging context, or NULL for default
 * @param msg_type - LOG_TYPE_* message type
 * @param fmt - printf-style format descriptor
 *
 * @return int - 0 if successful, -1 on failure
 */
int
pmlog_message_ctx(void *ctxp, int msg_type, const char *fmt, ...)
{
	int retval = -1;
	va_list args;
	ringbuf_t *ring;
	msgblk_t local;
	ssize_t len;
	int errcode;
	GThread *thd = NULL;
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();
	if (!(ctx = _get_context(ctxp))) {
		errno = ENOBUFS;
		return retval;
	}

	// If not full logging, reject high-verbosity messages
	if (_log_enable != LOG_ENABLE_FULL &&
			(msg_type == LOG_TYPE_TRACE3 ||
					msg_type == LOG_TYPE_TRACE2 ||
					msg_type == LOG_TYPE_DEBUG2)) {
		return 0;
	}

	/*
	 * Format the message. This function returns the actual number of bytes
	 * in the message, even if truncated to fit. This count does NOT include
	 * the final NUL character.
	 */
	va_start(args, fmt);
	len = _vmsgformat(&local, msg_type, fmt, args);
	va_end(args);
	if (len < 0) {
		errcode = errno;
		goto done;
	}
	/*
	 * Add the msgblk_t header information length (16 bytes), plus 1 byte
	 * for the terminating NUL, then pad the message as necessary to make
	 * this a multiple of 8.
	 */
	local.msglen = 1 + local.txtlen + MSG_HDR_SIZE;
	while ((local.msglen & 0x7)) {
		local.message[local.msglen++] = 0;
	}

	/*
	 * Record the message.
	 */
	g_mutex_lock(&ctx->mutex); _bloat();

	/*
	 * Write directly to stderr, blocking until complete. This is where you
	 * are going to trace operations while debugging the library, which
	 * could have something go wrong that prevents the write thread from
	 * getting the messages out. Do a direct write and ensure the stream
	 * buffer is flushed.
	 */
	if (((1 << local.msgtype) & ctx->stderr_mask) && stderr != NULL) {
		_msgwrite(ctx, &local, stderr);
		fflush(stderr);
		// drop through
	}

	/*
	 * Console messages get written directly to console. Same rules as
	 * above, except that the flush is handled when the device file is
	 * closed.
	 */
	if (msg_type == LOG_TYPE_CONSOLE) {
		_consolewrite(ctx, &local);
		retval = 0;
		goto done;
	}

	/*
	 * All other messages get routed through the write thread, which means
	 * that the write thread must be up and running. If the caller did not
	 * explicitly initialize the logging system, we initialize it now with
	 * defaults.
	 */
	if (ctx->wrthread_data == NULL) {
		// Initialize the logging system with defaults
		if (!_mtx_pmlog_init(ctx, NULL, 0, 0, 0, 0, &thd)) {
			errcode = errno;
			goto done;
		}
		// Open the log file
		if (!_mtx_file_open_errcode(ctx)) {
			errcode = errno;
			_mtx_pmlog_term(ctx, &thd, false);
			goto done;
		}
	}

	/*
	 * If we are autoflushing, push all messages (every kind) into the
	 * write-through ring. This does flush in the background.
	 */
	if (ctx->wrthread_data->autoflush_enable) {
		// autoflush always passes through the writethru ring
		ring = _mtx_getwrthruring(ctx, local.msglen);
		_mtx_putmessage(ring, &local);
		_mtx_flushring(ctx, ring);
		retval = 0;
		goto done;
	}

	switch (msg_type) {
	case LOG_TYPE_CONSOLE:
		// already handled above
		break;
	case LOG_TYPE_INTERNAL:
	case LOG_TYPE_MESSAGE:
		/*
		 * Write to the write-through ring. Wait for the ring to have
		 * space in it, then put the message, then immediately flush.
		 * Because this is the write-through ring, flushing does not
		 * lock the ring, so it continues to be available until it is
		 * full.
		 */
		ring = _mtx_getwrthruring(ctx, local.msglen);
		_mtx_putmessage(ring, &local);
		_mtx_flushring(ctx, ring);
		break;
	case LOG_TYPE_CRITICAL:
	case LOG_TYPE_WARNING:
	case LOG_TYPE_FAULT:
		/*
		 * These need to go out to the console, though without the
		 * flushed data from the ring.
		 */
		_consolewrite(ctx, &local);
		/*
		 * Write to the active ring and flush. Wait for there to be an
		 * active ring (activating an idle ring if necessary), then put
		 * the message (possibly overwriting old messages in the ring),
		 * then lock/flush the ring, invalidating the active ring. The
		 * next write will have to allocate a new active ring, or block
		 * until one becomes available.
		 */
		ring = _mtx_getactivering(ctx);
		_mtx_putmessage(ring, &local);
		_mtx_flushring(ctx, ring);
		break;
	default:
		/*
		 * Put this in the ring, only, and don't write to console.
		 */
		ring = _mtx_getactivering(ctx);
		_mtx_putmessage(ring, &local);
		break;
	}
	retval = 0;
done:
	g_mutex_unlock(&ctx->mutex);
	_join_wrthread(thd);
	if (retval != 0) {
		errno = errcode;
	}
	return retval;
}

/**
 * Enable or disable the autoflush feature, and return the previous value.
 *
 * @param ctxp - logging context, or NULL for default
 * @param enable - true to enable autoflush, false (default) to disable
 * @param flush - flush the active ring before starting autoflush
 *
 * @return bool - previous value of the enable flag
 */
bool
pmlog_autoflush_ctx(void *ctxp, bool enable, bool flush)
{
	bool old = false;
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();
	if (!(ctx = _get_context(ctxp)))
		return old;

	g_mutex_lock(&ctx->mutex); _bloat();
	_mtx_sync(ctx);
	if (ctx->wrthread_data) {
		if (enable) {
			ringbuf_t *ring = _mtx_getactivering(ctx);
			if (flush) {
				_mtx_flushring(ctx, ring);
			} else {
				_mtx_clearring(ring);
			}
		}
		old = ctx->wrthread_data->autoflush_enable;
		ctx->wrthread_data->autoflush_enable = enable;
	}
	g_mutex_unlock(&ctx->mutex);
	return old;
}

/**
 * Enable or disable logging to the log file, and return the previous value.
 *
 * @param ctxp - logging context, or NULL for default
 * @param enable - true (default) to enable log file logging, false to disable
 *
 * @return bool - previous value of the enable flag
 */
bool
pmlog_logwrt_ctx(void *ctxp, bool enable)
{
	bool old = false;
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();
	if (!(ctx = _get_context(ctxp)))
		return old;

	g_mutex_lock(&ctx->mutex); _bloat();
	_mtx_sync(ctx);
	if (ctx->wrthread_data) {
		old = ctx->wrthread_data->logwrt_enable;
		ctx->wrthread_data->logwrt_enable = enable;
	}
	g_mutex_unlock(&ctx->mutex);
	return old;
}

/**
 * Set the stderr debugging and trace levels.
 *
 * The errno value is set on error return, with the following values:
 *
 * - ENOBUFS - logging is disabled
 *
 * @param ctxp - logging context, or NULL for default
 * @param D_level - desired debug level (0-2), or -1 to ignore
 * @param T_level - desired trace level (0-3), or -1 to ignore
 *
 * @return int - 0 on success, -1 on error
 */
int
pmlog_stderr_set_level_ctx(void *ctxp, int D_level, int T_level)
{
	int retval = -1;
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();
	if (!(ctx = _get_context(ctxp))) {
		errno = ENOBUFS;
		return retval;
	}

	g_mutex_lock(&ctx->mutex); _bloat();
	_mtx_sync(ctx);
	// Set the mask
	ctx->stderr_mask =
			_level_to_mask(ctx->stderr_mask, D_level, T_level);
	retval = 0;
	g_mutex_unlock(&ctx->mutex);
	return retval;
}

/**
 * Get the current stderr debugging and trace levels.
 *
 * The errno value is set on error return, with the following values:
 *
 * - ENOBUFS - logging is disabled 
 *
 * @param ctxp - logging context, or NULL for default
 * @param D_level - pointer to debug return level, or NULL to ignore
 * @param T_level - pointer to trace return level, or NULL to ignore
 *
 * @return int - 0 on success, -1 on failure
 */
int
pmlog_stderr_get_level_ctx(void *ctxp, int *D_level, int *T_level)
{
	int retval = -1;
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();
	if (!(ctx = _get_context(ctxp))) {
		errno = ENOBUFS;
		return retval;
	}

	g_mutex_lock(&ctx->mutex); _bloat();
	_mtx_sync(ctx);
	if (ctx->wrthread_data) {
		uint64_t msk = ctx->stderr_mask;
		if (D_level) {
			if (msk & (1 << LOG_TYPE_DEBUG2)) {
				*D_level = 2;
			} else if (msk & (1 << LOG_TYPE_DEBUG1)) {
				*D_level = 1;
			} else {
				*D_level = 0;
			}
		}
		if (T_level) {
			if (msk & (1 << LOG_TYPE_TRACE3)) {
				*T_level = 3;
			} else if (msk & (1 << LOG_TYPE_TRACE2)) {
				*T_level = 2;
			} else if (msk & (1 << LOG_TYPE_TRACE1)) {
				*T_level = 1;
			} else {
				*T_level = 0;
			}
		}
		retval = 0;
	}
	g_mutex_unlock(&ctx->mutex);
	return retval;
}

/**
 * Flush the active ring buffer.
 *
 * @param ctxp - logging context, or NULL for default
 */
void
pmlog_flush_ring_ctx(void *ctxp)
{
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();
	if (!(ctx = _get_context(ctxp)))
		return;
	g_mutex_lock(&ctx->mutex); _bloat();
	if (ctx->wrthread_data) {
		ringbuf_t *ring = _mtx_getactivering(ctx);
		_mtx_flushring(ctx, ring);
	}
	g_mutex_unlock(&ctx->mutex);
}

/**
 * Clear the active ring buffer.
 *
 * @param ctxp - logging context, or NULL for default
 */
void
pmlog_clear_ring_ctx(void *ctxp)
{
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();
	if (!(ctx = _get_context(ctxp)))
		return;
	g_mutex_lock(&ctx->mutex); _bloat();
	if (ctx->wrthread_data) {
		ringbuf_t *ring = _mtx_getactivering(ctx);
		_mtx_clearring(ring);
	}
	g_mutex_unlock(&ctx->mutex);
}

/**
 * Trigger a log file rotation.
 *
 * @param ctxp - logging context, or NULL for default
 */
void
pmlog_rotate_ctx(void *ctxp)
{
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();
	if (!(ctx = _get_context(ctxp)))
		return;
	g_mutex_lock(&ctx->mutex); _bloat();
	if (ctx->wrthread_data) {
		ctx->wrthread_data->rotate = true;
		_bloat();
		g_cond_signal(&ctx->workcond);
		DBG("waiting for rotation");
		while (ctx->wrthread_data->rotate) {
			DBG("suspend");
			_bloat();
			g_cond_wait(&ctx->idlecond, &ctx->mutex);
			DBG("resume");
		}
	}
	g_mutex_unlock(&ctx->mutex);
}

/**
 * Synchronize with the write thread. This triggers the write thread to look for
 * work, and returns when the write thread returns to an idle state.
 *
 * @param ctxp - logging context, or NULL for default
 */
void
pmlog_sync_ctx(void *ctxp)
{
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();
	if (!(ctx = _get_context(ctxp)))
		return;
	g_mutex_lock(&ctx->mutex); _bloat();
	_mtx_sync(ctx);
	g_mutex_unlock(&ctx->mutex);
}

/**
 * Terminate the logging system. Thread-safe. This honors the reference count,
 * so only the last termination in a series will actually shut down logging.
 *
 * @param ctxp - logging context, or NULL for default
 */
void
pmlog_term_ctx(void *ctxp)
{
	GThread *thd = NULL;
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();
	if (!(ctx = _get_context(ctxp)))
		return;

	// Shut down the logging
	g_mutex_lock(&ctx->mutex); _bloat();
	_mtx_sync(ctx);
	_mtx_pmlog_term(ctx, &thd, false);
	g_mutex_unlock(&ctx->mutex);

	// Clean up any write thread resources
	_join_wrthread(thd);
}

/**
 * Terminate the logging system. Thread-safe. This overrides the reference
 * count, and pre-emptively shuts down the logging.
 */
void
pmlog_term_all(void)
{
	GThread *thd = NULL;
	GList *item;

	// Ensure static structures are initialized
	GLOBALINIT();

	g_mutex_lock(&_global_mutex);
	for (item = _context_list; item; item = item->next) {
		log_context_t *ctx = (log_context_t *)item->data;
		// Shut down the logging
		g_mutex_lock(&ctx->mutex); _bloat();
		_mtx_sync(ctx);
		_mtx_pmlog_term(ctx, &thd, true);
		g_mutex_unlock(&ctx->mutex);

		// Clean up any write thread resources
		_join_wrthread(thd);
	}
	g_mutex_unlock(&_global_mutex);
}

/**
 * Set the logging enable code. A valid code can be specified. If an invalid
 * code is specified, the current environment variable PMLOG_ENABLE is read and
 * that value is used.
 *
 * @param enable - LOG_ENABLE_* value, or -1 to force a re-read of the
 *      	 environment variable PMLOG_ENABLE
 *
 * @return int - old value of the enable code
 */
int
pmlog_enable(int enable)
{
	// Ensure static structures are initialized
	GLOBALINIT();

	if (enable < 0 || enable >= NUM_LOG_ENABLES)
		enable = _get_log_enable();
	return _set_log_enable(enable);
}

/**
 * Initialize the logging system. Thread-safe.
 *
 * Any argument can be passed as 0 (or NULL) to use defaults.
 *
 * ctxp can be specified as NULL to use the default (first) logging context.
 *
 * max_size can be specified as -1 to disable automatic file rotation, and allow
 * the log file to grow without limit. The files can still be manually rotated
 * (call to pmlog_rotate()), if the max_files value is > 1.
 *
 * max_files is not capped, but larger values will cause the log rotation to
 * become slow over time. A value of -1 or 1 will prohibit rotation, even forced
 * rotation (there can be only one log file).
 *
 * num_rings is not capped, but larger values will consume more global memory
 * for diminishing increase in performance. A value of -1 will disable the
 * normal ring buffers, conserving memory if only message logging is desired. In
 * this case, only INTERNAL and MESSAGE type messages will appear in the log,
 * unless autoflush is turned on, in which case ALL message types will appear in
 * the log.
 *
 * ring_size is not capped, but larger values will use more global memory. The
 * ring should be no larger than a "region of historical interest" when
 * diagnosing a failure. A value of -1 is treated the same as zero, i.e. will
 * result in the default ring size. Note that if num_rings is -1, this value is
 * ignored entirely.
 *
 * This will consume global memory for the ring buffers, and will use up one
 * thread for posting log messages to the log file asynchronously.
 *
 * Initialization is managed by reference count within a process. If multiple
 * threads all try to initialize the system, only the first will do anything,
 * and the others will simply block until the first one is done. Similarly, only
 * the final termination will terminate the system.
 *
 * The errno value is set on error return, with the following values:
 *
 * - ENOBUFS - logging is disabled
 * - EINVAL  - invalid argument value
 * - ENOMEM  - memory allocation failed
 * - ECHILD  - write thread insufficient resources
 * - other   - any file system error associated with opening the log file
 *
 * @param ctxp - logging context, or NULL for first
 * @param log_path - log file path, or NULL for default
 * @param max_size - maximum file size, 0 for default, -1 for unlimited growth
 * @param max_files - maximum file count, 0 for default, -1 to disable rotation
 * @param num_rings - number of ring buffers, 0 for default, -1 to disable
 * @param ring_size - log file ring buffer size in bytes, or 0 for default
 *
 * @return void * - logging context pointer
 */
void *
pmlog_init_ctx(void *ctxp, const char *log_path,
		int64_t max_size, int64_t max_files,
		int64_t num_rings, int64_t ring_size)
{
	void *retval = NULL;
	GThread *thd = NULL;
	int errcode = 0;
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Proceed with initialization
	if (!(ctx = _get_context(ctxp))) {
		errno = ENOBUFS;
		return retval;
	}
	g_mutex_lock(&ctx->mutex); _bloat();
	if (!_mtx_pmlog_init(ctx, log_path, max_size, max_files, num_rings, ring_size, &thd)) {
		errcode = errno;
		goto done;
	}
	if (!_mtx_file_open_errcode(ctx)) {
		errcode = errno;
		_mtx_pmlog_term(ctx, &thd, false);
		goto done;
	}

	retval = (void *)ctx;

done:
	g_mutex_unlock(&ctx->mutex);
	_join_wrthread(thd);

	DBG("retval = %p", retval);
	if (retval == NULL) {
		DBG("errno  = %d: %m", errno);
		errno = errcode;
	}
	return retval;
}

/**
 * Initialize the logging system in a new context. Thread-safe.
 *
 * This is the same as pmlog_init_ctx(), but creates a new logging context.
 *
 * @param log_path - log file path, or NULL for default
 * @param max_size - maximum file size, 0 for default, -1 for unlimited growth
 * @param max_files - maximum file count, 0 for default, -1 to disable rotation
 * @param num_rings - number of ring buffers, 0 for default, -1 to disable
 * @param ring_size - log file ring buffer size in bytes, or 0 for default
 *
 * @return void * - logging context pointer
 */
void *
pmlog_init_new(const char *log_path,
		int64_t max_size, int64_t max_files,
		int64_t num_rings, int64_t ring_size)
{
	return pmlog_init_ctx(_new_context(), log_path,
			max_size, max_files, num_rings, ring_size);
}

/* -------------------------------------------------------------------
 * White-box regression tests. Because these test internal states which are not
 * exported through the interface, these test routines need to be part of this
 * module. In addition, if internals change, these tests will also need to be
 * changed.
 */

// Storage for 'assert' comment
static char _comment[1024] = { 0 };

// Define a comment
#define comment(fmt, args...) \
        snprintf(_comment, sizeof(_comment), fmt, ##args)

// Assert that the expression evaluates to true (non-zero)
#define assert(expr, txt...) \
        if (!(expr)) do{printf("*** failed (%s): %s " txt "\n", #expr, _comment);errcnt++;}while(0)

/**
 * Like memset(), this tests to ensure that the entire length of data in the
 * buffer matches the specified value.
 *
 * @param buf - buffer to test
 * @param val - value to check for
 * @param len - number of bytes to check
 *
 * @return int - zero if successful, -1 if failure
 */
static int
_memchk(void *buf, int val, size_t len)
{
	unsigned char *b = (unsigned char *)buf;
	while (len-- > 0)
		if (*b++ != val)
			return -1;
	return 0;
}

/**
 * Return the difference between two timevals in microseconds.
 *
 * @param tv1 - later timeval
 * @param tv0 - earlier timeval
 *
 * @return uint64_t - (tv1 - tv0) in microseconds
 */
static uint64_t
_usecs(struct timeval *tv1, struct timeval *tv0)
{
	if (tv1->tv_usec < tv0->tv_usec) {
		tv1->tv_usec += 1000000;
		tv1->tv_sec  -= 1;
	}
	return (uint64_t)1000000 * (tv1->tv_sec - tv0->tv_sec) + (tv1->tv_usec - tv0->tv_usec);
}

/**
 * Check global variables for uninitialized values.
 *
 * @param void
 *
 * @return int - error count
 */
static int
_check_uninit(void)
{
	int errcnt = 0;
	log_context_t *ctx = _get_context(NULL);
	assert(ctx != NULL);
	if (ctx) {
		assert(ctx->init_count == 0);
		assert(ctx->rings == NULL);
		assert(ctx->num_rings == 0);
		assert(ctx->lockqp == NULL);
		assert(ctx->idleqp == NULL);
		assert(ctx->activering == NULL);
		assert(ctx->wrthruring == NULL);
		assert(ctx->wrthread_data == NULL);
	}
	return errcnt;
}

/**
 * Check global variables for initialized values.
 *
 * @param void
 *
 * @return int - error count
 */
static int
_check_init(void)
{
	int errcnt = 0;
	log_context_t *ctx = _get_context(NULL);
	assert(ctx != NULL);
	if (ctx) {
		assert(ctx->init_count != 0);
		assert(ctx->rings != NULL);
		assert(ctx->lockqp != NULL);
		assert(ctx->idleqp != NULL);
		assert(ctx->wrthruring != NULL);
		assert(ctx->wrthread_data != NULL);
	}
	return errcnt;
}

/**
 * Clear initialization environment variables.
 */
static void
_clearenv(void)
{
	unsetenv("PMLOG_FILE_PATH");
	unsetenv("PMLOG_MAX_FILE_SIZE");
	unsetenv("PMLOG_MAX_FILE_COUNT");
	unsetenv("PMLOG_NUM_RINGS");
	unsetenv("PMLOG_RING_SIZE");
	unsetenv("PMLOG_DEBUG_LEVEL");
	unsetenv("PMLOG_TRACE_LEVEL");
}

/**
 * Set an environment variable to a number.
 *
 * @param name - environment variable name
 * @param val - numeric value
 */
static void
_setenvnum(const char *name, int64_t val)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "%ld", val);
	setenv(name, buf, 1);
}

/**
 * Enable or disable timing bloat.
 *
 * @param enable - true to enable, false to disable
 *
 * @return bool - previous value
 */
bool
test_bloat(bool enable)
{
	bool old = _bloat_enable;
	_bloat_enable = enable;
	return old;
}

/**
 * Run initialization permutations, and white-box test the internal values of
 * the system.
 *
 * @param void
 *
 * @return int - error count
 */
int
test_pmlog_init(void)
{
	int errcnt = 0;
	struct stat sb;
	int Dlevel;
	int Tlevel;
	int errs, i;
	log_context_t *ctx = NULL;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Clear environment variables that will affect this test
	_clearenv();

	// Test startup initialization
	unlink(LOG_FILE_PATH_DFL);
	unlink(LOG_FILE_PATH_DFL ".ctl");
	comment("pre-init");
	errcnt += _check_uninit();

	/*
	 * Test all API functions without initializaing.
	 * These should not initialize anything, and should not segfault.
	 */
	comment("pre-init term_all");
	pmlog_term_all();
	errcnt += _check_uninit();
	assert(stat(LOG_FILE_PATH_DFL, &sb) == -1);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == -1);

	comment("pre-init term");
	pmlog_term();
	errcnt += _check_uninit();
	assert(stat(LOG_FILE_PATH_DFL, &sb) == -1);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == -1);

	comment("pre-init sync");
	pmlog_sync();
	errcnt += _check_uninit();
	assert(stat(LOG_FILE_PATH_DFL, &sb) == -1);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == -1);

	comment("pre-init autoflush");
	assert(pmlog_autoflush(true, false) == false, "#1");
	assert(pmlog_autoflush(true, false) == false, "#2");
	assert(stat(LOG_FILE_PATH_DFL, &sb) == -1);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == -1);

	comment("pre-init logwrt");
	errcnt += _check_uninit();
	assert(pmlog_logwrt(true) == false, "#1");
	assert(pmlog_logwrt(true) == false, "#2");
	assert(stat(LOG_FILE_PATH_DFL, &sb) == -1);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == -1);

	comment("pre-init set stderr");
	errcnt += _check_uninit();
	assert(pmlog_stderr_set_level(1, 1) == 0, "#1");
	assert(pmlog_stderr_set_level(1, 1) == 0, "#2");
	errcnt += _check_uninit();
	assert(stat(LOG_FILE_PATH_DFL, &sb) == -1);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == -1);

	comment("pre-init get stderr");
	errcnt += _check_uninit();
	assert(pmlog_stderr_get_level(&Dlevel, &Tlevel) == -1, "#1");
	assert(pmlog_stderr_get_level(&Dlevel, NULL) == -1, "#2");
	assert(pmlog_stderr_get_level(NULL, &Tlevel) == -1, "#3");
	assert(pmlog_stderr_get_level(NULL, NULL) == -1, "#4");
	errcnt += _check_uninit();
	assert(stat(LOG_FILE_PATH_DFL, &sb) == -1);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == -1);

	comment("pre-init flush");
	pmlog_flush_ring();
	errcnt += _check_uninit();
	assert(stat(LOG_FILE_PATH_DFL, &sb) == -1);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == -1);

	comment("pre-init clear");
	pmlog_clear_ring();
	errcnt += _check_uninit();
	assert(stat(LOG_FILE_PATH_DFL, &sb) == -1);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == -1);

	comment("pre-init rotate");
	pmlog_rotate();
	errcnt += _check_uninit();
	assert(stat(LOG_FILE_PATH_DFL, &sb) == -1);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == -1);

	/*
	 * Test default initialization. This is what is used if pmlog_message()
	 * is called without initializing.
	 */
	comment("default init");
	unlink(LOG_FILE_PATH_DFL);
	unlink(LOG_FILE_PATH_DFL ".ctl");
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	assert(stat(LOG_FILE_PATH_DFL, &sb) == 0);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == 0);
	errcnt += (errs = _check_init());
	if (errs == 0) {
		ctx = _get_context(NULL);
		assert(ctx->num_rings == LOG_NUM_RINGS_DFL);
		for (i = 0; i < ctx->num_rings; i++) {
			assert(ctx->rings[i].size == LOG_RING_SIZE_DFL);
		}
		assert(ctx->rings[ctx->num_rings].size == WRTHRU_SIZE);
		assert(strcmp(ctx->wrthread_data->file_path, LOG_FILE_PATH_DFL) == 0);
		assert(ctx->wrthread_data->max_size == LOG_FILE_SIZE_DFL);
		assert(ctx->wrthread_data->max_files == LOG_FILE_COUNT_DFL);
		assert(ctx->wrthread_data->write_errs == 0);
		assert(ctx->wrthread_data->rotations == 0);
		assert(ctx->wrthread_data->errcode == 0);
		assert(ctx->wrthread_data->sync == false);
		assert(ctx->wrthread_data->rotate == false);
		assert(ctx->wrthread_data->terminate == false);
		assert(ctx->wrthread_data->autoflush_enable == false);
		assert(ctx->wrthread_data->logwrt_enable == true);
		assert(ctx->wrthread_data->self != NULL);
	}

	/*
	 * After initialization, test the API functions for return values and no
	 * segfaults.
	 */
	comment("post-init sync");
	pmlog_sync();

	comment("post-init autoflush");
	assert(pmlog_autoflush(true, false) == false, "#1");
	assert(pmlog_autoflush(true, false) == true, "#2");
	assert(pmlog_autoflush(false, false) == true, "#3");
	assert(pmlog_autoflush(false, false) == false, "#4");

	comment("post-init logwrt");
	assert(pmlog_logwrt(false) == true, "#1");
	assert(pmlog_logwrt(false) == false, "#2");
	assert(pmlog_logwrt(true) == false, "#3");
	assert(pmlog_logwrt(true) == true, "#4");

	comment("post-init set stderr");
	assert(pmlog_stderr_set_level(1, 2) == 0);
	Dlevel = Tlevel = -1;
	assert(pmlog_stderr_get_level(&Dlevel, &Tlevel) == 0, "#1");
	assert(Dlevel == 1);
	assert(Tlevel == 2);
	Dlevel = Tlevel = -1;
	assert(pmlog_stderr_get_level(&Dlevel, NULL) == 0, "#2");
	assert(Dlevel == 1);
	assert(pmlog_stderr_get_level(NULL, &Tlevel) == 0, "#3");
	assert(Tlevel == 2);
	assert(pmlog_stderr_get_level(NULL, NULL) == 0, "#4");

	comment("post-init clr stderr");
	assert(pmlog_stderr_set_level(0, 0) == 0);
	Dlevel = Tlevel = -1;
	assert(pmlog_stderr_get_level(&Dlevel, &Tlevel) == 0, "#1");
	assert(Dlevel == 0);
	assert(Tlevel == 0);
	Dlevel = Tlevel = -1;
	assert(pmlog_stderr_get_level(&Dlevel, NULL) == 0, "#2");
	assert(Dlevel == 0);
	assert(pmlog_stderr_get_level(NULL, &Tlevel) == 0, "#3");
	assert(Tlevel == 0);
	assert(pmlog_stderr_get_level(NULL, NULL) == 0, "#4");

	comment("post-init flush");
	pmlog_flush_ring();

	comment("post-init clear");
	pmlog_clear_ring();

	comment("post-init rotate");
	pmlog_rotate();

	pmlog_term();
	errcnt += _check_uninit();

	/*
	 * Test initialization with minimal parameters (no debug rings).
	 */
	comment("minimal init");
	unlink(LOG_FILE_PATH_DFL);
	unlink(LOG_FILE_PATH_DFL ".ctl");
	assert(pmlog_init(NULL, -1, -1, -1, -1) == 0);
	assert(stat(LOG_FILE_PATH_DFL, &sb) == 0);
	assert(stat(LOG_FILE_PATH_DFL ".ctl", &sb) == 0);
	errcnt += (errs = _check_init());
	if (errs == 0) {
		ctx = _get_context(NULL);
		assert(ctx->num_rings == 0);
		assert(ctx->rings[ctx->num_rings].size == WRTHRU_SIZE);
		assert(strcmp(ctx->wrthread_data->file_path, LOG_FILE_PATH_DFL) == 0);
		assert(ctx->wrthread_data->max_size == 0);
		assert(ctx->wrthread_data->max_files == 1);
		assert(ctx->wrthread_data->write_errs == 0);
		assert(ctx->wrthread_data->rotations == 0);
		assert(ctx->wrthread_data->errcode == 0);
		assert(ctx->wrthread_data->sync == false);
		assert(ctx->wrthread_data->rotate == false);
		assert(ctx->wrthread_data->terminate == false);
		assert(ctx->wrthread_data->autoflush_enable == false);
		assert(ctx->wrthread_data->logwrt_enable == true);
		assert(ctx->wrthread_data->self != NULL);
	}
	// Make sure that these don't blow up
	pmlog_message(LOG_TYPE_MESSAGE, "LOG_TYPE_MESSAGE");
	pmlog_message(LOG_TYPE_CRITICAL, "LOG_TYPE_CRITICAL");
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_DEBUG1");
	pmlog_term();
	errcnt += _check_uninit();

	/*
	 * Test non-default initialization parameters.
	 */
	comment("alternate init");
	unlink("/tmp/pmlog.log");
	unlink("/tmp/pmlog.log.ctl");
	assert(pmlog_init("/tmp/pmlog.log",
					LOG_FILE_SIZE_DFL + 1, LOG_FILE_COUNT_DFL + 1,
					LOG_NUM_RINGS_DFL + 1, LOG_RING_SIZE_DFL + 1) == 0);
	assert(stat("/tmp/pmlog.log", &sb) == 0);
	assert(stat("/tmp/pmlog.log.ctl", &sb) == 0);
	errcnt += (errs = _check_init());
	if (errs == 0) {
		ctx = _get_context(NULL);
		assert(ctx->num_rings == LOG_NUM_RINGS_DFL + 1);
		for (i = 0; i < ctx->num_rings; i++) {
			assert(ctx->rings[i].size == LOG_RING_SIZE_DFL + 8);
		}
		assert(ctx->rings[ctx->num_rings].size == WRTHRU_SIZE);
		assert(strcmp(ctx->wrthread_data->file_path, "/tmp/pmlog.log") == 0);
		assert(ctx->wrthread_data->max_size == LOG_FILE_SIZE_DFL + 1024);
		assert(ctx->wrthread_data->max_files == LOG_FILE_COUNT_DFL + 1);
		assert(ctx->wrthread_data->write_errs == 0);
		assert(ctx->wrthread_data->rotations == 0);
		assert(ctx->wrthread_data->errcode == 0);
		assert(ctx->wrthread_data->sync == false);
		assert(ctx->wrthread_data->rotate == false);
		assert(ctx->wrthread_data->terminate == false);
		assert(ctx->wrthread_data->autoflush_enable == false);
		assert(ctx->wrthread_data->logwrt_enable == true);
		assert(ctx->wrthread_data->self != NULL);
	}
	pmlog_term();
	errcnt += _check_uninit();
	unlink("/tmp/pmlog.log");
	unlink("/tmp/pmlog.log.ctl");

	/*
	 * Test initialization using environment variables.
	 */
	setenv("PMLOG_FILE_PATH", "/tmp/pmlog.log", 1);
	_setenvnum("PMLOG_MAX_FILE_SIZE", LOG_FILE_SIZE_DFL + 1);
	_setenvnum("PMLOG_MAX_FILE_COUNT", LOG_FILE_COUNT_DFL + 1);
	_setenvnum("PMLOG_NUM_RINGS", LOG_NUM_RINGS_DFL + 1);
	_setenvnum("PMLOG_RING_SIZE", LOG_RING_SIZE_DFL + 1);
	_setenvnum("PMLOG_DEBUG_LEVEL", 1);
	_setenvnum("PMLOG_TRACE_LEVEL", 2);
	comment("setenv init");
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	assert(stat("/tmp/pmlog.log", &sb) == 0);
	assert(stat("/tmp/pmlog.log.ctl", &sb) == 0);
	errcnt += (errs = _check_init());
	if (errs == 0) {
		ctx = _get_context(NULL);
		assert(ctx->num_rings == LOG_NUM_RINGS_DFL + 1);
		for (i = 0; i < ctx->num_rings; i++) {
			assert(ctx->rings[i].size == LOG_RING_SIZE_DFL + 8);
		}
		assert(ctx->rings[ctx->num_rings].size == WRTHRU_SIZE);
		assert(strcmp(ctx->wrthread_data->file_path, "/tmp/pmlog.log") == 0);
		assert(ctx->wrthread_data->max_size == LOG_FILE_SIZE_DFL + 1024);
		assert(ctx->wrthread_data->max_files == LOG_FILE_COUNT_DFL + 1);
		assert(ctx->wrthread_data->write_errs == 0);
		assert(ctx->wrthread_data->rotations == 0);
		assert(ctx->wrthread_data->errcode == 0);
		assert(ctx->wrthread_data->sync == false);
		assert(ctx->wrthread_data->rotate == false);
		assert(ctx->wrthread_data->terminate == false);
		assert(ctx->wrthread_data->autoflush_enable == false);
		assert(ctx->wrthread_data->logwrt_enable == true);
		assert(ctx->wrthread_data->self != NULL);
	}
	Dlevel = Tlevel = -1;
	assert(pmlog_stderr_get_level(&Dlevel, &Tlevel) == 0);
	assert(Dlevel == 1);
	assert(Tlevel == 2);
	pmlog_term();
	errcnt += _check_uninit();
	unlink("/tmp/pmlog.log");
	unlink("/tmp/pmlog.log.ctl");
	_clearenv();

	/*
	 * Test initialization failure due to bad file path.
	 */
	comment("illegal file init");
	assert(pmlog_init("/tmp/nosuchdir/pmlog.log", 0, 0, 0, 0) == -1);
	assert(errno == ENOENT);
	assert(stat("/tmp/nosuchdir/pmlog.log", &sb) == -1);
	errcnt += _check_uninit();

	return errcnt;
}

/**
 * Whitebox test the ring wridx/rdidx pointers.
 *
 * This runs through all legal message sizes, and overfills the ring, then reads
 * back data and ensures that all records are intact.
 *
 * @param void
 *
 * @return int - error count
 */
int
test_pmlog_ringfill(void)
{
	int errcnt = 0;
	ringbuf_t *ring;
	msgblk_t local;
	uint64_t i, N;
	int len, spc;
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Clear environment variables that will affect this test
	_clearenv();

	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0, "test_pmlog_ringfill");
	ctx = _get_context(NULL);
	assert(ctx != NULL);
	if (errcnt != 0) {
		return errcnt;
	}

	ring = &ctx->rings[0];

	for (len = 8; len <= MAX_MSG_SIZE; len += 8) {
		// Zap the local message buffer and set lengths
		memset(&local, 0, sizeof(local));
		memset(local.message, 1, len);
		local.txtlen = len;
		local.msglen = local.txtlen + MSG_HDR_SIZE;
		// Clear the ring and initialize the space counter
		_mtx_clearring(ring);
		spc = ring->size;
		// Enough times to fill the ring and then some
		N = ring->size / local.msglen + 10;
		// Fail on the first errcnt > 0
		for (i = 0; i < N && errcnt == 0; i++) {
			comment("put message, len=%d, i=%ld", len, i);
			assert(_mtx_ringspace(ring) == spc);
			_mtx_putmessage(ring, &local);
			assert(ring->wridx != ring->rdidx);
			// Expected space left
			spc -= local.msglen;
			while (spc <= 0)
				spc += local.msglen;
		}
		// Corrupt the local message buffer
		memset(local.message, 2, len);
		i = 0;
		// Fetch the entire ring until empty
		while (_mtx_getmessage(ring, &local)) {
			// Read should have overwritten corruption
			comment("get message, len=%d, i=%ld", len, i);
			assert(_memchk(local.message, 1, len) == 0);
			// Corrupt the local message buffer
			memset(local.message, 2, len);
			i++;
		}
		// Ring should now be empty
		assert(ring->wridx == ring->rdidx);
	}

	pmlog_term();
	return errcnt;
}

/**
 * Whitebox check of file rotation.
 *
 * The test, below, goes through multiple passes of writing an "end" message to
 * the file, then rotating, then writing a "begin" message to the new file. So
 * the current log file should have one "begin" message, and each previous file
 * should have a "begin (start)" message, and an "end (start+1)" message.
 *
 * This check skips over LOG_TYPE_INTERNAL messages, and checks all other lines
 * to ensure they have the correct information in the correct order.
 *
 * A zero line count indicates that the file should not exist.
 *
 * @param path - path to file (with appropriate suffix)
 * @param nlines - number of lines we expect (0, 1, or 2)
 * @param start - 'pass' number from test
 *
 * @return int - error count
 */
static int
_check_rotate(const char *path, int nlines, int start)
{
	int errcnt = 0;
	FILE *fid = NULL;
	char buf[1024];
	char ref[1024];
	int lnum = 0;

	fid = fopen(path, "r");
	if (nlines == 0) {
		// File should not exist
		assert(fid == NULL);
	} else {
		// File should exist
		assert(fid != NULL);
	}
	if (fid) {
		lnum = 0;
		while (lnum < nlines && fgets(buf, sizeof(buf), fid)) {
			int typ;
			char *ptr = _msgparse(buf, 0, 0, 0, 0, &typ);
			if (typ == LOG_TYPE_INTERNAL)
				continue;
			if (lnum == 0) {
				snprintf(ref, sizeof(ref), "Beg pass #%d\n", start);
				comment("%s: strcmp('%s','%s')", path, ref, ptr);
				assert(strcmp(ref, ptr) == 0);
			} else if (lnum == 1) {
				snprintf(ref, sizeof(ref), "End pass #%d\n", start + 1);
				comment("%s: strcmp('%s','%s')", path, ref, ptr);
				assert(strcmp(ref, ptr) == 0);
			}
			lnum++;
		}
		fclose(fid);
	}
	return errcnt;
}

/**
 * Whitebox test file rotation.
 *
 * This does forced rotation after writing known data to the log, then checks
 * the content to ensure that we've moved and kept the files we intended.
 *
 * This then re-opens with a smaller rotation file count, and ensures that it
 * properly purges the excess logs.
 *
 * @param void
 *
 * @return int - error count
 */
int
test_pmlog_rotate(void)
{
	int errcnt = 0;
	int i;
	log_context_t *ctx;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Clear environment variables that will affect this test
	_clearenv();

	// Set max files to 5, rotate 10 times, make sure there are 5 files
	assert(pmlog_init(NULL, 0, 5, 0, 0) == 0, "test_pmlog_rotate");
	ctx = _get_context(NULL);
	assert(ctx != NULL);
	if (errcnt != 0) {
		return errcnt;
	}
	for (i = 0; i < 10; i++) {
		_thr_msg(ctx->wrthread_data, LOG_TYPE_MESSAGE, "End pass #%d\n", i);
		pmlog_rotate();
		_thr_msg(ctx->wrthread_data, LOG_TYPE_MESSAGE, "Beg pass #%d\n", i);
	}
	pmlog_term();
	errcnt += _check_rotate(LOG_FILE_PATH_DFL, 1, 9);
	errcnt += _check_rotate(LOG_FILE_PATH_DFL ".1", 2, 8);
	errcnt += _check_rotate(LOG_FILE_PATH_DFL ".2", 2, 7);
	errcnt += _check_rotate(LOG_FILE_PATH_DFL ".3", 2, 6);
	errcnt += _check_rotate(LOG_FILE_PATH_DFL ".4", 2, 5);
	errcnt += _check_rotate(LOG_FILE_PATH_DFL ".5", 0, 0);

	// Set max files to 3, make sure two files are deleted
	assert(pmlog_init(NULL, 0, 3, 0, 0) == 0);
	pmlog_term();
	errcnt += _check_rotate(LOG_FILE_PATH_DFL ".3", 0, 0);
	errcnt += _check_rotate(LOG_FILE_PATH_DFL ".4", 0, 0);
	errcnt += _check_rotate(LOG_FILE_PATH_DFL ".5", 0, 0);

	return errcnt;
}

/**
 * Test the performance.
 *
 * This walks through 'iters' iterations of the supplied test function,
 * increasing the message size by 8 bytes each time.
 *
 * It starts with a calibration run, which monitors time, and keeps doubling the
 * number of iterations until 0.5 seconds (or more) has elapsed. It then
 * estimates the number of iteratons it will take to finish in one second, and
 * performs that number of iterations, recording the total time, to eliminate
 * overheads associated with getting and converting the time values.
 *
 * The returned value is total iterations divided by the total number of
 * seconds.
 *
 * @param len - length of message in bytes
 * @param func - function to test
 * @param data - data for the function
 *
 * @return double - average operations/second
 */
static double
_test_rate(int len, void (*func)(void *, msgblk_t *), void *data)
{
	msgblk_t local;
	uint64_t i, N, T;
	struct timeval tv0, tv1;

	// Do a calibration pass
	memset(&local, 'A', sizeof(msgblk_t));
	local.message[len] = 0;
	local.txtlen = len;
	local.msglen = ((local.txtlen + 7) & ~7) + MSG_HDR_SIZE;
	// Double N until the total time is > 0.5 sec
	N = 1;
	while (true) {
		N *= 2;
		gettimeofday(&tv0, NULL);
		for (i = 0; i < N; i++) {
			(*func)(data, &local);
		}
		gettimeofday(&tv1, NULL);
		T = _usecs(&tv1, &tv0);
		if (T > 500000)
			break;
	}
	// Estimated iterations to burn one second
	N = (1000000 * N) / T;
	// Time a fixed number of iterations
	gettimeofday(&tv0, NULL);
	for (i = 0; i < N; i++) {
		(*func)(data, &local);
	}
	gettimeofday(&tv1, NULL);
	T = _usecs(&tv1, &tv0);
	return (!T) ? 0.0 : (double)1000000.0 * (double)N / (double)T;
}

/**
 * Test pure ring performance.
 *
 * @param len - length of message in bytes
 *
 * @return double - average operations/second
 */
static void
_ringrate(void *data, msgblk_t *local)
{
	_mtx_putmessage((ringbuf_t *)data, local);
}
double
test_pmlog_ringrate(int len)
{
	ringbuf_t *ring;
	double rate = 0.0;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Clear environment variables that will affect this test
	_clearenv();

	if (pmlog_init(NULL, 0, 0, 0, 0) == 0) {
		log_context_t *ctx = _get_context(NULL);
		ring = &ctx->rings[0];
		rate = _test_rate(len, _ringrate, ring);
		pmlog_term();
	}

	return rate;
}

/**
 * Test pure file write performance.
 *
 * @param len - length of message in bytes
 *
 * @return double - average operations/second
 */
static void
_filerate(void *data, msgblk_t *local)
{
	_thr_internal((thread_data_t *)data, "%s", local->message);
}
int
test_pmlog_filerate(int len)
{
	double rate = 0.0;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Clear environment variables that will affect this test
	_clearenv();

	if (pmlog_init(NULL, 0, 0, 0, 0) == 0) {
		log_context_t *ctx = _get_context(NULL);
		rate = _test_rate(len, _filerate, ctx->wrthread_data);
		pmlog_term();
	}

	return rate;
}

/**
 * Test message rate performance.
 *
 * @param msgtype - message type to test
 * @param len - length of message in bytes
 *
 * @return double - average operations/second
 */
static void
_msgrate(void *data, msgblk_t *local)
{
	int *msgtype = (int *)data;
	pmlog_message(*msgtype, "%s", local->message);
}
double
test_pmlog_msgrate(int msgtype, int len)
{
	double rate = 0.0;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Clear environment variables that will affect this test
	_clearenv();

	if (pmlog_init(NULL, 0, 0, 0, 0) == 0) {
		rate = _test_rate(len, _msgrate, &msgtype);
		pmlog_term();
	}

	return rate;
}

/**
 * Chop LFs off end of string.
 *
 * @param buf - buffer containing string
 */
static inline void
_chop(char *buf)
{
	char *p = buf + strlen(buf);
	while (--p >= buf && *p == '\n')
		*p = 0;
}

/**
 * Verify that the next non-internal message in the log matches the reference
 * string created by the formatting.
 *
 * The verbose flag indicates how much noise this function should make.
 *
 * verbose == 0 means it should be silent
 * verbose == 1 means it should print all mismatches except premature EOF
 * verbose == 2 means it should print all mismatches including premature EOF
 *
 * @param verbose - 0 for silent, 1 for errors, 2 for errors+premature EOF
 * @param fid - open file stream to read from
 * @param fmt - printf-like format
 *
 * @return int - 0 for success, 1 for premature EOF, -1 for error
 */
static int __attribute__((__format__(__printf__, 3, 4)))
_verify_message(int verbose, FILE *fid, const char *fmt, ...)
{
	int retval = 0;
	char ref[4096];
	char buf[4096];
	char *exp;
	char *act;
	va_list args;

	// Needs to be valid
	if (fid == NULL) {
		return -1;
	}

	// Generate the expected log message
	exp = NULL;
	if (fmt) {
		va_start(args, fmt);
		vsnprintf(ref, sizeof(ref), fmt, args);
		va_end(args);
		_chop(ref);
		exp = ref;
	}

	// Read log file until a non-INTERNAL message is found
	act = NULL;
	while (act == NULL) {
		int typ;

		if (!fgets(buf, sizeof(buf), fid))
			break;
		_chop(buf);
		act = pmlog_parse(buf, 0, 0, 0, 0, &typ);
		if (act && typ == LOG_TYPE_INTERNAL) {
			act = NULL;
		}
	}

	// Clip off [func:line]
	if (act && *act == '[') {
		while (*act && *act != ' ')
			act++;
		while (*act == ' ')
			act++;
	}

	// Compare reference with actual
	if (!exp && !act) {
		// EOF expected and found
	} else if (!exp) {
		if (verbose >= 1) {
			printf("mismatch: exp=EOF,act=%s\n", act);
		}
		retval = -1;
	} else if (!act) {
		if (verbose >= 2) {
			printf("mismatch: exp=%s,act=EOF\n", exp);
		}
		retval = 1;
	} else if (!strcmp(exp, act)) {
		// matched expected with found
	} else {
		if (verbose >= 1) {
			printf("mismatch: exp=%s,act=%s\n", exp, act);
		}
		retval = -1;
	}
	return retval;
}

// _test_message return converted to an error count (0 or 1), verbose == 2
#define _check_message(fid, fmt, args...) (_verify_message(2, fid, fmt, ##args) ? 1 : 0)

/**
 * Test permutations of the pmlog_message() function.
 *
 * @param void
 *
 * @return int - error count
 */
int
test_pmlog_messages(void)
{
	int errcnt = 0;
	int i, N;
	int bak, new;
	FILE *fid;
	char name[512];
	const char *fmt;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Clear environment variables that will affect this test
	_clearenv();

	// Generate a console message -- have to view console for this
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	pmlog_message(LOG_TYPE_CONSOLE, "LOG_TYPE_CONSOLE\n");
	pmlog_term();

	// MESSAGE should not flush
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	pmlog_rotate();
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_DEBUG1\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_DEBUG2\n");
	pmlog_message(LOG_TYPE_MESSAGE, "LOG_TYPE_MESSAGE\n");
	pmlog_term();
	assert((fid = fopen(LOG_FILE_PATH_DFL, "r")) != NULL);
	if (fid) {
		errcnt += _check_message(fid, "LOG_TYPE_MESSAGE\n");
		errcnt += _check_message(fid, NULL);
		fclose(fid);
	}

	// CRITICAL should flush
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	pmlog_rotate();
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_DEBUG1\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_DEBUG2\n");
	pmlog_message(LOG_TYPE_CRITICAL, "LOG_TYPE_CRITICAL\n");
	pmlog_term();
	assert((fid = fopen(LOG_FILE_PATH_DFL, "r")) != NULL);
	if (fid) {
		errcnt += _check_message(fid, "LOG_TYPE_DEBUG1\n");
		errcnt += _check_message(fid, "LOG_TYPE_DEBUG2\n");
		errcnt += _check_message(fid, "LOG_TYPE_CRITICAL\n");
		errcnt += _check_message(fid, NULL);
		fclose(fid);
	}

	// WARNING should flush
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	pmlog_rotate();
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_DEBUG1\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_DEBUG2\n");
	pmlog_message(LOG_TYPE_WARNING, "LOG_TYPE_WARNING\n");
	pmlog_term();
	assert((fid = fopen(LOG_FILE_PATH_DFL, "r")) != NULL);
	if (fid) {
		errcnt += _check_message(fid, "LOG_TYPE_DEBUG1\n");
		errcnt += _check_message(fid, "LOG_TYPE_DEBUG2\n");
		errcnt += _check_message(fid, "LOG_TYPE_WARNING\n");
		errcnt += _check_message(fid, NULL);
		fclose(fid);
	}

	// FAULT should flush
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	pmlog_rotate();
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_DEBUG1\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_DEBUG2\n");
	pmlog_message(LOG_TYPE_FAULT, "LOG_TYPE_FAULT\n");
	pmlog_term();
	assert((fid = fopen(LOG_FILE_PATH_DFL, "r")) != NULL);
	if (fid) {
		errcnt += _check_message(fid, "LOG_TYPE_DEBUG1\n");
		errcnt += _check_message(fid, "LOG_TYPE_DEBUG2\n");
		errcnt += _check_message(fid, "LOG_TYPE_FAULT\n");
		errcnt += _check_message(fid, NULL);
		fclose(fid);
	}

	// Explicit flush should flush
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	pmlog_rotate();
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_DEBUG1\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_DEBUG2\n");
	pmlog_flush_ring();
	pmlog_term();
	assert((fid = fopen(LOG_FILE_PATH_DFL, "r")) != NULL);
	if (fid) {
		errcnt += _check_message(fid, "LOG_TYPE_DEBUG1\n");
		errcnt += _check_message(fid, "LOG_TYPE_DEBUG2\n");
		errcnt += _check_message(fid, NULL);
		fclose(fid);
	}

	// Autoflush without flush
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	pmlog_rotate();
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_DEBUG1\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_DEBUG2\n");
	pmlog_autoflush(true, false);
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_TRACE1\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_TRACE2\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_TRACE3\n");
	pmlog_term();
	assert((fid = fopen(LOG_FILE_PATH_DFL, "r")) != NULL);
	if (fid) {
		errcnt += _check_message(fid, "LOG_TYPE_TRACE1\n");
		errcnt += _check_message(fid, "LOG_TYPE_TRACE2\n");
		errcnt += _check_message(fid, "LOG_TYPE_TRACE3\n");
		errcnt += _check_message(fid, NULL);
		fclose(fid);
	}

	// Autoflush with flush
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	pmlog_rotate();
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_DEBUG1\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_DEBUG2\n");
	pmlog_autoflush(true, true);
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_TRACE1\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_TRACE2\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_TRACE3\n");
	pmlog_term();
	assert((fid = fopen(LOG_FILE_PATH_DFL, "r")) != NULL);
	if (fid) {
		errcnt += _check_message(fid, "LOG_TYPE_DEBUG1\n");
		errcnt += _check_message(fid, "LOG_TYPE_DEBUG2\n");
		errcnt += _check_message(fid, "LOG_TYPE_TRACE1\n");
		errcnt += _check_message(fid, "LOG_TYPE_TRACE2\n");
		errcnt += _check_message(fid, "LOG_TYPE_TRACE3\n");
		errcnt += _check_message(fid, NULL);
		fclose(fid);
	}

	// Clear ring should clear entries
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	pmlog_rotate();
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_DEBUG1\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_DEBUG2\n");
	pmlog_clear_ring();
	pmlog_message(LOG_TYPE_DEBUG1, "LOG_TYPE_TRACE1\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_TRACE2\n");
	pmlog_message(LOG_TYPE_DEBUG2, "LOG_TYPE_TRACE3\n");
	pmlog_flush_ring();
	pmlog_term();
	assert((fid = fopen(LOG_FILE_PATH_DFL, "r")) != NULL);
	if (fid) {
		errcnt += _check_message(fid, "LOG_TYPE_TRACE1\n");
		errcnt += _check_message(fid, "LOG_TYPE_TRACE2\n");
		errcnt += _check_message(fid, "LOG_TYPE_TRACE3\n");
		errcnt += _check_message(fid, NULL);
		fclose(fid);
	}

	// Redirect stderr to a file
	fflush(stderr);
	// two open files to stderr, bak and 2
	bak = dup(2);
	// open the file
	new = open("/tmp/pmlog.stderr", O_CREAT | O_TRUNC | O_WRONLY, 0666);
	// duplicate new (the file) onto 2, closing old 2, then close new
	dup2(new, 2);
	close(new);
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	pmlog_rotate();
	pmlog_message(LOG_TYPE_MESSAGE, "Should appear in log\n");
	pmlog_stderr_set_level(1, -1);
	pmlog_message(LOG_TYPE_MESSAGE, "Should appear in log AND stderr\n");
	pmlog_logwrt(false);
	pmlog_message(LOG_TYPE_MESSAGE, "Should appear in stderr\n");
	pmlog_stderr_set_level(0, 0);
	pmlog_message(LOG_TYPE_MESSAGE, "Should appear in nowhere\n");
	pmlog_term();
	// duplicate bak (copy of stderr) onto 2, closing 2, then close bak
	fflush(stderr);
	dup2(bak, 2);
	close(bak);
	// check log file
	assert((fid = fopen(LOG_FILE_PATH_DFL, "r")) != NULL);
	if (fid) {
		errcnt += _check_message(fid, "Should appear in log\n");
		errcnt += _check_message(fid, "Should appear in log AND stderr\n");
		errcnt += _check_message(fid, NULL);
		fclose(fid);
	}
	// check stderr file
	assert((fid = fopen("/tmp/pmlog.stderr", "r")) != NULL);
	if (fid) {
		errcnt += _check_message(fid, "Should appear in log AND stderr\n");
		errcnt += _check_message(fid, "Should appear in stderr\n");
		errcnt += _check_message(fid, NULL);
		fclose(fid);
	}
	unlink("/tmp/pmlog.stderr");

	// Write a lot of records -- enough to rotate
	fmt = "ABCDEFGHIJKLMNOPQRST %d\n";
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	pmlog_rotate();
	for (i = 0; i < 32768; i++) {
		pmlog_message(LOG_TYPE_MESSAGE, fmt, i);
	}
	pmlog_term();

	// We can't be sure how many times we rotated, so search for first record
	for (N = 4; N >= 0; N--) {
		pmlog_path(name, sizeof(name), LOG_FILE_PATH_DFL, N);
		assert((fid = fopen(name, "r")) != NULL);
		if (fid) {
			if (0 == _verify_message(0, fid, fmt, 0)) {
				rewind(fid);
				break;
			}
			fclose(fid);
			fid = NULL;
		}
	}
	assert(N > 0);
	// The found file should now be open
	for (i = 0; N >= 0 && i < 32768; i++) {
		if (!fid)
			break;
		switch (_verify_message(1, fid, fmt, i)) {
		case -1:        // an actual error
			errcnt++;
			break;
		case 0:         // a success
			break;
		default:        // a premature EOF, roll to next file
			fclose(fid);
			fid = NULL;
			if (N == 0) {
				printf("Ran out of logs at i == %d\n", i);
				errcnt++;
				break;
			}
			pmlog_path(name, sizeof(name), LOG_FILE_PATH_DFL, --N);
			if (!(fid = fopen(name, "r"))) {
				printf("Could not open %s\n", name);
				errcnt++;
				break;
			}
			i--;    // retry last record
			break;
		}
	}
	if (fid) {
		fclose(fid);
	}
	return errcnt;
}

/**
 * Test writing to two separate log files.
 *
 * @return int - error count
 */
int
test_pmlog_twofiles(void)
{
	int errcnt = 0;
	void *ctxp;
	FILE *fid;
	int i;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Clear environment variables that will affect this test
	_clearenv();

	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0);
	ctxp = pmlog_init_new("/tmp/tmp.log", -1, -1, -1, 0);

	pmlog_rotate();
	pmlog_rotate_ctx(ctxp);
	for (i = 0; i < 10; i++) {
		pmlog_message(LOG_TYPE_MESSAGE, "File1 %d\n", i);
		pmlog_message_ctx(ctxp, LOG_TYPE_MESSAGE, "File2 %d\n", i);
	}
	pmlog_term();
	pmlog_term_ctx(ctxp);
	assert((fid = fopen(LOG_FILE_PATH_DFL, "r")) != NULL);
	if (fid) {
		for (i = 0; i < 10; i++) {
			errcnt += _check_message(fid, "File1 %d\n", i);
		}
		fclose(fid);
	}
	assert((fid = fopen("/tmp/tmp.log", "r")) != NULL);
	if (fid) {
		for (i = 0; i < 10; i++) {
			errcnt += _check_message(fid, "File2 %d\n", i);
		}
		fclose(fid);
	}
	unlink("/tmp/tmp.log");
	return errcnt;
}

/**
 * Read the log file and parse every line. This increments the error count on
 * any log file line that fails to parse, meaning a garbled log file line.
 *
 * @param name - name of the log file
 * @param count - pointer to returned line count, or NULL to ignore
 *
 * @return int - error count
 */
static int
_readbacklog(char *name, uint64_t *count)
{
	int errcnt = 0;
	FILE *fid;

	fid = fopen(name, "r");
	if (fid) {
		char buf[MAX_MSG_SIZE];
		int N = 0;

		while (fgets(buf, sizeof(buf), fid)) {
			int typ;
			_chop(buf);
			if (_msgparse(buf, 0, 0, 0, 0, &typ) == NULL) {
				printf("parse %s:%d '%s'\n", name, N, buf);
				errcnt++;
			}
			if (typ != LOG_TYPE_INTERNAL)
				N++;
		}
		fclose(fid);
		if (count)
			(*count) += N;
	} else {
		printf("open %s failed\n", name);
		errcnt++;
	}
	return errcnt;
}

/**
 * Child process for test_pmlog_processes().
 *
 * This simply dumps messages into into the log file until this child has
 * rotated it three times.
 *
 * @param instance - child instance number (0 - count-1)
 * @param count - total number of instances
 */
static void __attribute__((noreturn))
_child_proc(int instance, int count)
{
	const char *fmt = "ABCDEFGHIJKLMNOPQRST %d\n";
	struct timespec ns = { 0, 1000 };
	int rot = 0;
	int i;

	// leave enough headroom for count*3 rotations
	if (pmlog_init(NULL, 0, count * 4, 0, 0) == 0) {
		log_context_t *ctx = _get_context(NULL);
		// each process rotates three times
		for (i = 0; rot < 3; i++) {
			int rrr;
			pmlog_message(LOG_TYPE_MESSAGE, fmt, i);
			// yield to give some other processes a chance to run
			nanosleep(&ns, NULL);
			// snapshot of volatile 'rotations' count
			rrr = ctx->wrthread_data->rotations;
			// when it changes, display a message
			if (rrr != rot) {
				rot = rrr;
			}
		}
		pmlog_term();
	}
	exit(0);
}

/**
 * Test multiple concurrent application processes.
 *
 * This first clobbers all log files. It then spawns 'count' independent logging
 * processes, all of which compete to put messages in a common log file.
 *
 * After each thread has rotated files three times, they all stop, and should
 * leave (3*count)+1 log files present, which tests the contention for log
 * rotation: if this were to fail atomicity, we'd see extra log files (and some
 * would be very short).
 *
 * Finally, this reads through each log file, parsing the contents, and counts
 * an error for unparsable log entries, which represent log file corruption due
 * to competing logging processes.
 *
 * @param count - number of processes to test
 *
 * @return int - error count
 */
int
test_pmlog_processes(int count)
{
	int errcnt = 0;
	char name[PATH_MAX];
	struct stat sb;
	pid_t *pidarr;
	pid_t pid;
	int i, n;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Clear environment variables that will affect this test
	_clearenv();

	// Clobber all of the log files
	for (i = 0; i < count * 4; i++) {
		pmlog_path(name, sizeof(name), LOG_FILE_PATH_DFL, i);
		unlink(name);
	}
	// Spin children
	pidarr = calloc(count, sizeof(pid_t));
	n = 0;
	fflush(stderr);
	for (i = 0; i < count; i++) {
		fflush(stdout);
		switch ((pid = fork())) {
		case -1:        // failure
			printf("fork failed on child %d\n", i);
			break;
		case 0:         // child
			_child_proc(i, count);
			// noreturn function
		default:        // parent
			pidarr[n++] = pid;
			break;
		}
	}
	// wait for children
	for (i = 0; i < n; i++) {
		int status;
		waitpid(pidarr[i], &status, 0);
	}
	// check all logs for corruption
	for (i = 0; i <= 3 * count; i++) {
		pmlog_path(name, sizeof(name), LOG_FILE_PATH_DFL, i);
		errcnt += _readbacklog(name, NULL);
	}
	// check rotation
	pmlog_path(name, sizeof(name), LOG_FILE_PATH_DFL, i);
	assert(stat(name, &sb) == -1);
	free(pidarr);
	return errcnt;
}

// data structure for passing parameters to child thread
typedef struct {
	int instance;   // child instance number (0 - count-1)
	int count;      // total number of children
} child_data_t;

/**
 * Child thread for test_pmlog_threads().
 *
 * This simply dumps messages into into the log file until this thread has
 * seen a total of 3*count rotations.
 *
 * @param data - pointer to child_data_t structure for this thread
 *
 * @return void* - returned pointer to child_data_t structure
 */
static void *
_child_thread(void *data)
{
	child_data_t *td = (child_data_t *)data;
	const char *fmt = "ABCDEFGHIJKLMNOPQRST %d\n";
	struct timespec ns = { 0, 1000 };
	int rot = 0;
	int i;

	// leave enough headroom for count*3 rotations
	if (pmlog_init(NULL, 0, 4 * td->count, 0, 0) == 0) {
		log_context_t *ctx = _get_context(NULL);
		// each thread sees the same rotation count
		for (i = 0; rot < 3 * td->count; i++) {
			int rrr;
			pmlog_message(LOG_TYPE_MESSAGE, fmt, i);
			// yield to give some other threads a chance to run
			// use nanosleep() as in _child_process() for timing comparison
			nanosleep(&ns, NULL);
			// snapshot of volatile 'rotations' count
			rrr = ctx->wrthread_data->rotations;
			// when it changes, display a message
			if (rrr != rot) {
				rot = rrr;
			}
		}
		pmlog_term();
	}
	return data;
}

/**
 * Test multiple concurrent application threads.
 *
 * This first clobbers all log files. It then spawns 'count' independent logging
 * threads, all of which compete to put messages in a common log file.
 *
 * After logs have been rotated (3*count) times, threads all stop, and should
 * leave (3*count)+1 log files present, which tests the contention for log
 * rotation: if this were to fail atomicity, we'd see extra log files (and some
 * would be very short).
 *
 * Finally, this reads through each log file, parsing the contents, and counts
 * an error for unparsable log entries, which represent log file corruption due
 * to competing logging processes.
 *
 * @param count - number of threads to spawn
 *
 * @return int - error count
 */
int
test_pmlog_threads(int count)
{
	int errcnt = 0;
	char name[PATH_MAX];
	struct stat sb;
	child_data_t *tdata = NULL;
	GThread **tidarr = NULL;
	GThread *tid;
	int i, n;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Clear environment variables that will affect this test
	_clearenv();

	// Clobber all of the log files
	for (i = 0; i < count * 4; i++) {
		pmlog_path(name, sizeof(name), LOG_FILE_PATH_DFL, i);
		unlink(name);
	}
	// Spin threads
	tidarr = calloc(count, sizeof(GThread *));
	tdata = calloc(count, sizeof(child_data_t));
	n = 0;
	for (i = 0; i < count; i++) {
		tdata[i].count = count;
		tdata[i].instance = i;
		tid = g_thread_try_new("test thread", _child_thread, &tdata[i], NULL);
		if (tid) {
			tidarr[n++] = tid;
		}
	}
	// Wait for threads
	for (i = 0; i < n; i++) {
		g_thread_join(tidarr[i]);
	}
	// check all logs for corruption
	for (i = 0; i <= 3 * count; i++) {
		pmlog_path(name, sizeof(name), LOG_FILE_PATH_DFL, i);
		errcnt += _readbacklog(name, NULL);
	}
	// check rotation
	pmlog_path(name, sizeof(name), LOG_FILE_PATH_DFL, i);
	assert(stat(name, &sb) == -1);
	free(tdata);
	free(tidarr);
	return errcnt;
}

/**
 * Child process for testing signal flushing().
 *
 * This pushes ten debug messages into the ring buffer, then either raises a
 * fatal signal, or (on signal == 0) simply exits.
 *
 * @param signal - signal to terminate with, 0 for exit
 */
static void __attribute__((noreturn))
_exit_proc(int signal)
{
	const char *fmt = "ABCDEFGHIJKLMNOPQRST %d\n";
	int i;

	// Log ten debug messages
	for (i = 0; i < 10; i++) {
		pmlog_message(LOG_TYPE_DEBUG1, fmt, i);
	}
	// We expect signals to be fatal, if not, it's an error
	if (signal > 0) {
		raise(signal);
		printf("signal %d unexpected return to main code\n", signal);
		exit(1);
	}
	// On a signal of zero, print a fault and exit
	pmlog_message(LOG_TYPE_FAULT, "Forcing exit");
	exit(0);
}

/**
 * Test signals and exit conditions.
 *
 * This spins a child process that write debug messages, and then triggers
 * either a fatal signal or an exit() condition. If the signal is handled by
 * this code, we should see all messages in the log. If the signal is not
 * handled by this code, we should not see any messages.
 *
 * @param signal - fatal signal to test, or 0 to exit
 * @param expflush - if true, we expect a flush
 *
 * @return int - error count
 */
int
test_pmlog_exit(int signal, bool expflush)
{
	int errcnt = 0;
	int expcnt = (expflush) ? 11 : 0;
	uint64_t count = 0;
	char name[PATH_MAX];
	pid_t pid;
	int status;

	// Ensure static structures are initialized
	GLOBALINIT();

	// Clear environment variables that will affect this test
	_clearenv();

	// Clobber the current log file
	pmlog_path(name, sizeof(name), LOG_FILE_PATH_DFL, 0);
	unlink(name);

	// Spin a child to die in a particular way
	fflush(stderr);
	fflush(stdout);
	switch ((pid = fork())) {
	case -1:        // failure
		printf("fork failed on child\n");
		break;
	case 0:         // child
		_exit_proc(signal);
		// noreturn function
	default:        // parent
		break;
	}
	// wait for children
	waitpid(pid, &status, 0);
	// status == (exit_code << 8) | signal
	if (status != signal) {
		printf("expected child status %d, saw %d\n", signal, status);
		errcnt++;
	}
	// check all logs for corruption
	pmlog_path(name, sizeof(name), LOG_FILE_PATH_DFL, 0);
	errcnt += _readbacklog(name, &count);
	// note whether expected message count is seen
	if (count != expcnt) {
		printf("expected %d log entries, saw %ld\n", expcnt, count);
		errcnt++;
	}

	return errcnt;
}

/**
 * Test use of the PMLOG_ENABLE environment variable.
 *
 * @return int - error count
 */
int
test_pmlog_disable(void)
{
	int errcnt = 0;

	pmlog_term_all();       // enables logging
	unsetenv("PMLOG_ENABLE");
	pmlog_enable(-1);
	assert(pmlog_message(LOG_TYPE_CRITICAL, "Critical message") == 0, "#1");
	pmlog_term();
	// This should NOT disable logging
	setenv("PMLOG_ENABLE", "none", 1);
	assert(pmlog_message(LOG_TYPE_CRITICAL, "Critical message") == 0, "#2");
	pmlog_term();
	pmlog_enable(-1);
	assert(pmlog_message(LOG_TYPE_CRITICAL, "Critical message") == -1, "#3");
	assert(errno == ENOBUFS, "#3");
	pmlog_term();
	// This should NOT enable logging
	unsetenv("PMLOG_ENABLE");
	assert(pmlog_message(LOG_TYPE_CRITICAL, "Critical message") == -1, "#4");
	assert(errno == ENOBUFS, "#4");
	pmlog_term();

	unsetenv("PMLOG_ENABLE");
	pmlog_enable(-1);
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0, "#5");
	assert(pmlog_message(LOG_TYPE_CRITICAL, "Critical message") == 0, "#5");
	pmlog_term();
	// This should NOT disable logging
	setenv("PMLOG_ENABLE", "none", 1);
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0, "#6");
	assert(pmlog_message(LOG_TYPE_CRITICAL, "Critical message") == 0, "#6");
	pmlog_term();
	pmlog_enable(-1);
	assert(pmlog_init(NULL, 0, 0, 0, 0) == -1, "#7");
	assert(errno == ENOBUFS, "#7");
	assert(pmlog_message(LOG_TYPE_CRITICAL, "Critical message") == -1, "#7");
	assert(errno == ENOBUFS, "#7");
	pmlog_term();
	// This should NOT enable logging
	unsetenv("PMLOG_ENABLE");
	assert(pmlog_init(NULL, 0, 0, 0, 0) == -1, "#8");
	assert(errno == ENOBUFS, "#8");
	assert(pmlog_message(LOG_TYPE_CRITICAL, "Critical message") == -1, "#8");
	assert(errno == ENOBUFS, "#8");
	pmlog_term();
	pmlog_enable(-1);
	assert(pmlog_init(NULL, 0, 0, 0, 0) == 0, "#9");
	assert(_log_enable == LOG_ENABLE_DEFAULT, "#9");
	pmlog_term();

	return errcnt;
}


