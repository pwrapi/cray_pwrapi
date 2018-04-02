##################################################################
#
# Copyright (c) 2017, Cray Inc.
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

CC=gcc

INCDIRS = -I /opt/cray/powerapi/default/include
LIBDIRS	= -L /opt/cray/powerapi/default/lib64
OBJS = pwrdemo.o workload.o stats.o profile.o mt-dgemm.o an9-stream.o
CFLAGS = -O3 -openmp -lm

.PHONY: default
default: pwrdemo

pwrdemo: $(OBJS)
	$(CC) $(INCDIRS) $(LIBDIRS) $(CFLAGS) -o pwrdemo $(OBJS) -l powerapi

pwrdemo.o: pwrdemo.c workload.h
	$(CC) $(INCDIRS) $(LIBDIRS) -c pwrdemo.c -l powerapi

workload.o: workload.c workload.h mt-dgemm.h an9-stream.h
	$(CC) $(INCDIRS) $(LIBDIRS) -c workload.c -l powerapi

stats.o: stats.c stats.h
	$(CC) $(INCDIRS) $(LIBDIRS) -c stats.c -l powerapi

profile.o: profile.c profile.h
	$(CC) $(INCDIRS) $(LIBDIRS) -c profile.c -l powerapi -lm

mt-dgemm.o: mt-dgemm.c mt-dgemm.h
	$(CC) -c mt-dgemm.c

an9-stream.o: an9-stream.c an9-stream.h
	$(CC) -c an9-stream.c

clean:
	rm -f pwrdemo $(OBJS)
