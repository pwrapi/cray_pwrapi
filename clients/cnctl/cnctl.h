/*
 * Copyright (c) 2015-2016, Cray Inc.
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.

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

#ifndef __CNCTL_H
#define __CNCTL_H

#define CNCTL_MAJOR_VERSION	0
#define CNCTL_MINOR_VERSION	1

/*
 * cmd_type_t - An enum representing the requested command type.
 */
typedef enum {
	cmd_get,		// 'get' command
	cmd_set,		// 'set' command
	cmd_attr_cap,		// 'cap' (attr capabilities) command
	cmd_attr_val_cap,	// 'cap' (attr value capabilities) command
	cmd_uid_add,		// 'add uid' permissions command
	cmd_uid_remove,		// 'remove uid' permissions command
	cmd_uid_clear,		// 'clear uids' permissions command
	cmd_uid_restore,	// 'restore uids' permissions command
	cmd_uid_list		// 'list uids' permissions command
} cmd_type_t;

__attribute__ ((noreturn)) void cnctl_exit(int exit_code);

#endif /* ifndef __CNCTL_H */

