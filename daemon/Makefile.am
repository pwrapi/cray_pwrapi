##################################################################
#
# Copyright (c) 2015-2018, Cray Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or
# without modification, are permitted provided that the following
# conditions are met:
#
# 1. Redistributions of source code must retain the above
# copyright notice, this list of conditions and the following
# disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of
# its contributors may be used to endorse or promote products
# derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.
#
##################################################################

AM_CPPFLAGS = \
	$(GIO20_CFLAGS) \
	$(GLIB20_CFLAGS) \
	$(GMODULE20_CFLAGS) \
	$(GOBJECT20_CFLAGS) \
	$(GTHREAD20_CFLAGS) \
	$(CRAY_NHM_CFLAGS) \
	-I@top_srcdir@/include \
	-I@top_srcdir@/common \
	-I@top_srcdir@/nhm \
	-I. \
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
	$(CRAY_NHM_LIBS)

sbin_PROGRAMS = powerapid powerapid_ctrl

powerapid_SOURCES = \
	powerapid.c \
	pwrapi_socket.c \
	pwrapi_set.c \
	pwrapi_worker.c \
	pwrapi_signal.c \
	pwrapi_down.c \
	../common/gov.c \
	../common/permissions.c \
	../lib/log.c

powerapid_ctrl_SOURCES = \
	ctrl/ctrl.c \
	ctrl/ctrl_request.c \
	ctrl/ctrl_socket.c \
	../lib/log.c
