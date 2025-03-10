#pragma once
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#define SGE_SEC_BUFSIZE 1024

void sge_security_exit(int i);

#ifdef SECURE
/* int 0 on success, -1 on failure */
int sge_ssl_setup_security_path(const char *progname, const char *username);
#endif

bool sge_encrypt(const char *intext, char *outbuf, int outsize);
bool sge_decrypt(char *intext, int inlen, char *out_buffer, int *outsize);
bool change_encoding(char *cbuf, int* csize, unsigned char* ubuf, int* usize, int mode);

bool
sge_security_verify_user(const char *host, const char *commproc, u_long32 id, const char *user);

bool sge_security_verify_unique_identifier(bool check_admin_user, 
                                           const char* user, 
                                           const char* progname,
                                           unsigned long progid, 
                                           const char* hostname, 
                                           const char* commproc, 
                                           unsigned long commid);
