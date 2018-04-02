#!/usr/bin/python
#
# Copyright (c) 2016-2017, Cray Inc.
# All rights reserved.

## @package common
#  Common routines used by all subsystem tests.

import collections
import json
import os
import sys

from subprocess import Popen, PIPE, STDOUT

def systemctl_powerapid(command):
    cmd = ["/usr/bin/systemctl", command, "powerapid"]
    p = Popen(cmd, stdout=PIPE, stderr=STDOUT, bufsize=-1)
    p.communicate()
    return p.returncode == 0

def start_powerapid():
    if not systemctl_powerapid("status"):
        if not systemctl_powerapid("start"):
            print test_name() + ": FAIL: powerapid not startable"
            sys.exit(3)

def stop_powerapid():
    if systemctl_powerapid("status"):
        if not systemctl_powerapid("stop"):
            print test_name() + ": FAIL: powerapid not stoppable"
            sys.exit(3)

def restart_powerapid():
    if systemctl_powerapid("status"):
        if not systemctl_powerapid("restart"):
            print test_name() + ": FAIL: powerapid not restartable"
            sys.exit(3)

def check_powerapid():
    if not systemctl_powerapid("status"):
        print test_name() + ": FAIL: powerapid not running"
        sys.exit(3)

#
# init - Common initialization for Python tests
#
def init(name):
    #
    # Set the test name
    #
    global test_name_text
    global last_command
    global last_description
    test_name_text = name
    last_command = None
    last_description = None

    #
    # Ensure debugging turned off as it'll cause failures
    #
    os.environ['PWR_TRACE_LEVEL'] = "0"
    os.environ['PWR_DEBUG_LEVEL'] = "0"

    #
    # Ensure that powerapid is running
    #
    start_powerapid()

#
# test_name - Get the test name
#
def test_name():
    global test_name_text
    return test_name_text

def set_command(cmd_list):
    """Preserve the command in global storage"""
    global last_command
    last_command = ' '.join(cmd_list)

def get_command():
    """Get preserved command"""
    global last_command
    return last_command if last_command else "(No Command)"

def set_description(desc):
    """Preserve the description in global storage"""
    global last_description
    last_description = desc

def get_description():
    """Get preserved description"""
    global last_description
    return last_description if last_description else "(No Description)"

def error_message(message, output=None, desc=None, command=None):
    """Format an error message"""
    global test_name_text
    errmsg = ""
    errmsg += "FAIL {}: {}\n".format(test_name_text, message)
    errmsg += "  dsc={}\n".format(desc if desc else get_description())
    errmsg += "  cmd={}\n".format(command if command else get_command())
    if output:
        errmsg += "output==========================\n"
        errmsg += output
        errmsg += "\n================================\n"
    return errmsg

def skip_test(reason):
    """Skip an entire test"""
    global test_name_text
    print "SKIP {}: {}\n".format(test_name_text, reason)
    sys.exit(0)


def skip_test_if_not_root():
    """Returns True if you are effectively root, and if not, returns False.
    """
    if os.geteuid() != 0:
        skip_test("root permissions required for test")
