#
# Copyright (c) 2016-2017, Cray Inc.
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

AM_CPPFLAGS =	$(GIO20_CFLAGS) \
		$(GLIB20_CFLAGS) \
		$(GMODULE20_CFLAGS) \
		$(GOBJECT20_CFLAGS) \
		$(GTHREAD20_CFLAGS) \
		-I@top_srcdir@/include \
		-Wall \
		-Werror \
		-Wwrite-strings

AM_LDFLAGS =	$(GIO20_LIBS) \
		$(GLIB20_LIBS) \
		$(GMODULE20_LIBS) \
		$(GOBJECT20_LIBS) \
		$(GTHREAD20_LIBS) \
		@top_srcdir@/lib/libpowerapi.la

libtestdir = $(prefix)/bin/test/subsystems/lib

libtest_SCRIPTS =
libtest_PROGRAMS = apphints appos attr-freq attr-gov attr-power-max context \
		group hierarchy logging stats

apphints_SOURCES =			\
	apphints.c			\
	../common/common.c

appos_SOURCES =				\
	appos.c				\
	../common/common.c

attr_freq_SOURCES =			\
	attr-freq.c			\
	../common/common.c

attr_gov_SOURCES =			\
	attr-gov.c			\
	../common/common.c

attr_power_max_SOURCES =		\
	attr-power-max.c		\
	../common/common.c

context_SOURCES =			\
	context.c			\
	../common/common.c

group_SOURCES =				\
	group.c				\
	../common/common.c

hierarchy_SOURCES =			\
	hierarchy.c			\
	../common/common.c

logging_SOURCES =			\
	logging.c			\
	../../../include/log.h

stats_SOURCES =				\
	stats.c				\
	../common/common.c
