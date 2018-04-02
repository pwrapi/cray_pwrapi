Test Harness Architecture
=========================

Top-Level
---------

**pwrtest** is a Python script that runs the entire test suite. It
searches the subsystems directory recursively to find test scripts, and
executes them in no particular order. It redirects output from these
scripts to individual test output files, one file per test. Upon
completion, it displays a summary of all tests, pass/fail status, and
other information. The output is in JSON format. Pass/fail status is 
determined by the exit code of individual tests (0 meaning PASS, anything 
else meaining FAIL.)

**postmortem** consumes the output of **pwrtest** and renders it in a more
useful format for developers. Other consumers could be developed for
different groups of people.

Common Code
-----------

subsystems/common/common.py contains generally useful functions, which are
described in greater detail below.

subsystems/common/cnctl.py contains functions commonly used by all cnctl
testing.

subsystems/common/pwrcmd.py contains function commonly used by all pwrcmd
testing.

### powerapid

 * ```def start_powerapid():```
 * ```def stop_powerapid():```
 * ```def restart_powerapid():```
 * ```def check_powerapid():```

   These use **systemctl** to start, stop, restart, and check the
powerapid daemon process.

### Test management

 * ```def init(test_name):```

   init() must be called at the start of each subsystem test script, to
name the test (normally just the test file name) and setup the test
environment.

 * ``` def skip_test(reason):```

   If a specific subsystem test cannot be run because (for instance) the
test system it is run on does not have certain features installed, or does
not have enough hardware to perform the test, this can be called to skip
the test rather than fail it. Skipped tests are reported separately from
the failures.

 * ```def test_name():```
 * ```def set_command(cmd_list):```
 * ```def get_command():```
 * ```def set_description(desc):```
 * ```def get_description():```

   Utility functions used to additionally refine the description of the
specific sub operations in a test, for use in the error messages and in
general output.

### Error reporting

 * ```def error_message(message, output=None, desc=None command=None):```

   This should be used (when possible) to display error messages in the
scripts, though tests can, in general, spew any kind of useful output
desired.

   The *message* parameter is the error message itself, and does not need
to include any "location" information about where the failure occurred. It
can be a simple error message, like "count <= 0".

   The optional *output* parameter can be output text from the underlying
command.

   The optional *desc* parameter is a test description, which is usually
passed as "desc=desc", where the second *desc* is a parameter passed to
the calling function. In this way, a test description can be passed
through multiple levels of calls.

   The optional *command* parameter is used to override the default in
certain cases. The default is the last command issued to the underlying
cnctl or pwrcmd application. In most cases, this is the desired value. The
parameter is provided for cases when it is not. Like *desc*, this is
passed through multiple levels of the code.

Cnctl Common Code
-----------------

Not yet documented.

Pwrcmd Common Code
------------------

 * ```def exec_pwrcmd(options, desc=None, expect=EXPECT_SUCCESS):```

   This is the workhorse of the pwrcmd testing code.

   *options* are passed as a list of command line arguments.

   *desc* is a brief (1-line) description of the test.

   *expect* should be EXPECT_SUCCESS, EXPECT_FAILURE, or an error code.

   Any failure of the **pwrcmd** application is a failure. The only normal
failures for the application are syntax errors, or fatal issues like OOM
conditions.

   The **PWR_ReturnCode** value returned by **pwrcmd** in the JSON output
will indicate whether the requested operation succeeded or failed, and
this is compared against the *expect* parameter. If it matches
expectations, the test is considered successful.

   The command returns the full JSON dict structure from **pwrcmd**
output.

 * ```def get_check_result(result, numvals):```

   This applies only to operations that get attributes. It checks the
result to ensure that the expected number of returned attributes and
timestamps are present in the output. It returns None if successful, and
an error string if it fails.

 * ```def check_status(result, numvals, errduples=None):```

   This applies to get and set attributes. It checks the returned status
to ensure that it is properly formatted.

   The *errduples* parameter is a list of duples of the form:
(*object_name*, *attr_name*)

   This must match the list of status responses, though not necessarily in
the same order.

 * ```def check_method(result, method):```

   This applies to get and set attributes. The **pwrcmd** output includes
a 'method' key that names the underlying API function that it used to
execute the command. Since the **pwrcmd** syntax can be subtle on this
point, the 'method' key provides a positive indication that we are testing
the function we intended to test.

 * ```def get_attr(attr, obj_name, errduples=None, method=None, desc=None,
	expect=EXPECT_SUCCESS):```

   This invokes the get attribute variants. The *desc* parameter should be used
to describe the specific subtest being performed. This will automatically
call get_check_result() and check_status(), and will call check_method()
if the method is specified.

   The *attr* and *obj_name* parameters can be scalars, or lists. Behavior
of the operation depends according to the following:

   <table>
     <tr><th> attr   </th><th> obj_name </th><th> API call               </th></tr>
     <tr><td> scalar </td><td> scalar   </td><td> PWR_ObjAttrGetValue()  </td></tr>
     <tr><td> list   </td><td> scalar   </td><td> PWR_ObjAttrGetValues() </td></tr>
     <tr><td> scalar </td><td> list     </td><td> PWR_GrpAttrGetValue()  </td></tr>
     <tr><td> list   </td><td> list     </td><td> PWR_GrpAttrGetValues() </td></tr>
   </table>

 * ```def set_attr(attr, obj_name, value, role, errduples=None,
	method=None, desc=None, expect=EXPECT_SUCCESS):```

   This invokes the set attribute variants. The *desc* parameter should
be used to describe the specific subtest being performed. This will
automatically call check_status(), and will call check_method() if the
method is specified.


 * ```def set_attr_and_check(attrs, objs, values, role, errduples=None,
	method=None, desc=None, expect=EXPECT_SUCCESS):```

   The *attr* and *obj_name* parameters can be scalars, or lists. Behavior
of the operation depends according to the following:

   <table>
     <tr><th> attr   </th><th> obj_name </th><th> API call               </th></tr>
     <tr><td> scalar </td><td> scalar   </td><td> PWR_ObjAttrSetValue()  </td></tr>
     <tr><td> list   </td><td> scalar   </td><td> PWR_ObjAttrSetValues() </td></tr>
     <tr><td> scalar </td><td> list     </td><td> PWR_GrpAttrSetValue()  </td></tr>
     <tr><td> list   </td><td> list     </td><td> PWR_GrpAttrSetValues() </td></tr>
   </table>

 * ```def list_attr(expect=EXPECT_SUCCESS):```

   This generates a list of attributes supported.

 * ```def list_name(filter=None, expect=EXPECT_SUCCESS):```

   This generates a list of objects in the hierarchy.

   If the *filter* parameter is None, this generates a list of all names.

   If the *filter* parameter is specified, it should be a regular
expression that will be used to match names. You can use a regular
expression like "^core" to find all core object names, for example.


Simulated Environment
---------------------

To test in a simulated power environment, using ordinary files instead of
the real /sys files, perform the following steps:

1. Execute **./makerpms install** in your build environment.
2. Execute **cd /opt/cray/powerapi/default/bin**.
3. Execute **mv test/sysfiles.cfg test/sysfiles.cfg.bak**
4. Execute **echo "chroot = /tmp" >test/sysfiles.cfg**
5. Execute **./makesys**.
6. Execute **./pwrtest | ./postmortem**.

Logging
-------

Logging can be controlled through environment variables.

PMLOG_ENABLE can be set to any of the following values:

<table>
  <tr><th> Env Var value </th><th> Enum value </th><th> Meaning </th></tr>
  <tr><td> "none"     </td><td> PMLOG_ENABLE_NONE    </td><td> disabled </td></tr>
  <tr><td> "default"  </td><td> PMLOG_ENABLE_DEFAULT </td><td> TRACE3, TRACE2, DEBUG2 suppressed </td></tr>
  <tr><td> "full"     </td><td> PMLOG_ENABLE_FULL    </td><td> enabled fully </td></tr>
</table>

This value is sampled once at application start, and can be refreshed from
the environment variable by an explicit call to pmlog_enable(-1).
pmlog_enable() can also be called with the desired enum value directly.

The following parameters can be used to set logging characteristics:

<table>
  <tr><th> Environment Variable </th><th> Default Value </th></tr>
  <tr><td> PMLOG_FILE_PATH      </td><td> /var/opt/cray/powerapi/log/powerapi.log </td></tr>
  <tr><td> PMLOG_MAX_FILE_SIZE  </td><td> 1 MiB   </td></tr>
  <tr><td> PMLOG_MAX_FILE_COUNT </td><td> 5       </td></tr>
  <tr><td> PMLOG_NUM_RINGS      </td><td> 2       </td></tr>
  <tr><td> PMLOG_RING_SIZE      </td><td> 256 KiB </td></tr>
  <tr><td> PMLOG_DEBUG_LEVEL    </td><td> 0       </td></tr>
  <tr><td> PMLOG_TRACE_LEVEL    </td><td> 0       </td></tr>
</table>

PMLOG_FILE_PATH sets the name of the log file.

PMLOG_MAX_FILE_SIZE sets the maximum (approximate) size the log file can
achieve before a log file rotation occurs, which moves the log file to a
numbered suffix (starting with ".1") and opens a new empty log file. If
this is set to -1, automatic log rotation is turned off and the log file
will grow without bound. It is still possible to rotate logs using
pmlog_rotate().

PMLOG_MAX_FILE_COUNT sets the maximum number of files maintained in the
rotation. Once this value is reached the oldest log file will be deleted
during rotation. If this is set to -1, log rotation is prohibited, and the
log file will grow without bound.

PMLOG_NUM_RINGS specifies the number of ring buffers to maintain. This
should rarely be changed to any value other than 2. If this is set to -1,
ring buffers are disabled, which will disable any error logging, though it
will still permit informational message logging.

PMLOG_RING_SIZE specifies the number of bytes in each ring buffer. This
needs to be enough space to capture all log messages of interest in the
event of an error. If this is set to any value less than 4096, the value
of 4096 will be used.

PMLOG_DEBUG_LEVEL can be set to 1 or 2, which controls the relative
verbosity of debug messages issued to stderr. The default value of zero
does not display debug messages to stderr.

PMLOG_TRACE_LEVEL can be set to 1, 2, or 3, which controls the relative
verbosity of trace messages issued to stderr. The default value of zero
does not display trace messages to stderr.

A full trace of all operations is logged to ring buffers in memory. These
can be manually flushed by calling pmlog_flush(), but they are normally
flushed only when an error occurs. When an error occurs, all messages in
the ring buffer, including all debug and trace messages at the maximum
verbosity, are dumped to the log file. There is no provision to control
the verbosity of what is dumped to the file: all messages are always
written.

This has two major benefits.

First, all logs will contain the highest level of debugging information
leading up to a failure.

Second, there is no need to "raise the logging level" to see an error,
which in performance oriented code will often slow down operations enough
to make an intermittent problem vanish completely.
