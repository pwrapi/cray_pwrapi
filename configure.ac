#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
# Copyright (c) 2015-2018, Cray Inc.
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

AC_PREREQ(2.59)
AC_INIT([cray-powerapi], [1.0], [http://bugzilla.us.cray.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CANONICAL_HOST

AC_SUBST([whatisdir], [${mandir}])
AC_SUBST([pkgconfigdir], [${libdir}/pkgconfig])

AC_ARG_WITH([fhs-sysconfdir],
  [AS_HELP_STRING([--with-fhs-sysconfdir=string],
  [use to specify the /etc/opt/ path for config files])],
  [AC_SUBST([FHS_SYSCONFDIR], $withval)],
  [AC_MSG_ERROR([fhs-sysconfdir must be specified])])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT

# Checks for libraries.
PKG_CHECK_MODULES([GIO20], [gio-2.0])
PKG_CHECK_MODULES([GLIB20], [glib-2.0])
PKG_CHECK_MODULES([GMODULE20], [gmodule-2.0])
PKG_CHECK_MODULES([GOBJECT20], [gobject-2.0])
PKG_CHECK_MODULES([GTHREAD20], [gthread-2.0])

PKG_CHECK_MODULES([CRAY_NHM], [cray-nhm])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE

# Checks for library functions.

AC_CONFIG_FILES(
clients/Makefile
clients/cnctl/Makefile
clients/pwrcmd/Makefile
cray-powerapi.pc
daemon/Makefile
include/Makefile
lib/Makefile
Makefile
module
test/Makefile
test/subsystems/Makefile
test/subsystems/cnctl/Makefile
test/subsystems/lib/Makefile
test/subsystems/pwrcmd/Makefile
test/subsystems/common/Makefile
)
AC_OUTPUT
