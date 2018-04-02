#!/usr/bin/python
#
# Copyright (c) 2016, Cray Inc.
# All rights reserved.

## @package cnctl
#  Common routines used by cnctl subsystem tests.

import collections
import json
import os
import sys

from common import test_name
from subprocess import Popen, PIPE, STDOUT

#
# exec_cnctl - Execute cnctl with the specified options
#
def exec_cnctl(options):
    cnctl_cmd = ["/opt/cray/powerapi/default/bin/cnctl"]
    cnctl_cmd.extend(options)

    p = Popen(cnctl_cmd, stdout=PIPE, stderr=STDOUT,
              bufsize=-1)

    cnctl_output = p.communicate()

    if p.returncode != 0:
        print test_name() + ": FAIL: exec_cnctl: Popen(" + \
            str(cnctl_cmd) + ") returned " + str(p.returncode) + \
            ", Output = " + cnctl_output[0],
        sys.exit(100)

    output = json.loads(cnctl_output[0])

    return output

#
# exec_cnctl_json - Execute cnctl with the specified JSON input
#
def exec_cnctl_json(json_input):
    cnctl_cmd = ["/opt/cray/powerapi/default/bin/cnctl"]
    options = ["--json"]
    cnctl_cmd.extend(options)

    p = Popen(cnctl_cmd, stdin=PIPE, stdout=PIPE, stderr=STDOUT,
              bufsize=-1)

    cnctl_output = p.communicate(input=json_input)

    if p.returncode != 0:
        print test_name() + ": FAIL: exec_cnctl: Popen(" + \
            str(cnctl_cmd) + ") returned " + str(p.returncode) + \
            ", Output = " + cnctl_output[0],
        sys.exit(100)

    output = json.loads(cnctl_output[0])

    return output

#
# list_attr - Call cnctl to list the attributes supported
#
def list_attr():
    options = ["--command", "cap",
               "--json"]

    output = exec_cnctl(options)

    return output

