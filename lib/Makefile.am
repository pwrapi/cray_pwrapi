#
# Copyright (c) 2015-2017, Cray Inc.
#  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
# may be used to endorse or promote products derived from this software without
# specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

AM_CPPFLAGS = \
	$(GIO20_CFLAGS) \
	$(GLIB20_CFLAGS) \
	$(GMODULE20_CFLAGS) \
	$(GOBJECT20_CFLAGS) \
	$(GTHREAD20_CFLAGS) \
	-I@top_srcdir@/include

AM_CFLAGS = \
	-ggdb \
	-Wall \
	-Werror \
	-Wwrite-strings

AM_LDFLAGS = \
	$(GIO20_LIBS) \
	$(GLIB20_LIBS) \
	$(GMODULE20_LIBS) \
	$(GOBJECT20_LIBS) \
	$(GTHREAD20_LIBS) \
	-lm \
	-lrt

lib_LTLIBRARIES = libpowerapi.la

libpowerapi_la_SOURCES = \
	attributes.c \
	bitmask.c \
	context.c \
	group.c \
	hierarchy.c \
	ipc.c \
	log.c \
	object.c \
	opaque.c \
	pwr_list.c \
	report.c \
	statistics.c \
	timer.c \
	utility.c \
	version.c \
	hints/hint.c \
	plugins/common/command.c \
	plugins/common/common.c \
	plugins/common/file.c \
	plugins/cpudev/cstate.c \
	plugins/cpudev/freq.c \
	plugins/ipc_socket/ipc_socket.c \
	plugins/x86/x86_plugin.c \
	plugins/x86/x86_obj.c \
	plugins/x86/x86_obj_node.c \
	plugins/x86/x86_obj_pplane.c \
	plugins/x86/x86_obj_socket.c \
	plugins/x86/x86_obj_mem.c \
	plugins/x86/x86_obj_core.c \
	plugins/x86/x86_obj_ht.c \
	plugins/x86/x86_hierarchy.c \
	rolesys/acc_mc.c \
	rolesys/admin_mc.c \
	rolesys/app_os.c \
	rolesys/manager_rm.c \
	rolesys/mc_hw.c \
	rolesys/os_hw.c \
	rolesys/rm_mc.c \
	rolesys/rm_os.c \
	rolesys/user_mc.c \
	rolesys/user_rm.c \
	../common/gov.c

