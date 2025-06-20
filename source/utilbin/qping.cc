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

#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <cstring>
#include <cctype>

#include "uti/sge_time.h"
#include "uti/sge_arch.h"
#include "uti/sge_profiling.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_signal.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_string.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_Version.h"

#include "gdi/ocs_gdi_security.h"
#include "gdi/ocs_gdi_Packet.h"

#include "comm/cl_commlib.h"
#include "comm/lists/cl_util.h"

#include "basis_types.h"
#include "msg_utilbin.h"
#include "msg_clients_common.h"

#define ARGUMENT_COUNT 15
static char*  cl_values[ARGUMENT_COUNT+2];
static int    cl_short_host_name_option = 0;                                       
static int    cl_show[]         = {1,1 ,1,1 ,1,1,1 ,1,1,1,1,0 ,0,1,1};
static int    cl_alignment[]    = {1,0 ,1,0 ,1,1,1 ,1,1,1,1,0 ,0,1,1};
static size_t cl_column_width[] = {0,0 ,0,30,0,0,22,0,0,0,0,20,5,0,0};
static const char*  cl_description[] = {
          /* 00 */               "time of debug output creation",                       
          /* 01 */               "endpoint service name where debug client is connected",
          /* 02 */               "message direction",
          /* 03 */               "name of participating communication endpoint",
          /* 04 */               "message data format",
          /* 05 */               "message acknowledge type",
          /* 06 */               "message tag information",
          /* 07 */               "message id",
          /* 08 */               "message response id",
          /* 09 */               "message length",
          /* 10 */               "time when message was sent/received",
          /* 11 */               "message content dump (xml/bin/cull)",
          /* 12 */               "additional information",
          /* 13 */               "commlib linger time",
          /* 14 */               "nr. of connections"
};

static const char* cl_names[] = {
                       "time",
                       "local",
                       "d.",
                       "remote",
                       "format",
                       "ack type",
                       "msg tag",
                       "msg id",
                       "msg rid",
                       "msg len",
                       "msg time",
                       "msg dump",
                       "info",
                       "msg ltime",
                       "con count"
};



static void sighandler_ping(int sig);
#if 0
static int pipe_signal = 0;
static int hup_signal = 0;
#endif
static int do_shutdown = 0;

static void sighandler_ping(int sig) {
   if (sig == SIGPIPE) {
#if 0
      pipe_signal = 1;
#endif
      return;
   }
   if (sig == SIGHUP) {
#if 0
      hup_signal = 1;
#endif
      return;
   }
   cl_com_ignore_timeouts(true);
   do_shutdown = 1;
}

static void qping_set_output_option(char* option_string) {
   char* option = nullptr;
   char* value1 = nullptr;
   char* value2 = nullptr;
   int opt = 0;
   option = strstr(option_string, ":");
   if (option) {
      option[0]=0;
      if (strcmp(option_string,"h") == 0) {
         opt = 1;
         option++;
         value1=option;
      }
      if (strcmp(option_string,"hn") == 0) {
         opt = 2;
         option++;
         value1=option;
      }
      if (strcmp(option_string,"w") == 0) {
         opt = 3;
         option++;
         value2=strstr(option,":");
         value2[0] = 0;
         value1=option;
         value2++;
      }
      if (strcmp(option_string,"s") == 0) {
         opt = 4;
         option++;
         value1=option;
      }

      switch(opt) {
         case 1: {
            /* h */
            int column = -1;
            if (value1) {
               column = atoi(value1);
            }
            column--;
            if (column>=0 && column <ARGUMENT_COUNT) {
               cl_show[column] = 0;
            }
            break;
         }
         case 4: {
            /* s */
            int column = -1;
            if (value1) {
               column = atoi(value1);
            }
            column--;
            if (column>=0 && column <ARGUMENT_COUNT) {
               cl_show[column] = 1;
            }
            break;
         }
         case 2: {
            /* hn */
            if(value1) {
               if (strcmp(value1,"short") == 0) {
                  cl_short_host_name_option = 1;
               }
               if (strcmp(value1,"long") == 0) {
                  cl_short_host_name_option = 0;
               }
            }
            break;
         }
         case 3: {
            /* w */
            int column = -1;
            int width = -1;
            if(value1) {
               column = atoi(value1);
            }
            if(value2) {
               width = atoi(value2);
            }
            column--;
            if (column>=0 && column <ARGUMENT_COUNT) {
               if (width > 0) {
                  cl_column_width[column] = width;
               }
            }
            break;
         }
      }

   }

   
}

static void qping_convert_time(char* buffer, char* dest, size_t dest_size, bool show_hour) {
   time_t i;
   char* help;
   char* help2;
#ifdef HAS_LOCALTIME_R
   struct tm tm_buffer;
#endif
   struct tm *tm;
   struct saved_vars_s *context = nullptr;

   help=sge_strtok_r(buffer, ".", &context);
   if (help == nullptr) {
      help = (char*)"-";
   }
   help2=sge_strtok_r(nullptr,".", &context);
   if (help2 == nullptr) {
      help2 = (char*)"nullptr";
   }

   if (strcmp(help,"-") == 0) {
      snprintf(dest, dest_size, "N.A.");
   } else {

      i = atoi(help);
#ifndef HAS_LOCALTIME_R
      tm = localtime(&i);
#else
      tm = (struct tm *)localtime_r(&i, &tm_buffer);
#endif
      if (show_hour) {
         snprintf(dest, dest_size, "%02d:%02d:%02d.%s", tm->tm_hour, tm->tm_min, tm->tm_sec, help2);
      } else {
         snprintf(dest, dest_size, "%02d:%02d.%s", tm->tm_min, tm->tm_sec, help2);
      }
   }
   sge_free_saved_vars(context);
}

static void qping_general_communication_error(const cl_application_error_list_elem_t* commlib_error) {
   if (commlib_error != nullptr) {
      if (!commlib_error->cl_already_logged) {
         if (commlib_error->cl_info != nullptr) {
            fprintf(stderr,"%s: %s\n", cl_get_error_text(commlib_error->cl_error), commlib_error->cl_info);
         } else {
            fprintf(stderr,"error: %s\n", cl_get_error_text(commlib_error->cl_error));
         }
         if (commlib_error->cl_error == CL_RETVAL_ACCESS_DENIED) {
            do_shutdown = 1;
         } 
      }
   }
}

static void qping_parse_environment() {
   char* env_opt = nullptr;
   char opt_buffer[1024];
   char* opt_str = nullptr;

   env_opt = getenv("SGE_QPING_OUTPUT_FORMAT");
   if (env_opt != nullptr) {
      snprintf(opt_buffer, 1024, "%s", env_opt);
      opt_str=strtok(opt_buffer, " ");
      if (opt_str) {
         qping_set_output_option(opt_str);
      }
      while( (opt_str=strtok(nullptr, " "))) {
         qping_set_output_option(opt_str);
      }
   }
}

static void qping_printf_fill_up(FILE* fd, const char* name, int length, char c, int before) {
   int n = strlen(name);
   int i;

   if (before == 0) {
      fprintf(fd,"%s",name);
   }
   for (i=0;i<(length-n);i++) {
      fprintf(fd,"%c",c);
   }
   if (before != 0) {
      fprintf(fd,"%s",name);
   }

}

static void qping_print_line(const char* buffer, int nonewline, int dump_tag, const char *sender_comp_name) {
   size_t i=0;
   size_t max_name_length = 0;
   size_t full_width = 0;
   cl_com_debug_message_tag_t debug_tag = CL_DMT_MESSAGE;
   static int show_header = 1;
   char* message_debug_data = nullptr;
   char  time[512];
   char  msg_time[512];
   char  com_time[512];
   struct saved_vars_s *context = nullptr;

   for (i=0;i<ARGUMENT_COUNT;i++) {
      if (max_name_length < strlen(cl_names[i])) {
         max_name_length = strlen(cl_names[i]);
      }
   }
   
   i=0;
   cl_values[i++] = sge_strtok_r(buffer, "\t", &context);
   while ((cl_values[i++] = sge_strtok_r(nullptr, "\t\n", &context)));

   i = atoi(cl_values[0]);
   /* check if this message version has tag info (qping after 60u3 release) */
   if (i >= CL_DMT_MESSAGE &&i < CL_DMT_MAX_TYPE) {
      debug_tag = (cl_com_debug_message_tag_t)i;
      for (i=0;i<ARGUMENT_COUNT;i++) {
         cl_values[i] = cl_values[i+1];
      }
   }

   if (debug_tag == CL_DMT_MESSAGE && (dump_tag == 1 || dump_tag == 3)) {

      qping_convert_time(cl_values[0], time, sizeof(time), true);
      cl_values[0] = time;
   
      qping_convert_time(cl_values[10], msg_time, sizeof(msg_time), true);
      cl_values[10] = msg_time;
 
      qping_convert_time(cl_values[13], com_time, sizeof(com_time), false);
      cl_values[13] = com_time;
   
      if (cl_short_host_name_option != 0) {
         char* help = nullptr;
         char* help2 = nullptr;
         char help_buffer[1024];
   
         help = strstr(cl_values[1],".");
         if (help) {
            help2 = strstr(cl_values[1],"/");
   
            help[0]=0;
            if (help2) {
               snprintf(help_buffer, 1024,"%s%s", cl_values[1], help2);
               snprintf(cl_values[1], 1024, "%s", help_buffer);
            }
         }
   
         help = strstr(cl_values[3],".");
         if (help) {
            help2 = strstr(cl_values[3],"/");
   
            help[0]=0;
            if (help2) {
               snprintf(help_buffer,1024, "%s%s", cl_values[3], help2);
               snprintf(cl_values[3], 1024, "%s", help_buffer);
            }
         }
   
      }
   
      if (nonewline != 0) {
         message_debug_data = cl_values[11];
         if ( strstr( cl_values[4] , "bin") != nullptr ) {
            cl_values[11] = (char*)"binary message data";
         } else {
            cl_values[11] = (char*)"xml message data";
         }
      }
   
      for(i=0;i<ARGUMENT_COUNT;i++) {
         if (cl_column_width[i] < strlen(cl_values[i])) {
            cl_column_width[i] = strlen(cl_values[i]);
         }
         if (cl_column_width[i] < strlen(cl_names[i])) {
            cl_column_width[i] = strlen(cl_names[i]);
         }
         if (cl_show[i]) {
            full_width += cl_column_width[i];
            full_width++;
         }
      }
   
      if (show_header == 1) {
         for (i=0;i<ARGUMENT_COUNT;i++) {
            if (cl_show[i]) {
               qping_printf_fill_up(stdout, cl_names[i], cl_column_width[i],' ',cl_alignment[i]);
               printf("|");
            }
         }
         printf("\n");
         for (i=0;i<ARGUMENT_COUNT;i++) {
            if (cl_show[i]) {
               qping_printf_fill_up(stdout, "",cl_column_width[i],'-',cl_alignment[i]);
               printf("|");
            }
         }
         printf("\n");
   
         show_header = 0;
      }
      for (i=0;i<ARGUMENT_COUNT;i++) {
         if (cl_show[i]) {
            qping_printf_fill_up(stdout, cl_values[i],cl_column_width[i],' ',cl_alignment[i]);
            printf("|");
         }
      }
      printf("\n");
   
      if (nonewline != 0 && cl_show[11]) {
         if ( strstr( cl_values[4] , "bin") != nullptr ) {
            unsigned char* binary_buffer = nullptr;
            const char* bin_start = "--- BINARY block start ";
            const char* bin_end   = "--- BINARY block end ";
            int counter = 0;
            size_t message_debug_data_length = strlen(message_debug_data);
            printf("%s", bin_start);
            for(i=strlen(bin_start);i<full_width;i++) {
               printf("-");
            }
            printf("\n");
            for (i=0;i<message_debug_data_length;i++) {
               if (counter == 0) {
                  printf("   ");
               }
               printf("%c", message_debug_data[i]);
               counter++;
               if (counter % 16 == 0) {
                  printf(" ");
               }
               if(counter == 64) {
                  size_t x = i - 63;
                  int hi,lo;
                  int value = 0;
                  int printed = 0;
                  printf("   ");
                  while(x<=i) {
                     hi = message_debug_data[x++];
                     lo = message_debug_data[x++];
                     value = (cl_util_get_hex_value(hi) << 4) + cl_util_get_hex_value(lo);
                     if (isalnum(value)) {
                        printf("%c",value);
                     } else {
                        printf(".");
                     }
                     printed++;
   
                     if ( printed % 8 == 0) {
                        printf(" ");
                     }
                  }
                  counter = 0;
                  printf("\n");
               }
               
               if((i+1) == message_debug_data_length && counter != 0 ) {
                  size_t x = i - counter + 1;
                  int hi,lo;
                  int value = 0;
                  int printed = 0; 
                  int a;
                  printf("   ");
                  for(a=0;a<(64 - counter);a++) {
                     printf(" ");
                     if (a % 16 == 0) {
                        printf(" ");
                     }
   
                  }
                  while(x<=i) {
                     hi = message_debug_data[x++];
                     lo = message_debug_data[x++];
                     value = (cl_util_get_hex_value(hi) << 4) + cl_util_get_hex_value(lo);
                     if (isalnum(value)) {
                        printf("%c",value);
                     } else {
                        printf(".");
                     }
                     printed++;
   
                     if ( printed % 8 == 0) {
                        printf(" ");
                     }
                  }
                  printf("\n");
               }
            }
   
            /* try to unpack gdi data */
            if (strstr( cl_values[6] , "TAG_GDI_REQUEST") != nullptr) {
               unsigned long buffer_length = 0;
               if (cl_util_get_binary_buffer(message_debug_data, &binary_buffer , &buffer_length) == CL_RETVAL_OK) {
                  sge_pack_buffer buf;

                  // unpack the message content
                  // we may *not* decode the auth_info - at least not with munge authentication, as Munge will decode
                  // a certificate only once, and it needs to be decoded in the message receiver as well.
                  // we do not output the parsed auth_info anyway, so we skip decoding and just output the raw auth_info
                  // string.
                  if (init_packbuffer_from_buffer(&buf, (char*)binary_buffer, buffer_length, false) == PACK_SUCCESS) {
                     ocs::gdi::Packet *packet = new ocs::gdi::Packet();
               
                     if (packet->unpack(nullptr, &buf)) {
                        printf("      unpacked gdi request (binary buffer length %lu):\n", buffer_length );
                        printf("         packet:\n");

                        printf("host   : %s\n", packet->host);
                        printf("commproc   : %s\n", packet->commproc);
                        if (packet->version) {
                           printf("version   : " sge_u32 "\n", packet->version);
                        } else {
                           printf("version   : %s\n", "nullptr");
                        }
                        if (buf.auth_info != nullptr) {
                           printf("auth_info   : %s\n", buf.auth_info);
                        } else {
                           printf("auth_info   : %s\n", "nullptr");
                        }
                        for (auto *task : packet->tasks) {
                           printf("         task:\n");

                           if (task->command) {
                              printf("op     : %d\n", task->command);
                           } else {
                              printf("op     : %s\n", "nullptr");
                           }
                           if (task->target) {
                              printf("target : %d\n", task->target);
                           } else {
                              printf("target : %s\n", "nullptr");
                           }
   
                           if (task->data_list) {
                              lWriteListTo(task->data_list,stdout); 
                           } else {
                              printf("lp   : %s\n", "nullptr");
   
                           }
                           if (task->answer_list) {
                              lWriteListTo(task->answer_list,stdout); 
                           } else {
                              printf("alp   : %s\n", "nullptr");
                           }
   
                           if (task->condition) {
                              lWriteWhereTo(task->condition,stdout); 
                           } else {
                              printf("cp   : %s\n", "nullptr");
                           }
                           if (task->enumeration) {
                              lWriteWhatTo(task->enumeration,stdout); 
                           } else {
                              printf("enp   : %s\n", "nullptr");
                           }
                        }
                     } else {
                        delete packet;
                     }

                     clear_packbuffer(&buf);
                  }
               }
   
            }
            if (strstr(cl_values[6], "TAG_REPORT_REQUEST") != nullptr) {
               unsigned long buffer_length = 0;
               if (cl_util_get_binary_buffer(message_debug_data, &binary_buffer , &buffer_length) == CL_RETVAL_OK) {
                  sge_pack_buffer buf;
   
                  if (init_packbuffer_from_buffer(&buf, (char*)binary_buffer, buffer_length, false) == PACK_SUCCESS) {
                     lList *rep = nullptr;
   
                     if (cull_unpack_list(&buf, &rep) == 0) {
                        printf("      unpacked report request (binary buffer length %lu):\n", buffer_length );
                        if (rep) {
                           lWriteListTo(rep, stdout); 
                        } else {
                           printf("rep: nullptr\n");
                        }
                     }
                     lFreeList(&rep);
                     rep = nullptr;
                     clear_packbuffer(&buf);
                  }
               }
            }
   
            if (strstr( cl_values[6] , "TAG_EVENT_CLIENT_EXIT") != nullptr) {
               unsigned long buffer_length = 0;
               if (cl_util_get_binary_buffer(message_debug_data, &binary_buffer , &buffer_length) == CL_RETVAL_OK) {
                  sge_pack_buffer buf;
   
                  if (init_packbuffer_from_buffer(&buf, (char*)binary_buffer, buffer_length, false) == PACK_SUCCESS) {
                     u_long32 client_id = 0;
                     if (unpackint(&buf, &client_id) == PACK_SUCCESS) {
                        printf("      unpacked event client exit (binary buffer length %lu):\n", buffer_length );
                        printf("event client " sge_u32 " exit\n", client_id);
                     }
                     clear_packbuffer(&buf);
                  }
               }
            }
   
            if (strstr( cl_values[6] , "TAG_ACK_REQUEST") != nullptr ) {
               unsigned long buffer_length = 0;
               if (cl_util_get_binary_buffer(message_debug_data, &binary_buffer, &buffer_length) == CL_RETVAL_OK) {
                  sge_pack_buffer buf;
   
                  if (init_packbuffer_from_buffer(&buf, (char*)binary_buffer, buffer_length, false) == PACK_SUCCESS) {
                     lListElem *ack = nullptr;

                     while (pb_unused(&buf) > 0) {
                        if (cull_unpack_elem(&buf, &ack, nullptr)) {
                           printf("TAG_ACK_REQUEST: unpack error\n");
                        } else {
                           lWriteElemTo(ack, stdout); /* ack */
                           lFreeElem(&ack);
                        }
                     }

                     clear_packbuffer(&buf);
                  }
               }
            }
   
            if (strstr(cl_values[6] , "TAG_JOB_EXECUTION") != nullptr ||
                strstr(cl_values[6] , "TAG_SLAVE_ALLOW") != nullptr) {
               unsigned long buffer_length = 0;
               if (cl_util_get_binary_buffer(message_debug_data, &binary_buffer, &buffer_length) == CL_RETVAL_OK) {
                  sge_pack_buffer buf;
   
                  if (init_packbuffer_from_buffer(&buf, (char*)binary_buffer, buffer_length, false) == PACK_SUCCESS) {
                     if (strcmp(sender_comp_name, "qmaster") == 0) {
                        u_long32 feature_set;
                        lListElem *job = nullptr;
                        if (unpackint(&buf, &feature_set) == PACK_SUCCESS) {
                           printf("      unpacked %s (binary buffer length %lu):\n", cl_values[6], buffer_length);
                           printf("feature_set: " sge_u32 "\n", feature_set);
                        }
                        if (cull_unpack_elem(&buf, &job, nullptr) == PACK_SUCCESS) {
                           lWriteElemTo(job, stdout); /* job */
                        } else {
                           printf("could not unpack job\n");
                        }
                        lFreeElem(&job);
                     } else {
                        u_long32 feature_set;
                        lListElem *petr = nullptr;
                        if (unpackint(&buf, &feature_set) == PACK_SUCCESS) {
                           printf("      unpacked %s - PE TASK REQUEST (binary buffer length %lu):\n", cl_values[6], buffer_length);
                           printf("feature_set: " sge_u32 "\n", feature_set);
                        }
                        if (cull_unpack_elem(&buf, &petr, nullptr) == PACK_SUCCESS) {
                           lWriteElemTo(petr, stdout);
                        } else {
                           printf("could not unpack pe task request\n");
                        }
                        lFreeElem(&petr);
                     }
                     clear_packbuffer(&buf);
                  }
               }
            }
   
            if (strstr(cl_values[6] , "TAG_SIGJOB") != nullptr) {
               unsigned long buffer_length = 0;
               if (cl_util_get_binary_buffer(message_debug_data, &binary_buffer , &buffer_length) == CL_RETVAL_OK) {
                  sge_pack_buffer buf;
   
                  if (init_packbuffer_from_buffer(&buf, (char*)binary_buffer, buffer_length, false) == PACK_SUCCESS) {
                     u_long32 jobid    = 0;
                     u_long32 job_signal   = 0;
                     u_long32 jataskid = 0;
                     char *qname       = nullptr;
   
                     if (unpackint(&buf, &jobid) == PACK_SUCCESS) {
                        printf("jobid (JB_job_number):    " sge_u32 "\n", jobid);
                     }
                     if (unpackint(&buf, &jataskid) == PACK_SUCCESS) {
                        printf("jataskid (JAT_task_number): " sge_u32 "\n", jataskid);
                     }
                     if (unpackstr(&buf, &qname) == PACK_SUCCESS) {
                        if (qname != nullptr) {
                           printf("qname(QU_full_name):    \"%s\"\n", qname);
                        } else {
                           printf("qping: qname(QU_full_name) is nullptr !!!!\n");
                        }
                     }
   
                     if (unpackint(&buf, &job_signal) == PACK_SUCCESS) {
                        printf("signal:   " sge_u32 " (%s)\n", job_signal, sge_sig2str(job_signal));
                     }
   
                     if (qname) {
                        sge_free(&qname);
                     }
                     clear_packbuffer(&buf);
                  }
               }
            }
            
   
            if (strstr(cl_values[6], "TAG_SIGQUEUE") != nullptr) {
               unsigned long buffer_length = 0;
               if (cl_util_get_binary_buffer(message_debug_data, &binary_buffer , &buffer_length) == CL_RETVAL_OK) {
                  sge_pack_buffer buf;
   
                  printf("binary buffer length is %lu\n",buffer_length);  
   
                  if (init_packbuffer_from_buffer(&buf, (char*)binary_buffer, buffer_length, false) == PACK_SUCCESS) {
                     u_long32 jobid    = 0;
                     u_long32 queue_signal   = 0;
                     u_long32 jataskid = 0;
                     char *qname       = nullptr;
   
                     if (unpackint(&buf, &jobid) == PACK_SUCCESS) {
                        printf("jobid (0 - unused):    " sge_u32 "\n", jobid);
                     }
                     if (unpackint(&buf, &jataskid) == PACK_SUCCESS) {
                        printf("jataskid (0 - unused): " sge_u32 "\n", jataskid);
                     }
                     if (unpackstr(&buf, &qname) == PACK_SUCCESS) {
                        if (qname != nullptr) {
                           printf("qname(QU_full_name):    \"%s\"\n", qname);
                        } else {
                           printf("qping: qname(QU_full_name) is nullptr !!!!\n");
                        }
                     }
                     if (unpackint(&buf, &queue_signal) == PACK_SUCCESS) {
                        printf("signal:   " sge_u32 " (%s)\n", queue_signal, sge_sig2str(queue_signal));
                     }
   
                     if (qname) {
                        sge_free(&qname);
                     }
                     clear_packbuffer(&buf);
                  }
               }
            }
   
            
            
   #if 0
         CR: 
   
         qping TODO:
    
         a) Add a option do ignore hosts/components
   
         b) Following Tags are not unpacked
         case TAG_SLAVE_ALLOW
         case TAG_CHANGE_TICKET
         case TAG_SIGJOB
         case TAG_SIGQUEUE
         case TAG_KILL_EXECD
         case TAG_GET_NEW_CONF
         case TAG_FULL_LOAD_REPORT
         ...
         are missing !!!
   #endif
   
            printf("%s", bin_end);
            for(i=strlen(bin_end);i<full_width;i++) {
               printf("-");
            }
            printf("\n");
   
   
         } else {
            int spaces = 1;
            int new_line = -1;
            int x;
            const char* xml_start = "--- XML block start ";
            const char* xml_end   = "--- XML block end ";
            unsigned long message_debug_data_length = strlen(message_debug_data);
            printf("%s", xml_start);
            for(i=strlen(xml_start);i<full_width;i++) {
               printf("-");
            }
            printf("\n");
   
            for (i=0;i<message_debug_data_length;i++) {
               if (message_debug_data[i] == '<' && message_debug_data[i+1] != '/') {
                  if (new_line != -1) {
                     printf("\n");
                     spaces++;
                  }
                  new_line = 1;
               }
               if (message_debug_data[i] == '<' && message_debug_data[i+1] == '/' && spaces == 1) {
                  new_line = 1;
                  printf("\n");
               }
   
               if (new_line == 1) {
                  for(x=0;x<spaces;x++) {
                     printf("   ");
                  }
                  new_line = 0;
               }
               printf("%c", message_debug_data[i]);
               if (message_debug_data[i] == '<' && message_debug_data[i+1] == '/') {
                  spaces--;
               }
   
            }
            
            printf("\n%s", xml_end);
            for(i=strlen(xml_end);i<full_width;i++) {
               printf("-");
            }
            printf("\n");
         }
      }
      sge_free_saved_vars(context);
      return;
   }  /* end of CL_DMT_MESSAGE tag */

   if (debug_tag == CL_DMT_APP_MESSAGE && (dump_tag == 1 || dump_tag == 2)) {
      qping_convert_time(cl_values[0], time, sizeof(time), true);
      
      if (nonewline != 0) {
#if 0
         char* app_start = "--- APPLICATION debug block start ";
         char* app_end   = "--- APPLICATION debug block end ";
#endif
         
         printf("--- APP: %s: %s\n", time, cl_values[1]);
      }
      sge_free_saved_vars(context);
      return;
   }
   sge_free_saved_vars(context);
   return;
}


static void usage(int ret)
{
  size_t max_name_length = 0;
  size_t i=0;
  FILE* out = stdout;

  if (ret != 0) {
     out = stderr;
  }   

  fprintf(out, "%s %s\n", ocs::Version::get_short_product_name().c_str(), ocs::Version::get_version_string().c_str());
  fprintf(out, "%s qping [-help] [-noalias] [-ssl|-tcp] [ [ [-i <interval>] [-info] [-f] ] | [ [-dump_tag tag [param] ] [-dump] [-nonewline] ] ] <host> <port> <name> <id>\n",MSG_UTILBIN_USAGE);
  fprintf(out, "   -i         : set ping interval time\n");
  fprintf(out, "   -info      : show full status information and exit\n");
  fprintf(out, "   -f         : show full status information on each ping interval\n");
  fprintf(out, "   -noalias   : ignore $SGE_ROOT/SGE_CELL/common/host_aliases file\n");
  fprintf(out, "   -ssl       : use SSL framework\n");
  fprintf(out, "   -tcp       : use TCP framework\n");
  fprintf(out, "   -dump      : dump communication traffic (see \"communication traffic output options\" for additional information)\n");
  fprintf(out, "                   (provides the same output like -dump_tag MSG)\n");
  fprintf(out, "   -dump_tag  : dump communication traffic (see \"communication traffic output options\" for additional information)\n");
  fprintf(out, "                   tag=ALL <debug level> - show all\n");
  fprintf(out, "                   tag=APP <debug level> - show application messages\n");
  fprintf(out, "                   tag=MSG               - show commlib protocol messages\n");
  fprintf(out, "                   <debug level>         - ERROR, WARNING, INFO, DEBUG or DPRINTF\n");
  fprintf(out, "   -nonewline : dump output will not have a linebreak within a message\n");
  fprintf(out, "   -help      : show this info\n");
  fprintf(out, "   host       : host name of running component\n");
  fprintf(out, "   port       : port number of running component\n");
  fprintf(out, "   name       : name of running component (e.g.: \"qmaster\" or \"execd\")\n");
  fprintf(out, "   id         : id of running component (e.g.: 1 for daemons)\n\n");
  fprintf(out, "example:\n");
  fprintf(out, "   qping -info clustermaster 5000 qmaster 1\n\n");
  fprintf(out, "communication traffic output options:\n");
  fprintf(out, "   The environment variable SGE_QPING_OUTPUT_FORMAT can be used to hide columns and\n");
  fprintf(out, "   to set default column width. For hostname output the parameter hn is used.\n");
  fprintf(out, "   SGE_QPING_OUTPUT_FORMAT=\"h:1 h:4 w:1:20\"\n");
  fprintf(out, "   will hide the columns 1 and 4 and set the width of column 1 to 20 characters.\n");
  fprintf(out, "       h:X   -> hide column X\n");
  fprintf(out, "       s:X   -> show column X\n");
  fprintf(out, "       w:X:Y -> set width of column X to Y\n");
  fprintf(out, "       hn:X  -> set hostname output parameter X. X values are \"long\" or \"short\"\n\n");
  fprintf(out, "   available columns are:\n\n");
  
  qping_parse_environment();

  for (i=0;i<ARGUMENT_COUNT;i++) {
      if (max_name_length < strlen(cl_names[i])) {
         max_name_length = strlen(cl_names[i]);
      }
  }

  for(i=0;i<ARGUMENT_COUNT;i++) {
     const char *visible="yes";
     if (cl_show[i] == 0) {
        visible="no";
     }
     if (i==0) {
        fprintf(out, "   nr active ");
        qping_printf_fill_up(out, "name", max_name_length, ' ', 0);
        fprintf(out, " description\n");
        fprintf(out, "   == ====== ");
        qping_printf_fill_up(out, "====", max_name_length, ' ', 0);
        fprintf(out, " ===========\n");
     }
     fprintf(out, "   %02d %3s    ", (int)i + 1, visible);
     qping_printf_fill_up(out, cl_names[i], max_name_length, ' ', 0);
     fprintf(out, " %s\n",cl_description[i] );
  }
   
   if (ret == 1) {
      fprintf(out, "%s\n", MSG_PARSE_NOOPTIONARGUMENT);
   } 
   
   exit(ret);
}


int main(int argc, char *argv[]) {
   char* comp_host          = nullptr;
   char* resolved_comp_host = nullptr;
   char* comp_name          = nullptr;
   cl_com_handle_t* handle  = nullptr;
   cl_framework_t   communication_framework = CL_CT_TCP;
   cl_tcp_connect_t connect_type = CL_TCP_DEFAULT;
   cl_xml_connection_type_t connection_type = CL_CM_CT_MESSAGE;
   const char* client_name  = "qping";
#ifdef SECURE
   int   got_no_framework  = 0;
#endif

   int   parameter_start   = 1;
   int   comp_id           = -1;
   int   comp_port         = -1;
   int   interval          = 1;
   int   dump_tag          = 0;
   int   i                 = 0;
   int   exit_value        = 0;
   int   retval            = CL_RETVAL_OK;
   struct sigaction sa;
   int   option_f          = 0;
   int   option_info       = 0;
   int   option_noalias    = 0;
   int   option_ssl        = 0;
   int   option_tcp        = 0;
   int   option_dump       = 0;
   int   option_nonewline  = 1;
   int   option_debuglevel = 0;
   int   parameter_count   = 4;
   int   commlib_error = CL_RETVAL_OK;

   
   /* setup signalhandling */
   memset(&sa, 0, sizeof(sa));
   sa.sa_handler = sighandler_ping;  /* one handler for all signals */
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, nullptr);
   sigaction(SIGTERM, &sa, nullptr);
   sigaction(SIGHUP, &sa, nullptr);
   sigaction(SIGPIPE, &sa, nullptr);

   component_set_component_id(QPING);

   lInit(nmv);

   for (i=1;i<argc;i++) {
      if (argv[i][0] == '-') {
         if (strcmp( argv[i] , "-i") == 0) {
             parameter_count = parameter_count + 2;
             parameter_start = parameter_start + 2;
             i++;
             if ( argv[i] != nullptr) {
                interval = atoi(argv[i]);
                if (interval < 1) {
                   fprintf(stderr, "interval parameter must be larger than 0\n");
                   exit(1);
                }
             } else {
                fprintf(stderr, "no interval parameter value\n");
                exit(1);
             }
         }
         if (strcmp( argv[i] , "-info") == 0) {
             option_info = 1;
             parameter_count++;
             parameter_start++;
         }
         if (strcmp( argv[i] , "-f") == 0) {
             option_f = 1;
             parameter_count++;
             parameter_start++;
         }
         if (strcmp( argv[i] , "-noalias") == 0) {
             option_noalias = 1;
             parameter_count++;
             parameter_start++;
         }
         if (strcmp( argv[i] , "-tcp") == 0) {
             option_tcp = 1;
             parameter_count++;
             parameter_start++;
         }
         if (strcmp( argv[i] , "-ssl") == 0) {
             option_ssl = 1;
             parameter_count++;
             parameter_start++;
         }
         if (strcmp( argv[i] , "-dump") == 0) {
             option_dump = 1;
             dump_tag = 3;
             parameter_count++;
             parameter_start++;
         }
         if (strcmp( argv[i] , "-dump_tag") == 0) {
             option_dump = 1;
             parameter_count += 2;
             parameter_start += 2;
             i++;
             if ( argv[i] != nullptr) {
                if (strcmp(argv[i],"ALL") == 0) {
                   dump_tag = 1;
                   parameter_count++;
                   parameter_start++;
                   i++;
                   if (argv[i] != nullptr) {
                      if (strcmp(argv[i],"ERROR") == 0) {
                         option_debuglevel = 1;
                      }
                      if (strcmp(argv[i],"WARNING") == 0) {
                         option_debuglevel = 2;
                      }
                      if (strcmp(argv[i],"INFO") == 0) {
                         option_debuglevel = 3;
                      }
                      if (strcmp(argv[i],"DEBUG") == 0) {
                         option_debuglevel = 4;
                      }
                      if (strcmp(argv[i],"DPRINTF") == 0) {
                         option_debuglevel = 5;
                      }
                      if (option_debuglevel == 0) {
                         fprintf(stderr, "unexpected debug level\n");
                         exit(1);
                      }
                   }
                }
                if (strcmp(argv[i],"APP") == 0) {
                   dump_tag = 2;
                   parameter_count++;
                   parameter_start++;
                   i++;
                   if (argv[i] != nullptr) {
                      if (strcmp(argv[i],"ERROR") == 0) {
                         option_debuglevel = 1;
                      }
                      if (strcmp(argv[i],"WARNING") == 0) {
                         option_debuglevel = 2;
                      }
                      if (strcmp(argv[i],"INFO") == 0) {
                         option_debuglevel = 3;
                      }
                      if (strcmp(argv[i],"DEBUG") == 0) {
                         option_debuglevel = 4;
                      }
                      if (strcmp(argv[i],"DPRINTF") == 0) {
                         option_debuglevel = 5;
                      }
                      if (option_debuglevel == 0) {
                         fprintf(stderr, "unexpected debug level\n");
                         exit(1);
                      }
                   }
                }
                if (strcmp(argv[i],"MSG") == 0) {
                   dump_tag = 3;
                }
                if (dump_tag == 0) {
                   fprintf(stderr, "unexpected dump tag\n");
                   exit(1);
                }
             } else {
                fprintf(stderr, "no -dump_tag parameter value\n");
                exit(1);
             }
         }
         

         if (strcmp( argv[i] , "-nonewline") == 0) {
             option_nonewline = 0;
             parameter_count++;
             parameter_start++;
         }

         if (strcmp( argv[i] , "-help") == 0) {
             usage(0);
         }
      } else {
         break;
      }
   }

   if (argc != parameter_count + 1 ) {
      usage(1);
   }

   comp_host = argv[parameter_start];
   if (argv[parameter_start + 1] != nullptr) {
      comp_port = atoi(argv[parameter_start + 1]);
   }
   comp_name = argv[parameter_start + 2];
   if (argv[parameter_start + 3] != nullptr) {
      comp_id   = atoi(argv[parameter_start + 3]);
   }

   if ( comp_host == nullptr  ) {
      fprintf(stderr,"please enter a host name\n");
      exit(1);
   }

   if ( comp_name == nullptr  ) {
      fprintf(stderr,"please enter a component name\n");
      exit(1);
   }
   if ( comp_port < 0  ) {
      fprintf(stderr,"please enter a correct port number\n");
      exit(1);
   }
   if ( comp_id <= 0 ) {
      fprintf(stderr,"please enter a component id larger than 0\n");
      exit(1);
   }

   retval = cl_com_setup_commlib(CL_RW_THREAD ,CL_LOG_OFF, nullptr);
   if (retval != CL_RETVAL_OK) {
      fprintf(stderr,"%s\n",cl_get_error_text(retval));
      exit(1);
   }

   retval = cl_com_set_error_func(qping_general_communication_error);
   if (retval != CL_RETVAL_OK) {
      fprintf(stderr,"%s\n",cl_get_error_text(retval));
   }


   /* set alias file */
   if ( !option_noalias ) {
      const char *alias_path = sge_get_alias_path();
      if (alias_path != nullptr) {
         retval = cl_com_set_alias_file(alias_path);
         if (retval != CL_RETVAL_OK) {
            fprintf(stderr,"%s\n",cl_get_error_text(retval));
         }
         sge_free(&alias_path);
      }
   }

   if ( option_ssl != 0 && option_tcp != 0) {
      fprintf(stderr,"using of option -ssl and option -tcp not supported\n");
      exit(1);
   }
   

   /* find out the framework type to use */
   if ( option_ssl == 0 && option_tcp == 0 ) {
      char buffer[2*1024];
      dstring bw;
      sge_dstring_init(&bw, buffer, sizeof(buffer));

#ifdef SECURE
      got_no_framework = 1;
#endif
      if ( strcmp( "csp", bootstrap_get_security_mode()) == 0) {
         option_ssl = 1;
      } else {
         option_tcp = 1;
      }
   }

   if (option_ssl != 0) {
      communication_framework = CL_CT_SSL;
#ifdef SECURE
      if (got_no_framework == 1) {
         /* we got no framework and we have a bootstrap file */
         sge_getme(QPING);
         sge_ssl_setup_security_path("qping", component_get_username());
      } else {
         if (getenv("SSL_CA_CERT_FILE") == nullptr) {
            fprintf(stderr,"You have not set the SGE default environment or you specified the -ssl option.\n");
            fprintf(stderr,"Please set the following environment variables to specifiy your certificates:\n");
            fprintf(stderr,"- SSL_CA_CERT_FILE - CA certificate file\n");
            fprintf(stderr,"- SSL_CERT_FILE    - certificates file\n");
            fprintf(stderr,"- SSL_KEY_FILE     - key file\n");
            fprintf(stderr,"- SSL_RAND_FILE    - rand file\n");
            exit(1);
         } else {
            cl_ssl_setup_t ssl_config;
            ssl_config.ssl_method           = CL_SSL_v23;                 /*  v23 method                                  */
            ssl_config.ssl_CA_cert_pem_file = getenv("SSL_CA_CERT_FILE"); /*  CA certificate file                         */
            ssl_config.ssl_CA_key_pem_file  = nullptr;                       /*  private certificate file of CA (not used)   */
            ssl_config.ssl_cert_pem_file    = getenv("SSL_CERT_FILE");    /*  certificates file                           */
            ssl_config.ssl_key_pem_file     = getenv("SSL_KEY_FILE");     /*  key file                                    */
            ssl_config.ssl_rand_file        = getenv("SSL_RAND_FILE");    /*  rand file (if RAND_status() not ok)         */
            ssl_config.ssl_reconnect_file   = nullptr;                       /*  file for reconnect data    (not used)       */
            ssl_config.ssl_crl_file         = nullptr;                       /*  file for revocation list */
            ssl_config.ssl_refresh_time     = 0;                          /*  key alive time for connections (not used)   */
            ssl_config.ssl_password         = nullptr;                       /*  password for encrypted keyfiles (not used)  */
            ssl_config.ssl_verify_func      = nullptr;                       /*  function callback for peer user/name check  */
            cl_com_specify_ssl_configuration(&ssl_config);
         }
      }
#endif
   }
   if (option_tcp != 0) {
      communication_framework = CL_CT_TCP;
   }

   if (option_dump != 0) {
      connect_type = CL_TCP_RESERVED_PORT;
      connection_type = CL_CM_CT_STREAM;
      client_name  = "debug_client";
   }

   retval = cl_com_set_error_func(qping_general_communication_error);
   if (retval != CL_RETVAL_OK) {
      fprintf(stderr,"%s\n",cl_get_error_text(retval));
   }


   handle=cl_com_create_handle(&commlib_error, communication_framework, connection_type, false, comp_port, connect_type, client_name, 0, 1,0 );

   if (handle == nullptr) {
      fprintf(stderr, "could not create communication handle: %s\n", cl_get_error_text(commlib_error));
      cl_com_cleanup_commlib();
      exit(1);
   }

   if (option_dump == 0) {
      /* enable auto close of application */
      cl_com_set_auto_close_mode(handle, CL_CM_AC_ENABLED );
   }


   retval = cl_com_cached_gethostbyname(comp_host, &resolved_comp_host,nullptr, nullptr, nullptr);
   if (retval != CL_RETVAL_OK) {
      fprintf(stderr, "could not resolve hostname %s\n", comp_host);
      cl_com_cleanup_commlib();
      exit(1);
   }

   if (option_dump == 0) {
      while (do_shutdown == 0 ) {
         cl_com_SIRM_t* status = nullptr;
         retval = cl_commlib_get_endpoint_status(handle, resolved_comp_host , comp_name, comp_id, &status);
         if (retval != CL_RETVAL_OK) {
            printf("endpoint %s/%s/%d at port %d: %s\n",
                   resolved_comp_host, comp_name, comp_id, comp_port,
                   cl_get_error_text(retval) );  
            exit_value = 1;
         } else {
            if (status != nullptr) {
               char buffer[512];
               dstring ds;
               sge_dstring_init(&ds, buffer, sizeof(buffer));
   
               printf("%s", sge_ctime64(0, &ds));
   
               if (option_info == 0 && option_f == 0) {
                  printf(" endpoint %s/%s/%d at port %d is up since %ld seconds\n", 
                         resolved_comp_host, comp_name, comp_id, comp_port,
                         status->runtime);  
               } else {
                  u_long64 starttime = sge_gmt32_to_gmt64(status->starttime);
                  
                  printf(":\nSIRM version:             %s\n", status->version );
                  printf("SIRM message id:          %ld\n", status->mid);
                  printf("start time:               %s (" sge_u64 ")\n", sge_ctime64(starttime, &ds), starttime);
                  printf("run time [s]:             %ld\n", status->runtime);
                  printf("messages in read buffer:  %ld\n", status->application_messages_brm);
                  printf("messages in write buffer: %ld\n", status->application_messages_bwm);
                  printf("nr. of connected clients: %ld\n", status->application_connections_noc);
                  printf("status:                   %ld\n", status->application_status);
                  printf("info:                     %s\n",  status->info );
                  printf("\n");
               }
            } else {
               printf("unexpected error\n");
            }
         }
   
         cl_com_free_sirm_message(&status);
   
         if (option_info != 0) {
            break;
         }
         sleep(interval);
      }
   } else {
      dstring line_buffer = DSTRING_INIT;

      qping_parse_environment();

      while (do_shutdown == 0 ) {
         int                retval  = 0;
#if 0
         static int stop_running = 15;
#endif
         cl_com_message_t*  message = nullptr;
         cl_com_endpoint_t* sender  = nullptr;

#if 0
         if (stop_running-- == 0) {
            do_shutdown = 1;
         }
#endif

         cl_commlib_trigger(handle, 1);
         retval = cl_commlib_receive_message(handle, nullptr, nullptr, 0,      /* handle, comp_host, comp_name , comp_id, */
                                             false, 0,                 /* syncron, response_mid */
                                             &message, &sender );
         
         if ( retval != CL_RETVAL_OK) {
            if ( retval == CL_RETVAL_CONNECTION_NOT_FOUND ) {
                char command_buffer[256];
                cl_byte_t *reference = (cl_byte_t *)command_buffer;
                printf("open connection to \"%s/%s/%d\" ... " ,resolved_comp_host , comp_name, comp_id);
                retval = cl_commlib_open_connection(handle, resolved_comp_host , comp_name, comp_id);
                printf("%s\n", cl_get_error_text(retval));
                if (retval == CL_RETVAL_CREATE_RESERVED_PORT_SOCKET) {
                   printf("please start qping as root\n");
                   break;       
                }


                /*
                 * set message tag we want to receive from endpoint 
                 */
                switch(dump_tag) {
                   case 1: {
                      snprintf(command_buffer,256,"set tag ALL");
                      break;
                   }
                   case 2: {
                      snprintf(command_buffer,256,"set tag APP");
                      break;
                   }
                   case 3: {
                      snprintf(command_buffer,256,"set tag MSG");
                      break;
                   }
                }
                cl_commlib_send_message(handle,
                                    resolved_comp_host, comp_name, comp_id,
                                    CL_MIH_MAT_NAK, 
                                    &reference, strlen(command_buffer)+1, 
                                    nullptr, 0, 0, true, false);

                /*
                 * set if we want the message dump 
                 */

                if (cl_show[11] != 0) {
                   snprintf(command_buffer,256,"set dump ON");
                } else {
                   snprintf(command_buffer,256,"set dump OFF");
                }
                cl_commlib_send_message(handle,
                                    resolved_comp_host, comp_name, comp_id,
                                    CL_MIH_MAT_NAK, 
                                    &reference, strlen(command_buffer)+1, 
                                    nullptr, 0, 0, true, false);

                            
                /*
                 * set debug level 
                 */
                switch(option_debuglevel) {
                   case 1: {
                      snprintf(command_buffer,256,"set debug ERROR");
                      break;
                   }
                   case 2: {
                      snprintf(command_buffer,256,"set debug WARNING");
                      break;
                   }
                   case 3: {
                      snprintf(command_buffer,256,"set debug INFO");
                      break;
                   }
                   case 4: {
                      snprintf(command_buffer,256,"set debug DEBUG");
                      break;
                   }
                   case 5: {
                      snprintf(command_buffer,256,"set debug DPRINTF");
                      break;
                   }
                   default: {
                      snprintf(command_buffer,256,"set debug OFF");
                      break;
                   }
                }

                cl_commlib_send_message(handle,
                                    resolved_comp_host, comp_name, comp_id,
                                    CL_MIH_MAT_NAK, 
                                    &reference, strlen(command_buffer)+1, 
                                    nullptr, 0, 0, true, false);
            }
         } else {
            size_t i;
            for (i=0; i < message->message_length; i++) {
               sge_dstring_append_char(&line_buffer, message->message[i]);
               if (message->message[i] == '\n') {
                  qping_print_line(sge_dstring_get_string(&line_buffer), option_nonewline, dump_tag, sender->comp_name);
                  sge_dstring_free(&line_buffer);
               }
            }
            fflush(stdout);
            cl_com_free_message(&message);
            cl_com_free_endpoint(&sender);
         }
      }
      sge_dstring_free(&line_buffer);
   }
   retval = cl_commlib_shutdown_handle(handle,false);
   if (retval != CL_RETVAL_OK) {
      fprintf(stderr,"%s\n",cl_get_error_text(retval));
      sge_free(&resolved_comp_host);
      resolved_comp_host = nullptr;
      cl_com_cleanup_commlib();
      exit(1);
   }

   retval = cl_com_cleanup_commlib();
   if (retval != CL_RETVAL_OK) {
      fprintf(stderr,"%s\n",cl_get_error_text(retval));
      sge_free(&resolved_comp_host);
      resolved_comp_host = nullptr;
      exit(1);
   }
   sge_free(&resolved_comp_host);
   resolved_comp_host = nullptr;
   
   sge_prof_cleanup();
   return exit_value;  
}
