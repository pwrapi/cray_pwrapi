/*
 * Copyright (c) 2016-2017, Cray Inc.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>

#include <cray-powerapi/api.h>

#include "common.h"

//
// check_int_equal - Determine if expected value was obtained.
//
// Argument(s):
//
//	value - Value int
//	expected - Expected int
//
// Return Code(s):
//
//	void
//
void
check_int_equal(int value, int expected, int exit_code)
{
	if (value != expected) {
		printf("FAIL (value=%d != expected=%d)\n", value, expected);
		exit(exit_code);
		// NOT REACHED
	}

	printf("PASS\n");
}

//
// check_int_greater_than - Determine if value is greater than target.
//
// Argument(s):
//
//	value - Value int
//	target - Target int
//
// Return Code(s):
//
//	void
//
void
check_int_greater_than(int value, int target, int exit_code)
{
	if (value <= target) {
		printf("FAIL (value=%d <= target=%d)\n", value, target);
		exit(exit_code);
		// NOT REACHED
	}

	printf("PASS\n");
}

//
// check_int_greater_than_equal - Determine if value is greater than or equal
// to target.
//
// Argument(s):
//
//	value - Value int
//	target - Target int
//
// Return Code(s):
//
//	void
//
void
check_int_greater_than_equal(int value, int target, int exit_code)
{
	if (value < target) {
		printf("FAIL (value=%d < target=%d)\n", value, target);
		exit(exit_code);
		// NOT REACHED
	}

	printf("PASS\n");
}

//
// check_double_equal - Determine if expected value was obtained.
//
// Argument(s):
//
//	value - Value double
//	expected - Expected double
//
// Return Code(s):
//
//	void
//
void
check_double_equal(double value, double expected, int exit_code)
{
	if (abs(value - expected) > 1e-6) {
		printf("FAIL (value=%lf != expected=%lf)\n", value, expected);
		exit(exit_code);
		// NOT REACHED
	}

	printf("PASS\n");
}

//
// check_double_greater_than - Determine if value is greater than target.
//
// Argument(s):
//
//	value - Value double
//	target - Target double
//
// Return Code(s):
//
//	void
//
void
check_double_greater_than(double value, double target, int exit_code)
{
	if (value <= target) {
		printf("FAIL (value=%lf <= target=%lf)\n", value, target);
		exit(exit_code);
		// NOT REACHED
	}

	printf("PASS\n");
}

//
// check_double_greater_than_equal - Determine if value is greater than or
// equal to target.
//
// Argument(s):
//
//	value - Value double
//	target - Target double
//
// Return Code(s):
//
//	void
//
void
check_double_greater_than_equal(double value, double target, int exit_code)
{
	if (value < target) {
		printf("FAIL (value=%lf < target=%lf)\n", value, target);
		exit(exit_code);
		// NOT REACHED
	}

	printf("PASS\n");
}

//
// find_objects_of_type - Find objects of the given type and populate result
//
// Argument(s):
//
//	parent - Starting point in hierarchy
//	find_type - Object type to find
//	result - Place to put all of the objects
//
// Return Code(s):
//
//	void
//
void
find_objects_of_type(PWR_Obj parent, PWR_ObjType find_type, PWR_Grp result)
{
	PWR_ObjType type = PWR_OBJ_INVALID;
	PWR_Grp group = NULL;
	PWR_Obj child = NULL;
	unsigned int num_obj = 0;
	int idx;

	TST_ObjGetType(parent, &type, PWR_RET_SUCCESS);
	if (type == find_type) {
		TST_GrpAddObj(result, parent, PWR_RET_SUCCESS);
		return;
	}

	TST_ObjGetChildren(parent, &group, PWR_RET_SUCCESS);
	if (group == NULL) {
		return;
	}

	TST_GrpGetNumObjs(group, &num_obj, PWR_RET_SUCCESS);

	for (idx = 0; idx < num_obj; idx++) {
		TST_GrpGetObjByIndx(group, idx, &child, PWR_RET_SUCCESS);

		find_objects_of_type(child, find_type, result);
	}

	TST_GrpDestroy(group, PWR_RET_SUCCESS);
}

void
get_object_of_type(PWR_Cntxt ctx, PWR_Obj entry, PWR_Obj *obj, PWR_ObjType type)
{
	PWR_Grp grp = NULL;
	unsigned int num_objs = 0;

	switch (type) {
	case PWR_OBJ_SOCKET:
		TST_CntxtGetGrpByName(ctx, CRAY_NAMED_GRP_SOCKETS, &grp,
				PWR_RET_SUCCESS);
		break;
	case PWR_OBJ_CORE:
		TST_CntxtGetGrpByName(ctx, CRAY_NAMED_GRP_CORES, &grp,
				PWR_RET_SUCCESS);
		break;
	case PWR_OBJ_MEM:
		TST_CntxtGetGrpByName(ctx, CRAY_NAMED_GRP_MEMS, &grp,
				PWR_RET_SUCCESS);
		break;
	case PWR_OBJ_HT:
		TST_CntxtGetGrpByName(ctx, CRAY_NAMED_GRP_HTS, &grp,
				PWR_RET_SUCCESS);
		break;
	default:
		TST_GrpCreate(ctx, &grp, PWR_RET_SUCCESS);

		find_objects_of_type(entry, type, grp);
		break;
	}

	TST_GrpGetNumObjs(grp, &num_objs, PWR_RET_SUCCESS);
	printf("Found %u objects: ", num_objs);
	check_int_greater_than(num_objs, 0, EC_NO_HT_OBJ);

	TST_GrpGetObjByIndx(grp, 0, obj, PWR_RET_SUCCESS);

	TST_GrpDestroy(grp, PWR_RET_SUCCESS);
}

//
// get_socket_obj - Get an object of PWR_OBJ_SOCKET type
//
// Argument(s):
//
//	ctx - Context
//	entry - Entry point in hierarchy
//	obj - Place to put object
//
// Return Code(s):
//
//	void
//
void
get_socket_obj(PWR_Cntxt ctx, PWR_Obj entry, PWR_Obj *obj)
{
	get_object_of_type(ctx, entry, obj, PWR_OBJ_SOCKET);
}

//
// get_ht_obj - Get an object of PWR_OBJ_HT type
//
// Argument(s):
//
//	ctx - Context
//	entry - Entry point in hierarchy
//	obj - Place to put object
//
// Return Code(s):
//
//	void
//
void
get_ht_obj(PWR_Cntxt ctx, PWR_Obj entry, PWR_Obj *obj)
{
	get_object_of_type(ctx, entry, obj, PWR_OBJ_HT);
}

//
// TST_CntxtInit - Create a context
//
// Argument(s):
//
//	type - Context type to create
//	role - Role to use in context creation
//	name - Name of the new context
//	context - Return location for new context
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_CntxtInit(PWR_CntxtType type, PWR_Role role, const char *name,
	      PWR_Cntxt *context, int expected_retval)
{
	int retval = PWR_RET_SUCCESS;

	printf("%s(type=%d role=%d name=%s context=%p expected_retval=%d): ",
	       __func__, type, role, name, context, expected_retval);

	retval = PWR_CntxtInit(type, role, name, context);

	check_int_equal(retval, expected_retval, EC_CNTXT_CREATE);
}

//
// TST_CntxtDestroy - Destroy a context
//
// Argument(s):
//
//	context - Context to destroy
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_CntxtDestroy(PWR_Cntxt *context, int expected_retval)
{
	int retval = PWR_RET_SUCCESS;

	printf("%s(context=%p, expected_retval=%d): ",
	       __func__, context, expected_retval);

	retval = PWR_CntxtDestroy(context);

	check_int_equal(retval, expected_retval, EC_CNTXT_DESTROY);
}

//
// TST_CntxtGetEntryPoint - Get our entry point
//
// Argument(s):
//
//	context - Context to destroy
//	entry_point - Where to return the entry point
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_CntxtGetEntryPoint(PWR_Cntxt context, PWR_Obj *entry_point,
		       int expected_retval)
{
	int retval = PWR_RET_SUCCESS;

	printf("%s(context=%p, entry_point=%p, expected_retval=%d): ",
	       __func__, context, entry_point, expected_retval);

	retval = PWR_CntxtGetEntryPoint(context, entry_point);

	check_int_equal(retval, expected_retval, EC_CNTXT_GET_ENTRY_POINT);
}

//
// TST_CntxtGetGrpByName - Get copy of named group object
//
// Argument(s):
//
//	context - Context for operation
//	name - Name of the desired group
//	group - Where to return the new group
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_CntxtGetGrpByName(PWR_Cntxt context, const char *name,
		      PWR_Grp *group, int expected_retval)
{
	int retval = PWR_RET_SUCCESS;

	printf("%s(context=%p name=%s group=%p expected_retval=%d): ",
		__func__, context, name, group, expected_retval);

	retval = PWR_CntxtGetGrpByName(context, name, group);

	check_int_equal(retval, expected_retval, EC_CNTXT_GET_GROUP_BY_NAME);
}

//
// TST_ObjGetType - Get the object type for the target object
//
// Argument(s):
//
//	object - Object for which to query the type
//	type - Where to return the object type
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_ObjGetType(PWR_Obj object, PWR_ObjType *type, int expected_retval)
{
	int retval = PWR_RET_SUCCESS;

	printf("%s(object=%p, type=%p, expected_retval=%d): ",
	       __func__, object, type, expected_retval);

	retval = PWR_ObjGetType(object, type);

	check_int_equal(retval, expected_retval, EC_OBJ_GET_TYPE);
}

//
// TST_ObjGetName - Get the object name for the target object
//
// Argument(s):
//
//	object - Object for which to query the name
//	buf - Buffer to store the object name
//	len - Length of buffer
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_ObjGetName(PWR_Obj object, char *buf, size_t len, int expected_retval)
{
	int retval;

	printf("%s(object=%p buf=%p len=%lu expected_retval=%d): ",
		__func__, object, buf, len, expected_retval);

	retval = PWR_ObjGetName(object, buf, len);

	check_int_equal(retval, expected_retval, EC_OBJ_GET_NAME);
}

//
// TST_ObjAttrGetValue - Get value of attribute of object
//
// Argument(s):
//
//	object - Object for which to get the attribute
//	attr - The attribute to get the value of
//	value - Place to store the value
//	ts - Place to store the time for the attribute
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_ObjAttrGetValue(PWR_Obj object, PWR_AttrName attr, void *value,
	PWR_Time *ts, int expected_retval)
{
	int retval;

	printf("%s(object=%p attr=%d value=%p ts=%p expected_retval=%d): ",
		__func__, object, attr, value, ts, expected_retval);

	retval = PWR_ObjAttrGetValue(object, attr, value, ts);

	check_int_equal(retval, expected_retval, EC_OBJ_ATTR_GET_VALUE);
}

//
// TST_ObjAttrSetValue - Set value of attribute of object
//
// Argument(s):
//
//	object - Object for which to set the attribute
//	attr - The attribute to set the value of
//	value - Where to get the value
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_ObjAttrSetValue(PWR_Obj object, PWR_AttrName attr, const void *value,
	int expected_retval)
{
	int retval;

	printf("%s(object=%p attr=%d value=%p expected_retval=%d): ",
		__func__, object, attr, value, expected_retval);

	retval = PWR_ObjAttrSetValue(object, attr, value);

	check_int_equal(retval, expected_retval, EC_OBJ_ATTR_SET_VALUE);
}

//
// TST_ObjGetChildren - Get the children of the given object
//
// Argument(s):
//
//	object - Object for which to get the children
//	group - Place to store the group of children
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_ObjGetChildren(PWR_Obj object, PWR_Grp *group, int expected_retval)
{
	int retval;

	printf("%s(object=%p group=%p expected_retval=%d): ",
		__func__, object, group, expected_retval);

	retval = PWR_ObjGetChildren(object, group);

	if (retval == PWR_RET_WARN_NO_CHILDREN) {
		printf("PASS\n");
		return;
	}

	check_int_equal(retval, expected_retval, EC_OBJ_GET_CHILDREN);
}

//
// TST_GrpCreate - Create a group
//
// Argument(s):
//
//	context - Context for operation
//	group - Where to return the new group
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_GrpCreate(PWR_Cntxt context, PWR_Grp *group, int expected_retval)
{
	int retval = PWR_RET_SUCCESS;

	printf("%s(context=%p, group=%p, expected_retval=%d): ",
	       __func__, context, group, expected_retval);

	retval = PWR_GrpCreate(context, group);

	check_int_equal(retval, expected_retval, EC_GROUP_CREATE);
}

//
// TST_GrpDestroy - Destroy a group
//
// Argument(s):
//
//	group - Group to destroy
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_GrpDestroy(PWR_Grp group, int expected_retval)
{
	int retval = PWR_RET_SUCCESS;

	printf("%s(group=%p, expected_retval=%d): ",
	       __func__, group, expected_retval);

	retval = PWR_GrpDestroy(group);

	check_int_equal(retval, expected_retval, EC_GROUP_DESTROY);
}

//
// TST_GrpDuplicate - Duplicate a group
//
// Argument(s):
//
//	group1 - Group to duplicate
//	group2 - Where to return the duplicate group
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_GrpDuplicate(PWR_Grp group1, PWR_Grp *group2, int expected_retval)
{
	int retval = PWR_RET_SUCCESS;

	printf("%s(group1=%p, group2=%p, expected_retval=%d): ",
	       __func__, group1, group2, expected_retval);

	retval = PWR_GrpDuplicate(group1, group2);

	check_int_equal(retval, expected_retval, EC_GROUP_DUPLICATE);
}

//
// TST_GrpAddObj - Add an object to a group
//
// Argument(s):
//
//	group - Group to operate on
//	object - Object to add to group
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_GrpAddObj(PWR_Grp group, PWR_Obj object, int expected_retval)
{
	int retval = PWR_RET_SUCCESS;

	printf("%s(group=%p, object=%p, expected_retval=%d): ",
	       __func__, group, object, expected_retval);

	retval = PWR_GrpAddObj(group, object);

	check_int_equal(retval, expected_retval, EC_GROUP_ADD);
}

//
// TST_GrpRemoveObj - Remove an object from a group
//
// Argument(s):
//
//	group - Group to operate on
//	object - Object to remove from group
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_GrpRemoveObj(PWR_Grp group, PWR_Obj object, int expected_retval)
{
	int retval = PWR_RET_SUCCESS;

	printf("%s(group=%p, object=%p, expected_retval=%d): ",
	       __func__, group, object, expected_retval);

	retval = PWR_GrpRemoveObj(group, object);

	check_int_equal(retval, expected_retval, EC_GROUP_REMOVE);
}

//
// TST_GrpGetNumObjs - Get number of objects in group
//
// Argument(s):
//
//	group - Group to operate on
//	num_objects - Where to return the number of objects
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
// TODO: Rev 1.5 of spec should change return value to a parameter
void
TST_GrpGetNumObjs(PWR_Grp group, unsigned int *num_objects, int expected_retval)
{
	int retval = PWR_RET_SUCCESS;

	printf("%s(group=%p num=%p expected_retval=%d): ",
		__func__, group, num_objects, expected_retval);

	retval = PWR_GrpGetNumObjs(group);

	check_int_greater_than(retval, PWR_RET_FAILURE, EC_GROUP_GET_NUM_OBJS);

	*num_objects = retval;
}

//
// TST_GrpGetObjByIndx - Get object at specified index in group
//
// Argument(s):
//
//	group - Group to operate on
//	idx - desired index
//	object - Where to put the object
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_GrpGetObjByIndx(PWR_Grp group, int idx, PWR_Obj *object,
	int expected_retval)
{
	int retval;

	printf("%s(group=%p idx=%d object=%p expected_retval=%d): ",
		__func__, group, idx, object, expected_retval);

	retval = PWR_GrpGetObjByIndx(group, idx, object);

	check_int_equal(retval, expected_retval, EC_GROUP_GET_OBJ_BY_IDX);
}

//
// TST_GrpUnion - Get the union of the two groups
//
// Argument(s):
//
//	group1 - Group to operate on
//	group2 - Group to operate on
//	group3 - Where to put the new group
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_GrpUnion(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3,
	     int expected_retval)
{
	int retval;

	printf("%s(group1=%p group2=%p group3=%p expected_retval=%d): ",
		__func__, group1, group2, group3, expected_retval);

	retval = PWR_GrpUnion(group1, group2, group3);

	check_int_equal(retval, expected_retval, EC_GROUP_UNION);
}

//
// TST_GrpIntersection - Get the intersection of the two groups
//
// Argument(s):
//
//	group1 - Group to operate on
//	group2 - Group to operate on
//	group3 - Where to put the new group
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_GrpIntersection(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3,
		    int expected_retval)
{
	int retval;

	printf("%s(group1=%p group2=%p group3=%p expected_retval=%d): ",
		__func__, group1, group2, group3, expected_retval);

	retval = PWR_GrpIntersection(group1, group2, group3);

	check_int_equal(retval, expected_retval, EC_GROUP_INTERSECTION);
}

//
// TST_GrpDifference - Get the difference of the two groups
//
// Argument(s):
//
//	group1 - Group to operate on
//	group2 - Group to operate on
//	group3 - Where to put the new group
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_GrpDifference(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3,
		  int expected_retval)
{
	int retval;

	printf("%s(group1=%p group2=%p group3=%p expected_retval=%d): ",
		__func__, group1, group2, group3, expected_retval);

	retval = PWR_GrpDifference(group1, group2, group3);

	check_int_equal(retval, expected_retval, EC_GROUP_DIFFERENCE);
}

//
// TST_GrpSymDifference - Get the symmetric difference of the two groups
//
// Argument(s):
//
//	group1 - Group to operate on
//	group2 - Group to operate on
//	group3 - Where to put the new group
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_GrpSymDifference(PWR_Grp group1, PWR_Grp group2, PWR_Grp *group3,
		     int expected_retval)
{
	int retval;

	printf("%s(group1=%p group2=%p group3=%p expected_retval=%d): ",
		__func__, group1, group2, group3, expected_retval);

	retval = PWR_GrpSymDifference(group1, group2, group3);

	check_int_equal(retval, expected_retval, EC_GROUP_SYM_DIFFERENCE);
}

//
// TST_StatCreatObj - Create a statistics object for a single object
//
// Argument(s):
//
//	object - Object to collect statistics on
//	name - Attribute to collect statistics on
//	statistic - Statistic to collect
//	stat - Where to put the new statistic
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_StatCreateObj(PWR_Obj object, PWR_AttrName name,
		  PWR_AttrStat statistic, PWR_Stat *stat,
		  int expected_retval)
{
	int retval;

	printf("%s(object=%p name=%d statistic=%d stat= %p expected_retval=%d): ",
		__func__, object, name, statistic, stat, expected_retval);

	retval = PWR_ObjCreateStat(object, name, statistic, stat);

	check_int_equal(retval, expected_retval, EC_STAT_CREATE_OBJ);
}

//
// TST_StatCreatGrp - Create a statistics object for a group of objects
//
// Argument(s):
//
//	group - Group of objects to collect statistics on
//	name - Attribute to collect statistics on
//	statistic - Statistic to collect
//	stat - Where to put the new statistic
//	expected_retval - Expected retval for operation
//
// Return Code(s):
//
//	void
//
void
TST_StatCreateGrp(PWR_Grp group, PWR_AttrName name,
		  PWR_AttrStat statistic, PWR_Stat *stat,
		  int expected_retval)
{
	int retval;

	printf("%s(group=%p name=%d statistic=%d stat= %p expected_retval=%d): ",
		__func__, group, name, statistic, stat, expected_retval);

	retval = PWR_GrpCreateStat(group, name, statistic, stat);

	check_int_equal(retval, expected_retval, EC_STAT_CREATE_GRP);
}

void
TST_StatDestroy(PWR_Stat stat, int expected_retval)
{
	int retval;

	printf("%s(stat=%p expected_retval=%d): ",
		__func__, stat, expected_retval);

	retval = PWR_StatDestroy(stat);

	check_int_equal(retval, expected_retval, EC_STAT_DESTROY);
}

void
TST_StatStart(PWR_Stat stat, int expected_retval)
{
	int retval;

	printf("%s(stat=%p expected_retval=%d): ",
		__func__, stat, expected_retval);

	retval = PWR_StatStart(stat);

	check_int_equal(retval, expected_retval, EC_STAT_START);
}

void
TST_StatStop(PWR_Stat stat, int expected_retval)
{
	int retval;

	printf("%s(stat=%p expected_retval=%d): ",
		__func__, stat, expected_retval);

	retval = PWR_StatStop(stat);

	check_int_equal(retval, expected_retval, EC_STAT_STOP);
}

void
TST_StatClear(PWR_Stat stat, int expected_retval)
{
	int retval;

	printf("%s(stat=%p expected_retval=%d): ",
		__func__, stat, expected_retval);

	retval = PWR_StatClear(stat);

	check_int_equal(retval, expected_retval, EC_STAT_CLEAR);
}

void
TST_StatGetValue(PWR_Stat stat, double *value, PWR_TimePeriod *times,
		 int expected_retval)
{
	int retval;

	retval = PWR_StatGetValue(stat, value, times);

	printf("%s(stat=%p value=%p(%g) times=%p expected_retval=%d): ",
		__func__, stat, value, *value, times, expected_retval);

	check_int_equal(retval, expected_retval, EC_STAT_GET_VALUE);
}

void
TST_StatGetValues(PWR_Stat stat, double values[], PWR_TimePeriod times[],
		  int expected_retval)
{
	int retval;

	retval = PWR_StatGetValues(stat, values, times);

	printf("%s(stat=%p values=%p(%g) times=%p expected_retval=%d): ",
		__func__, stat, values, values[0], times, expected_retval);

	check_int_equal(retval, expected_retval, EC_STAT_GET_VALUES);
}

void
TST_StatGetReduce(PWR_Stat stat, PWR_AttrStat reduceOp, int *index,
		  double *value, PWR_Time *time, int expected_retval)
{
	int retval;

	retval = PWR_StatGetReduce(stat, reduceOp, index, value, time);

	printf("%s(stat=%p op=%d index=%p(%d) value=%p(%g) time=%p"
		" expected_retval=%d): ",
		__func__, stat, reduceOp, index, *index, value, *value, time,
		expected_retval);

	check_int_equal(retval, expected_retval, EC_STAT_GET_REDUCE);
}

void
TST_CntxtGetObjByName(PWR_Cntxt context, const char *name, PWR_Obj *objectp,
		int expected_retval)
{
	int retval;

	retval = PWR_CntxtGetObjByName(context, name, objectp);

	printf("%s(context=%p name=%s objectp=%p expected_retval=%d): ",
		__func__, context, name, objectp, expected_retval);

	check_int_equal(retval, expected_retval, EC_CNTXT_GET_OBJ_BY_NAME);
}

void
TST_AppHintCreate(PWR_Obj object, const char *name, uint64_t *hintidp,
		PWR_RegionHint hint, PWR_RegionIntensity level, int expected_retval)
{
	int retval;

	retval = PWR_AppHintCreate(object, name, hintidp, hint, level);

	printf("%s(object=%p name=%s hintidp=%p hint=%d level=%d expected_retval=%d): ",
			__func__, object, name, hintidp, hint, level, expected_retval);

	check_int_equal(retval, expected_retval, EC_HINT_CREATE);
}

void
TST_AppHintDestroy(uint64_t hintid, int expected_retval)
{
	int retval;

	retval = PWR_AppHintDestroy(hintid);

	printf("%s(hintid=%lu expected_retval=%d): ",
			__func__, hintid, expected_retval);

	check_int_equal(retval, expected_retval, EC_HINT_DESTROY);
}

void
TST_AppHintStart(uint64_t hintid, int expected_retval)
{
	int retval;

	retval = PWR_AppHintStart(hintid);

	printf("%s(hintid=%lu expected_retval=%d): ",
			__func__, hintid, expected_retval);

	check_int_equal(retval, expected_retval, EC_HINT_START);
}

void
TST_AppHintStop(uint64_t hintid, int expected_retval)
{
	int retval;

	retval = PWR_AppHintStop(hintid);

	printf("%s(hintid=%lu expected_retval=%d): ",
			__func__, hintid, expected_retval);

	check_int_equal(retval, expected_retval, EC_HINT_STOP);
}

void
TST_AppHintProgress(uint64_t hintid, double progress, int expected_retval)
{
	int retval;

	retval = PWR_AppHintProgress(hintid, progress);

	printf("%s(hintid=%lu progress=%04.2f expected_retval=%d): ",
			__func__, hintid, progress, expected_retval);

	check_int_equal(retval, expected_retval, EC_HINT_PROGRESS);
}

void
TST_GetSleepState(PWR_Obj obj, PWR_SleepState *state, int expected_retval)
{
	int retval;

	retval = PWR_GetSleepState(obj, state);

	printf("%s(obj=%p state=%p(%d) expected_retval=%d): ",
		__func__, obj, state, *state, expected_retval);

	check_int_equal(retval, expected_retval, EC_APPOS_GET_SLEEP_STATE);
}

void
TST_SetSleepStateLimit(PWR_Obj obj, PWR_SleepState state, int expected_retval)
{
	int retval;

	retval = PWR_SetSleepStateLimit(obj, state);

	printf("%s(obj=%p state=%d expected_retval=%d): ",
		__func__, obj, state, expected_retval);

	check_int_equal(retval, expected_retval,
			EC_APPOS_SET_SLEEP_STATE_LIMIT);
}

void
TST_WakeUpLatency(PWR_Obj obj, PWR_SleepState state, PWR_Time *latency,
		int expected_retval)
{
	int retval;

	retval = PWR_WakeUpLatency(obj, state, latency);

	printf("%s(obj=%p state=%d latency=%p(%lu) expected_retval=%d): ",
		__func__, obj, state, latency, *latency, expected_retval);

	check_int_equal(retval, expected_retval, EC_APPOS_WAKEUP_LATENCY);
}

void
TST_RecommendSleepState(PWR_Obj obj, PWR_Time latency, PWR_SleepState *state,
		int expected_retval)
{
	int retval;

	retval = PWR_RecommendSleepState(obj, latency, state);

	printf("%s(obj=%p latency=%lu state=%p(%d) expected_retval=%d): ",
		__func__, obj, latency, state, *state, expected_retval);

	check_int_equal(retval, expected_retval,
			EC_APPOS_RECOMMEND_SLEEP_STATE);
}

void
TST_GetPerfState(PWR_Obj obj, PWR_PerfState *state, int expected_retval)
{
	int retval;

	retval = PWR_GetPerfState(obj, state);

	printf("%s(obj=%p state=%p(%d) expected_retval=%d): ",
		__func__, obj, state, *state, expected_retval);

	check_int_equal(retval, expected_retval,
			EC_APPOS_GET_PERF_STATE);
}

void
TST_SetPerfState(PWR_Obj obj, PWR_PerfState state, int expected_retval)
{
	int retval;

	retval = PWR_SetPerfState(obj, state);

	printf("%s(obj=%p state=%d expected_retval=%d): ",
		__func__, obj, state, expected_retval);

	check_int_equal(retval, expected_retval,
			EC_APPOS_SET_PERF_STATE);
}
