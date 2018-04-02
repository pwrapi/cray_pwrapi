/*
 * Copyright (c) 2017, Cray Inc.
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
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/time.h>

/*
 * Define this macro to completely disable the logging code. This does not
 * actually prevent compilation of the code, but it ensures that:
 *
 *   - LOG_ENABLE_NONE is always used (it cannot be changed)
 *   - LOG_* and TRACE_* macros evaluate to a no-op static inline function
 *
 * Thus, all direct calls into the pmlog_*() API will return immediately with no
 * operation performed, and all LOG_* and TRACE_* macros will be optimized out
 * of the code.
 */
#undef	PMLOG_NOCOMPILE

/*
 * Default values.
 */
#define	LOG_FILE_PATH_DFL	"/var/opt/cray/powerapi/log/powerapi.log"
#define	LOG_FILE_SIZE_DFL	((int64_t)  1*1024*1024	)
#define	LOG_FILE_COUNT_DFL	((int64_t)  5		)
#define	LOG_NUM_RINGS_DFL	((int64_t)  2		)
#define	LOG_RING_SIZE_DFL	((int64_t)  256*1024	)

/*
 * NOTE: the following values can be deprecated or extended, but they may not be
 * reordered or deleted, else historical logs may become unparseable.
 */
typedef enum {
	LOG_TYPE_CONSOLE = 0,   // console only
	LOG_TYPE_INTERNAL,      // logger messages only
	LOG_TYPE_MESSAGE,       // informational message
	LOG_TYPE_CRITICAL,      // critical message (about to exit)
	LOG_TYPE_WARNING,       // warning message (smell of dead fish)
	LOG_TYPE_FAULT,         // failure message (actual error)
	LOG_TYPE_TRACE1,        // top-level trace
	LOG_TYPE_TRACE2,        // mid-level trace
	LOG_TYPE_TRACE3,        // low-level trace
	LOG_TYPE_DEBUG1,        // normal debug
	LOG_TYPE_DEBUG2,        // high-frequency debug
	NUM_LOG_TYPES
} LOG_TYPES;

typedef enum {
	LOG_ENABLE_NONE = 0,    // disable all logging
	LOG_ENABLE_DEFAULT,     // enable logging of TRACE1 and DEBUG1
	LOG_ENABLE_FULL,        // enable logging of all TRACE and DEBUG
	NUM_LOG_ENABLES
} LOG_ENABLES;

/*
 * These are used for stderr level logging.
 */
#define	LOG_MASK_TRACE	\
		((1 << LOG_TYPE_TRACE1) |\
		 (1 << LOG_TYPE_TRACE2) |\
		 (1 << LOG_TYPE_TRACE3))

#define	LOG_MASK_DEBUG	\
		((1 << LOG_TYPE_DEBUG1) |\
		 (1 << LOG_TYPE_DEBUG2))

int64_t getenvzero(const char *name);

void *pmlog_init_new(const char *log_path,
		int64_t max_size, int64_t max_files,
		int64_t num_rings, int64_t ring_size);
void *pmlog_init_ctx(void *ctxp, const char *log_path,
		int64_t max_size, int64_t max_files,
		int64_t num_rings, int64_t ring_size);
void pmlog_sync_ctx(void *ctxp);
void pmlog_term_ctx(void *ctxp);
void pmlog_term_all(void);
int pmlog_enable(int enable);

bool pmlog_autoflush_ctx(void *ctxp, bool enable, bool flush);
bool pmlog_logwrt_ctx(void *ctxp, bool enable);
int pmlog_stderr_set_level_ctx(void *ctxp, int D_level, int T_level);
int pmlog_stderr_get_level_ctx(void *ctxp, int *D_level, int *T_level);
void pmlog_flush_ring_ctx(void *ctxp);
void pmlog_clear_ring_ctx(void *ctxp);
void pmlog_rotate_ctx(void *ctxp);

extern int __attribute__((__format__(__printf__, 3, 4))) \
		pmlog_message_ctx(void *ctxp, int msg_type, const char *fmt, ...);

char *pmlog_path(char *buf, size_t siz, const char *bas, int N);
char *pmlog_parse(char *msg, struct timeval *tv, char *appname,
		pid_t *pid, pid_t *tid, int *msgtype);

/*
 * These defines call the context-dependent functions with a context of NULL,
 * which is the normal debug/trace/error logging context.
 */
#define	pmlog_init(a...)	(pmlog_init_ctx(NULL, ## a) ? 0 : -1)
#define	pmlog_sync()		pmlog_sync_ctx(NULL)
#define	pmlog_term()		pmlog_term_ctx(NULL)
#define	pmlog_autoflush(a...)	pmlog_autoflush_ctx(NULL, ## a)
#define	pmlog_logwrt(a...)	pmlog_logwrt_ctx(NULL, ## a)
#define	pmlog_stderr_set_level(a...)	pmlog_stderr_set_level_ctx(NULL, ## a)
#define	pmlog_stderr_get_level(a...)	pmlog_stderr_get_level_ctx(NULL, ## a)
#define	pmlog_flush_ring()	pmlog_flush_ring_ctx(NULL)
#define	pmlog_clear_ring()	pmlog_clear_ring_ctx(NULL)
#define	pmlog_rotate()		pmlog_rotate_ctx(NULL)

/*
 * This conditional define is what eliminates calls into the logging system when
 * PMLOG_NOCOMPILE is defined for all of the LOG_* and TRACE_* macros.
 */
#ifdef	PMLOG_NOCOMPILE
static inline int pmlog_message_nop(void *ctxp, ...) { return 0;}
#define	pmlog_message(a...)	pmlog_message_nop(NULL, ##a)
#else
#define	pmlog_message(a...)	pmlog_message_ctx(NULL, ##a)
#endif

// Note that all macros append LF to message
#define	_LOG(L, f, a...)	pmlog_message(L, "[%s:%d] " f "\n", __func__, __LINE__, ## a)

#define LOG_CONS(f, a...)	_LOG(LOG_TYPE_CONSOLE, f, ## a)
#define LOG_MSG(f, a...)	_LOG(LOG_TYPE_MESSAGE, f, ## a)
#define LOG_CRIT(f, a...)	_LOG(LOG_TYPE_CRITICAL, f, ## a)
#define LOG_WARN(f, a...)	_LOG(LOG_TYPE_WARNING, f, ## a)
#define LOG_FAULT(f, a...)	_LOG(LOG_TYPE_FAULT, f, ## a)
#define LOG_DBG(f, a...)	_LOG(LOG_TYPE_DEBUG1, f, ## a)
#define LOG_VRB(f, a...)	_LOG(LOG_TYPE_DEBUG2, f, ## a)

#define TRACE1_ENTER(f, a...)	_LOG(LOG_TYPE_TRACE1, "[ENTER] " f, ## a)
#define TRACE1_EXIT(f, a...)	_LOG(LOG_TYPE_TRACE1, "[EXIT] " f, ## a)
#define TRACE2_ENTER(f, a...)	_LOG(LOG_TYPE_TRACE2, "[ENTER] " f, ## a)
#define TRACE2_EXIT(f, a...)	_LOG(LOG_TYPE_TRACE2, "[EXIT] " f, ## a)
#define TRACE3_ENTER(f, a...)	_LOG(LOG_TYPE_TRACE3, "[ENTER] " f, ## a)
#define TRACE3_EXIT(f, a...)	_LOG(LOG_TYPE_TRACE3, "[EXIT] " f, ## a)

/*
 * Usage notes:
 *
 * - Type     Macro      Console Ring    File    Flush   Text
 * - CONSOLE  LOG_CONS   yes     no      no      no      CONS
 * - INTERNAL <none>     no      no      yes     no      LOGR
 * - MESSAGE  LOG_MSG    no      no      yes     no      MESG
 * - CRITICAL LOG_CRIT   yes     yes     yes     yes     CRIT
 * - WARNING  LOG_WARN   yes     yes     yes     yes     WARN
 * - FAULT    LOG_FAULT  yes     yes     yes     yes     FAIL
 * - DEBUG1   LOG_DBG    no      yes     flush   no      DBG1
 * - DEBUG2   LOG_VRB    no      yes     flush   no      DBG2
 * - TRACE*   TRACE*     no      yes     flush   no      TRC*
 * - *        <none>     no      yes     flush   no      USR{n}
 *
 * Messages appear in the log tagged with the string that appears in the Text
 * column above. This can be used to selectively filter different messages in
 * the log. For instance, you can grep through the log for 'WARN' conditions,
 * which might show why an application is starting to show odd results.
 *
 * LOG_CONS can be used for special cases, where we want to send a message to
 * the console without appearing in the logs.
 *
 * LOG_TYPE_INTERNAL messages are generated by the logging system itself, and
 * are like LOG_MSG, but with a different message type code. They are reserved
 * for use by the logging system.
 *
 * LOG_MSG is intended for logging "comments" in the log, that aren't part of
 * any error tracing or handling. It's for informational messages, like the
 * current version of the code, or application start/end. These appear ONLY in
 * the log file.
 *
 * LOG_CRIT is reserved for library code that is going to call exit(), thus
 * unexpectedly terminating the application from within the pmlog library. These
 * should not be used for any other purpose. In mature code, they should never
 * happen. These are also sent to the console.
 *
 * LOG_WARN is reserved for unusual conditions that don't cause overt errors,
 * but which could represent conditions that prevent the code from running
 * properly. These are unusual, but potentially harmless conditions. If they
 * represent a definite failure, the library code should be changed to return an
 * error condition, and the message should be changed to LOG_FAULT. Warnings are
 * also sent to the console.
 *
 * LOG_FAULT is the general-purpose error reporting macro. These indicate that
 * something went wrong -- typically, the library call returns a failure
 * condition with an error code, and this message can supplement the
 * understanding of that error, since the error codes can be ambiguous. These
 * are also sent to the console. These should NOT be used for "normal" errors
 * that result in a recovery action by the library code, such as (for instance)
 * failing to open a file, and then creating it.
 *
 * LOG_DBG is the general-purpose debugging trace message, and can be used
 * librarally for any purpose. These are written ONLY to the ring buffer, and
 * older messages in the ring are overwritten. These will appear in the log file
 * only if the ring is flushed. These are NEVER written to the console.
 *
 * LOG_VRB is a differentiated debugging trace message for extremely verbose
 * messages, such as those that appear inside a loop.
 *
 * The different TRACE* macros provides a hierarchy of three levels of tracing
 * to allow the code flow to be examined in increasing levels of detail.
 *
 * The application developer can extend this list of types to add more
 * 'debugging' messages by calling pmlog_message() directly. These will appear
 * with USR{n} in the log messages, where {n} is the decimal value of the
 * extended type.
 *
 * When autoflush is enabled, explicitly through pmlog_autoflush(), or by
 * setting the stderr debug/trace levels to non-zero values, the ring buffer is
 * bypassed, and ALL debug and trace messages go to the log. The messages
 * delivered to stderr (if any) are filtered by the level settings. Because
 * every message must be flushed to disk (and stderr), this can slow down the
 * application.
 *
 * Output of the macros in the log consists of six space-delimited header
 * fields, followed by a free-form message:
 *
 * 2017/02/24-09:07:42.131932 logging 23659 23660 LOGR [pmlog_write_thread:1849] Logging closed
 *
 * {timestamp} {appname} {pid} {tid} {msgtype} {[func:line]} {message...}
 */

#endif	// __LOG_H__


