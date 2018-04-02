#!/usr/bin/python
#
# Copyright (c) 2016-2017, Cray Inc.
# All rights reserved.

## @package pwrcmd
#  Common routines used by pwrcmd subsystem tests.

import collections
import json
import os
import re
import sys

from common import test_name, set_command, set_description, get_command, \
    get_description, error_message
from subprocess import Popen, PIPE, STDOUT

#
# These must be above the range of possible PWR_ReturnCode values
#
EXPECT_SUCCESS = 101
EXPECT_FAILURE = 102

def get_check_result(result, numvals):
    """Check the result of a 'get' operation"""
    if not isinstance(result, dict):
        return "pwrcmd output is not a dict"
    # Some errors return only the error itself
    if (result['PWR_ReturnCode'] != 0 and
            "attr_vals" not in result and
            "timestamps" not in result):
        return None
    # attr_vals must exist as a list with numvals elements
    if "attr_vals" not in result:
        return "'attr_vals' not found"
    if not isinstance(result['attr_vals'], list):
        return "'attr_vals' is not a list"
    if len(result['attr_vals']) != numvals:
        return "expected {} attr_vals".format(numvals)
    # timestamps must exist as a list with numvals elements
    if "timestamps" not in result:
        return "'timestamps' not found"
    if not isinstance(result['timestamps'], list):
        return "'timestamps' is not a list"
    if len(result['timestamps']) != numvals:
        return "expected {} timestamps".format(numvals)
    # status must exist
    if "status" not in result:
        return "'status' not found"
    return None

def check_meta_result(result, expected_value=None):
    """Check the result of a 'get' metadata operation
    """
    if not isinstance(result, dict):
        return "pwrcmd output is not a dict"
    # Some errors return only the error itself
    if result['PWR_ReturnCode'] != 0 and "value" not in result:
        return None
    # value must exist as if PWR_ReturnCode is 0 (successful 'get')
    if "value" not in result:
        return "'value' not found"
    if expected_value is not None:
        if str(result['value']) != str(expected_value):
            return "Result value ({}) did not match expected ({})".format(result['value'], expected_value)
    return None

def check_status(result, numvals, errduples=None):
    """Check the status of a 'get' or 'set' operation"""
    # If no errduples at all, don't test
    if errduples is None:
        return None

    # If there are no errors expected, status must be None
    numerrs = len(errduples)
    if numerrs == 0:
        if result['status'] is not None:
            return "expected 'status' is None"
        return None

    # Otherwise, status must be a list with numerrs elements
    if not isinstance(result['status'], list):
        return "result['status'] is not a list"
    if len(result['status']) != numerrs:
        return "expected {} status entries".format(numerrs)

    # Check all of the status values against the expected list
    idx = 0
    for stat in result['status']:
        # Each status must contain these keys
        if not "object" in stat:
            return "'object' not found in status[{}]".format(idx)
        if not "attr" in stat:
            return "'attr' not found in status[{}]".format(idx)
        if not "index" in stat:
            return "'index' not found in status[{}]".format(idx)
        if not "error" in stat:
            return "'error' not found in status[{}]".format(idx)
        # The error code should not be zero
        if stat['error'] == 0:
            return "status[{}]['error'] == 0".format(idx)
        # There is no guarantee of the order in which values are returned
        # because there is a mismatch between our pwrcmd implementation
        # (a serialized list of objects in the group) and the actual
        # group itself, which has an internal sorting order that may be
        # different from the order in which we built the group. So we
        # have to search the list of expected errors to find one that
        # matches.
        errdup = (stat['object'], stat['attr'])
        for dup in errduples:
            # If we find it, good
            if dup == errdup:
                break
        else:
            # We finished the for loop, not found
            return "unexpected error in status[{}]".format(idx)
        # Index location must be within range of returned values
        index = stat['index']
        if index not in range(0, numvals):
            return "status[{}]['index'] out of range".format(idx)
        idx += 1
    return None

def check_method(result, method):
    """Compare given method to method listed in the result, if any.
    """
    if method is None:
        return None
    if 'method' in result and result['method'] != method:
        return "wrong method {}".format(result['method'])
    return None

#
# exec_pwrcmd - Execute pwrcmd with the specified options
#
def exec_pwrcmd(options, desc=None, expect=EXPECT_SUCCESS):
    """Execute a 'pwrcmd' command"""
    pwrcmd_cmd = ["/opt/cray/powerapi/default/bin/pwrcmd"]
    pwrcmd_cmd.extend(options)

    set_command(pwrcmd_cmd)
    set_description(desc)

    # Execute the command
    p = Popen(pwrcmd_cmd, stdout=PIPE, stderr=STDOUT, bufsize=-1)
    pwrcmd_output = p.communicate()

    # pwrcmd itself should not fail -- if it does, it's fatal
    if p.returncode != 0:
        print error_message(
            "Popen() returncode = {}".format(p.returncode),
            pwrcmd_output[0])
        sys.exit(100)

    # All output should be JSON -- if not, it's fatal
    try:
        output = json.loads(pwrcmd_output[0])
    except:
        print error_message("json.loads() failed", pwrcmd_output[0])
        sys.exit(101)

    # All output should include PWR_ReturnCode
    if "PWR_ReturnCode" not in output:
        print error_message(
            "PWR_ReturnCode not found",
            pwrcmd_output[0])
        sys.exit(102)

    # Evaluate PWR_ReturnCode against expectations
    actcode = int(output["PWR_ReturnCode"])
    if expect == EXPECT_SUCCESS and actcode != 0:
        failtxt = "{} expected success, saw retcode {}".format(
            "PWR_ReturnCode", actcode)
    elif expect == EXPECT_FAILURE and actcode == 0:
        failtxt = "{} expected failure, saw success".format(
            "PWR_ReturnCode")
    elif expect < EXPECT_SUCCESS and actcode != expect:
        failtxt = "{} expected retcode {}, saw retcode {}".format(
            "PWR_ReturnCode", expect, actcode)
    else:
        failtxt = None
    if failtxt:
        print error_message(failtxt, pwrcmd_output[0])
        sys.exit(103)

    # Return the dict structure parsed from JSON
    return output

#
# get_attr - Call pwrcmd to get the attribute value of the specified
#            attribute/object
#
def get_attr(attr, obj_name, errduples=None, method=None,
             desc=None, expect=EXPECT_SUCCESS):
    """Get one or more attributes"""
    if not isinstance(attr, list):
        attr = [attr]
    if not isinstance(obj_name, list):
        obj_name = [obj_name]

    attr_cnt = len([x for x in attr if x != ''])
    obj_cnt = len([x for x in obj_name if x != ''])
    numvals = attr_cnt * obj_cnt

    attr = ','.join(attr)
    obj_name = ','.join(obj_name)

    options = ["--command", "get",
               "--attribute", attr,
               "--name", obj_name]

    result = exec_pwrcmd(options, desc=desc, expect=expect)
    errmsg = get_check_result(result, numvals)
    if errmsg:
        print error_message(
            "check_result failed: {}".format(errmsg),
            json.dumps(result, indent=2))
        sys.exit(101)
    errmsg = check_status(result, numvals, errduples)
    if errmsg:
        print error_message(
            "check_status failed: {}".format(errmsg),
            json.dumps(result, indent=2))
        sys.exit(102)

    errmsg = check_method(result, method)
    if errmsg:
        print error_message(
            "check_method failed: {}".format(errmsg),
            json.dumps(result, indent=2))
        sys.exit(103)

    return result

#
# set_attr - Call pwrcmd to set the attribute value of the specified
#            attribute/object
#
def set_attr(attr, obj_name, value, role, errduples=None, method=None,
             desc=None, expect=EXPECT_SUCCESS):
    """Set one or more attributes"""
    if not isinstance(attr, list):
        attr = [attr]
    if not isinstance(obj_name, list):
        obj_name = [obj_name]
    if not isinstance(value, list):
        value = [value]

    attr_cnt = len([x for x in attr if x != ''])
    obj_cnt = len([x for x in obj_name if x != ''])
    numvals = attr_cnt * obj_cnt

    attr = ','.join(attr)
    obj_name = ','.join(obj_name)
    value = ','.join([str(val) for val in value])

    options = ["--command", "set",
               "--attribute", attr,
               "--name", obj_name,
               "--value", value,
               "--role", role]

    result = exec_pwrcmd(options, desc=desc, expect=expect)
    errmsg = check_status(result, numvals, errduples)
    if errmsg:
        print error_message(
            "check_status failed: {}".format(errmsg),
            json.dumps(result, indent=2))
        sys.exit(102)
    errmsg = check_method(result, method)
    if errmsg:
        print error_message(
            "check_method failed: {}".format(errmsg),
            json.dumps(result, indent=2))
        sys.exit(103)

    return result

def set_attr_and_check(attrs, objs, values, role, errduples=None, method=None,
                       desc=None, expect=EXPECT_SUCCESS):
    """Perform a set/get/reset operation on one or more attributes"""

    # Build this up as we go

    # We want these as lists-of-one, not as scalars
    if not isinstance(attrs, list):
        attrs = [attrs]
    if not isinstance(objs, list):
        objs = [objs]
    if not isinstance(values, list):
        values = [values]
    print "  set    values = {}".format(values)

    # This will have attributes * objects values in it
    result = get_attr(attrs, objs, desc=desc + " get orig")
    orig_values = result['attr_vals']
    print "  orig   values = {}".format(orig_values)
    print get_command()

    # There is no API call to set a heterogeneous collection
    # of attribute values for multiple objects. Instead, we
    # have to set multiple attribute values for each object.
    # We need to slice this into
    # [ [o1v1,o1v2,o1v3,...],
    #   [o2v1,o2v2,o2v3,...], ... ]
    numattrs = len(attrs)
    numvals = len(orig_values)
    reset_values = [orig_values[i:i + numattrs] for i in range(0, numvals,
                                                               numattrs)]
    print "  reset  values = {}".format(reset_values)

    # This is what we expect actual_values to be
    # Values for set follow attribute count: 1 value for 1 attribute
    # Values for get expand as attributes * objects
    # This produces [o1v1,o1v2,o1v3,...o2v1,o2v2,o2v3,...]
    expect_values = [val for obj in objs for val in values]
    print "  expect values = {}".format(expect_values)

    # Perform the set
    set_attr(attrs, objs, values, role, errduples, desc=desc + " set new",
             method=method, expect=expect)
    # Record this command and description for errors
    cmd = get_command()
    dsc = get_description()

    # Read attributes * objects values after set
    result = get_attr(attrs, objs, desc=desc + " get check new")
    actual_values = result['attr_vals']
    print "  actual values = {}".format(actual_values)

    # To put the originals back, we need to walk through reset_values
    i = 0
    for obj in objs:
        set_attr(attrs, obj, reset_values[i], role, desc=desc
                 + " reset orig")
        i += 1

    # Read back after reset, attributes * objects values
    result = get_attr(attrs, objs, desc=desc + " get check orig")
    final_values = result['attr_vals']
    print "  final  values = {}".format(final_values)

    # Want some indication of what went wrong
    text = "\n".join(
        ["  set    values = {}".format(values),
         "  orig   values = {}".format(orig_values),
         "  expect values = {}".format(expect_values),
         "  actual values = {}".format(actual_values),
         "  final  values = {}".format(final_values)])
    if actual_values != expect_values:
        print error_message("new values != expected values",
                            output=text, desc=dsc, command=cmd)
        return False
    if orig_values != final_values:
        print error_message("final values != orig values",
                            output=text, desc=dsc, command=cmd)
        return False

    return True

#
# list_attr - Call pwrcmd to list the attributes supported
#
def list_attr(expect=EXPECT_SUCCESS):
    """List all supported attributes"""
    options = ["--command", "list",
               "--list", "attr"]

    output = exec_pwrcmd(options, expect=expect)

    if "attr_list" not in output:
        print error_message(
            "attr_list not found",
            json.dumps(output, indent=2))
        sys.exit(102)

    return output["attr_list"]

#
# list_name - Call pwrcmd to list the object names supported
#
def list_name(filter=None, expect=EXPECT_SUCCESS):
    """List all objects in hierarchy, filtered by regex"""
    options = ["--command", "list",
               "--list", "name"]

    output = exec_pwrcmd(options, expect=expect)

    if "name_list" not in output:
        print error_message(
            "name_list not found",
            json.dumps(output, indent=2))
        sys.exit(102)

    if filter is None:
        return output["name_list"]

    select = []
    for name in output["name_list"]:
        if re.search(filter, name):
            select.append(name)

    return select


#
# get_meta - Call pwrcmd to get the metadata value of the specified
#            attribute/object
#
def get_meta(meta, obj_name, attr=None, desc=None, expect=EXPECT_SUCCESS):
    """Get one metadata value associated with an object/attribute.

    Args:
        meta:
            a string metadata name.
        obj_name:
            a string object name.
        attr:
            a string name of an attribute (unnecessary if object metadata not
            associated with an attribute).
        desc:
            a text description.
        expect:
            the expected return code of the pwrcmd call.
    Returns:
        The output from pwrcmd (JSON-format).

    Raises:
        This method raises no exceptions, but will exit() with a non-zero
        exit code on error.
    """
    if meta is None:
        print error_message("No meta value supplied to get_meta()")
        sys.exit(101)
    elif obj_name is None:
        print error_message("No obj_name value supplied to get_meta()")
        sys.exit(101)

    options = ["--command", "get",
               "--metadata", meta,
               "--name", obj_name]

    if attr is not None:
        options.extend(["--attribute", attr])

    result = exec_pwrcmd(options, desc=desc, expect=expect)
    errmsg = check_meta_result(result)
    if errmsg:
        print error_message(
            "check_meta_result failed: {}".format(errmsg),
            json.dumps(result, indent=2))
        sys.exit(101)
    return result

#
# set_meta - Call pwrcmd to set the metadata value of the specified
#            attribute/object
#
def set_meta(meta, obj_name, value, attr=None, role="PWR_ROLE_RM",
             method=None, desc=None, expect=EXPECT_SUCCESS):
    """Set one metadata value associated with an object/attribute.

    Args:
        meta:
            a string metadata name.
        obj_name:
            a string object name.
        attr:
            a string name of an attribute (unnecessary if object metadata not
            associated with an attribute).
        role:
            a string role name (e.g., "PWR_ROLE_RM")
        method:
            a string method name to check against (e.g, "PWR_ObjAttrSetMeta")
        desc:
            a text description.
        expect:
            the expected return code of the pwrcmd call.
    Returns:
        The output from pwrcmd (JSON-format).

    Raises:
        This method raises no exceptions, but will exit() with a non-zero
        exit code on error.
    """
    if meta is None:
        print error_message("No meta value supplied")
        sys.exit(101)
    elif obj_name is None:
        print error_message("No obj_name value supplied")
        sys.exit(101)
    elif value is None:
        print error_message("No value parameter supplied")
        sys.exit(101)

    if not isinstance(meta, str):
        try:
            meta = str(meta)
        except TypeError:
            print error_message("meta value is not stringable")
            sys.exit(102)
    if not isinstance(attr, str) and attr is not None:
        try:
            attr = str(attr)
        except TypeError:
            print error_message("attr value is not stringable")
            sys.exit(102)
    if not isinstance(obj_name, str):
        try:
            obj_name = str(obj_name)
        except TypeError:
            print error_message("obj_name value is not stringable")
            sys.exit(102)
    if not isinstance(value, str):
        try:
            value = str(value)
        except TypeError:
            print error_message("value parameter is not stringable")
            sys.exit(102)

    options = ["--command", "set",
               "--meta", meta,
               "--name", obj_name,
               "--value", value,
               "--role", role]

    if attr is not None:
        options.extend(["--attribute", attr])

    result = exec_pwrcmd(options, desc=desc, expect=expect)
    errmsg = check_method(result, method)
    if errmsg:
        print error_message(
            "check_method failed: {}".format(errmsg),
            json.dumps(result, indent=2))
        sys.exit(103)

    return result

#
# set_meta_and_check - Call pwrcmd to set the metadata value of the specified
#                      attribute/object, verify it was changed, then reset to
#                      its previous value.
#
def set_meta_and_check(meta, obj_name, value, attr=None, role="PWR_ROLE_RM",
                       method=None, desc=None, expect=EXPECT_SUCCESS):
    """Set one metadata value associated with an object/attribute, verify that
    the change was made, and restore the metadata value to its prior value.

    Args:
        meta:
            a string metadata name.
        obj_name:
            a string object name.
        attr:
            a string name of an attribute (unnecessary if object metadata not
            associated with an attribute).
        role:
            a string role name (e.g., "PWR_ROLE_RM")
        method:
            a string method name to check against (e.g, "PWR_ObjAttrSetMeta")
        desc:
            a text description.
        expect:
            the expected return code of the pwrcmd call.
    Returns:
        True if the set-verify-reset test is successful; False, otherwise.

    Raises:
        This method raises no exceptions, but will exit() with a non-zero
        exit code on error.
    """
    print "  set    value = {}".format(value)

    # Get the original value
    result = get_meta(meta, obj_name, attr=attr, desc=desc+" get orig")
    orig_value = result['value']
    print "  orig   value = {}".format(orig_value)

    expect_value = value
    print "  expect value = {}".format(expect_value)

    # Perform the set
    set_meta(meta, obj_name, value=value, attr=attr, role=role, method=method,
             desc=desc + " set new")
    # Record this command and description for errors
    cmd = get_command()
    dsc = get_description()

    # Get the new metadatum value. This should be the same as expect_value
    result = get_meta(meta, obj_name, attr=attr, desc=desc+" get check new")
    actual_value = result['value']
    print "  actual value = {}".format(actual_value)

    # Set the metadatum to the original value
    set_meta(meta, obj_name, value=orig_value, attr=attr, role=role,
             method=method, desc=desc + " reset orig")

    # Read back after reset
    result = get_meta(meta, obj_name, attr=attr, desc=desc + " get check orig")
    final_value = result['value']
    print "  final  value = {}".format(final_value)

    # Want some indication of what went wrong
    text = "\n".join(
        ["  set    value = {}".format(value),
         "  orig   value = {}".format(orig_value),
         "  expect value = {}".format(expect_value),
         "  actual value = {}".format(actual_value),
         "  final  value = {}".format(final_value)])
    if actual_value != expect_value:
        print error_message("new value != expected value",
                            output=text, desc=dsc, command=cmd)
        return False
    if orig_value != final_value:
        print error_message("final value != orig value",
                            output=text, desc=dsc, command=cmd)
        return False
    return True
