

/* xorriso - creates, loads, manipulates and burns ISO 9660 filesystem images.

   Copyright 2007-2025 Thomas Schmitt, <scdbackup@gmx.net>

   Provided under GPL version 2 or later.

   This file contains functions which are needed to read data
   from ISO image.
*/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include <fcntl.h>
#include <utime.h>

/* O_BINARY is needed for Cygwin but undefined elsewhere */
#ifndef O_BINARY
#define O_BINARY 0
#endif


#include "lib_mgt.h"
#include "drive_mgt.h"
#include "iso_img.h"
#include "iso_tree.h"
#include "iso_manip.h"
#include "read_run.h"
#include "sort_cmp.h"
#include "disk_ops.h"


int Xorriso__read_pacifier(IsoImage *image, IsoFileSource *filesource)
{
 struct XorrisO *xorriso;

 xorriso= (struct XorrisO *) iso_image_get_attached_data(image);
 if(xorriso==NULL)
   return(1);
 Xorriso_process_msg_queues(xorriso,0);
 xorriso->pacifier_count++;
 if(xorriso->pacifier_count%10)
   return(1);
 Xorriso_pacifier_callback(xorriso, "nodes read", xorriso->pacifier_count, 0,
                           "", 0);
 return(1);
}


/* @param flag bit0= open IsoNode *node_pt rather than looking up pathname
               bit1= dig out the most original stream for reading
*/
int Xorriso_iso_file_open(struct XorrisO *xorriso, char *pathname,
                          void *node_pt, void **stream, int flag)
{
 int ret;
 char *eff_path= NULL;
 IsoNode *node= NULL;
 IsoFile *filenode= NULL;
 IsoStream *iso_stream= NULL, *input_stream;

 Xorriso_alloc_meM(eff_path, char, SfileadrL);

 *stream= NULL;
 if(flag&1) {
   node= (IsoNode *) node_pt;
 } else {
   ret= Xorriso_get_node_by_path(xorriso, pathname, eff_path, &node, 0);
   if(ret<=0)
     goto ex;
 }
 if(!LIBISO_ISREG(node)) {
   sprintf(xorriso->info_text,
           "Given path does not lead to a regular data file in the image");
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
   {ret= 0; goto ex;}
 }

 filenode= (IsoFile *) node;
 iso_stream= iso_file_get_stream(filenode);
 if(iso_stream==NULL) {
   Xorriso_process_msg_queues(xorriso,0);
   sprintf(xorriso->info_text,
           "Could not obtain source stream of file in the image for reading");
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
   {ret= 0; goto ex;}
 }
 if(flag & 2) {
   /* Dig out the most original stream */
   while(1) {
     input_stream= iso_stream_get_input_stream(iso_stream, 0);
     if(input_stream == NULL)
   break;
     iso_stream= input_stream;
   }
 }
 if(!iso_stream_is_repeatable(iso_stream)) {
   sprintf(xorriso->info_text,
           "The data production of the file in the image is one-time only");
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
   {ret= 0; goto ex;}
 }
 ret= iso_stream_open(iso_stream);
 if(ret<0) {
   sprintf(xorriso->info_text,
           "Could not open data file in the image for reading");
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
   {ret= 0; goto ex;}
 }
 Xorriso_process_msg_queues(xorriso,0);
 *stream= iso_stream;

#ifdef NIX
 /* <<< */
 {
   unsigned int fs_id;
   dev_t dev_id;
   ino_t ino;

   iso_stream_get_id(iso_stream, &fs_id, &dev_id, &ino);
   fprintf(stderr, "xorriso_debug: iso_ino= %ld\n", (long int) ino);
 }
#endif

 ret= 1;
ex:;
 Xorriso_free_meM(eff_path);
 return(ret);
}


int Xorriso_iso_file_read(struct XorrisO *xorriso, void *stream, char *buf,
                          int count, int flag)
{
 int ret, rcnt= 0;
 IsoStream *stream_pt;

 stream_pt= (IsoStream *) stream;

 while(rcnt<count) {
   ret= iso_stream_read(stream_pt, (void *) (buf+rcnt), (size_t) (count-rcnt));
   if(ret==0) /* EOF */
 break;
   if(ret<0) { /* error */
     Xorriso_process_msg_queues(xorriso,0);
     Xorriso_report_iso_error(xorriso, "", ret, "Error on read",
                              0, "FAILURE", 1 | ((ret == -1)<<2) );
     return(-1);
   }
   rcnt+= ret;
 }
 return(rcnt);
}


int Xorriso_iso_file_close(struct XorrisO *xorriso, void **stream, int flag)
{
 int ret;

 if(*stream==NULL)
   return(0);
 ret= iso_stream_close(*stream);
 if(ret==1)
   *stream= NULL;
 Xorriso_process_msg_queues(xorriso,0);
 return(ret);
}


/* @param flag bit0= in_node is valid, do not resolve img_path
               bit1= test mode: print DEBUG messages
   @return  <0 = error,
             0 = surely not identical regular files ,
             1 = surely identical
             2 = potentially depending on unknown disk file (e.g. -cut_out)
*/
int Xorriso_restore_is_identical(struct XorrisO *xorriso, void *in_node,
                                 char *img_path, char *disk_path,
                                 char type_text[5], int flag)
{
 int ret;
 unsigned int fs_id;
 dev_t dev_id;
 ino_t ino_id;
 IsoStream *stream;
 IsoImage *volume;
 IsoNode *node;
 struct stat stbuf;
 off_t dummy;

 memset(type_text, 0, 5);
 if(!Xorriso_change_is_pending(xorriso, 0))
   return(0);
 if(flag&1) {
   node= (IsoNode *) in_node;
 } else {
   ret= Xorriso_get_volume(xorriso, &volume, 0);
   if(ret<=0)
     return(-1);
   ret= Xorriso_node_from_path(xorriso, volume, img_path, &node, 1);
   if(ret<=0)
     return(-1);
 }
 ret= Xorriso__file_start_lba(node, &dummy, 0);
 if(ret != 0) {
   Xorriso_process_msg_queues(xorriso, 0);
   return(0);
 }
 if(!LIBISO_ISREG(node))
   return(0);
 stream= iso_file_get_stream((IsoFile *) node);
 memcpy(type_text, stream->class->type, 4);
 iso_stream_get_id(stream, &fs_id, &dev_id, &ino_id);
 if(flag&2) {
   sprintf(xorriso->info_text, "%s : fs=%d  dev=%.f  ino=%.f  (%s)",
           img_path, fs_id, (double) dev_id, (double) ino_id, type_text);
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "DEBUG", 0);
 }
 ret= stat(disk_path, &stbuf);
 if(ret==-1)
   return(0);
 if(flag&2) {
   sprintf(xorriso->info_text, "%s :       dev=%.f  ino=%.f",
           disk_path, (double) stbuf.st_dev, (double) stbuf.st_ino);
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "DEBUG", 0);
 }
 if(fs_id!=1)
   return(2);

 /* >>> obtain underlying dev_t ino_t of type "cout" */;

 if(strcmp(type_text, "fsrc")!=0)
   return(2);
 if(stbuf.st_dev==dev_id && stbuf.st_ino==ino_id)
   return(1);
 return(0);
}


int Xorriso_iso_file_to_fd(struct XorrisO *xorriso, char *path, int fd, 
                           int flag)
{
 int ret, rret, wret, to_write, wanted;
 void *stream= NULL;
 char *buffer= NULL, *wpt;
 off_t todo;
 static int buffer_size= 64 * 1024;

 Xorriso_alloc_meM(buffer, char, buffer_size);

 ret= Xorriso_iso_file_open(xorriso, path, NULL, &stream, 0);
 if(ret <= 0)
   goto ex;

 todo= iso_stream_get_size((IsoStream *) stream);
 while(todo > 0) {
   if(todo < buffer_size)
     wanted= todo;
   else 
     wanted= buffer_size;
   rret = Xorriso_iso_file_read(xorriso, stream, buffer, wanted, 0);
   if(rret <= 0)
     {ret= -1; goto ex;}
   todo-= rret;
   wpt= buffer;
   for(to_write= rret; to_write > 0;) {
     wret= write(fd, wpt, to_write);
     if(wret <= 0) {
       if(wret == 0)
         sprintf(xorriso->info_text,
                 "Strange behavior of write(2): return == 0 with ");
       else
         sprintf(xorriso->info_text, "Write error with ");
       Text_shellsafe(path, xorriso->info_text, 1);
       Xorriso_msgs_submit(xorriso, 0, xorriso->info_text,
                           wret == 0 ? 0 : errno, "FAILURE",0);
       ret= 0; goto ex;
     }
     to_write-= wret;
     wpt+= wret;
   }
 }
 ret= 1;
ex:;
 if(stream != NULL)
   Xorriso_iso_file_close(xorriso, &stream, 0);
 Xorriso_free_meM(buffer);
 return(ret);
}


/*
   @flag   bit0= do not report error in case of error worth of single flag try
   @return 0=failure, 1=ok, 2=pardoned,
                      3=pardoned error worth of single flag try
*/
int Xorriso_report_chattr_outcome(struct XorrisO *xorriso, char *disk_path,
                                  uint64_t lfa_flags, uint64_t lfa_mask,
                                  int iso_ret, int os_errno, int flag)
{
 int ret, eps_ret, sev, lfa_error= 0;
 char msg[101], *lfa_text= NULL, severity[20];

 if(iso_ret == 1)
   {ret= 1; goto ex;}

 if(strcmp(xorriso->lfa_restore_err_sev, "silent") == 0 ||
    strcmp(xorriso->lfa_restore_err_sev, "all") == 0)
   {ret= 2; goto ex;}
 Xorriso__to_upper(xorriso->lfa_restore_err_sev, severity, sizeof(severity),
                   0);

 ret= iso_util_encode_lfa_flags(lfa_flags & lfa_mask, &lfa_text, 0);
 if(lfa_text == NULL)
   lfa_text= strdup("-unknown-attributes-");
 if(iso_ret < 0) {
   if(iso_ret == (int) ISO_LFA_NO_SET_LOCAL) {
     lfa_error= 1;
     if(flag & 1)
       {ret= 3; goto ex;}
   }
   strcpy(msg, "Could not set chattr '");
   if(lfa_text != NULL)
     strcat(msg, lfa_text);
   strcat(msg, "'");
   /* Adjust severity to event_pt.
      Number 0x7f000000 comes from libisofs.h, iso_error_get_severity */
   Xorriso__text_to_sev(severity, &sev, 0);
   iso_ret&= ~0x7f000000;
   iso_ret|= sev;
   Xorriso_report_iso_error(xorriso, disk_path, iso_ret, msg, os_errno,
                            severity, 2);
   ret= 0; goto ex;
 } else if(iso_ret == 2) {
    sprintf(xorriso->info_text,
            "Could not map all of chattr '%s' to local file attributes of ",
            lfa_text);
 } else if(iso_ret == 3) {
    sprintf(xorriso->info_text, "Will not apply chattr '%s' to symbolic link ",
                                 lfa_text);
 } else {
    sprintf(xorriso->info_text,
 "Unknown return value %d from iso_local_set_lfa_flags() with chattr '%s' to ",
                               iso_ret, lfa_text);
 }
 Text_shellsafe(disk_path, xorriso->info_text, 1);
 Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, severity, 0);
 ret= 0;
ex:;
 if(ret == 0) {
   eps_ret= Xorriso_eval_problem_status(xorriso, 0, 1);
   if(eps_ret >= 0)
     ret= 2 + !!lfa_error;
 }
 if(lfa_text != NULL)
   free(lfa_text);
 return(ret);
}


/*
   @param flag bit0= try single attribute flags if the whole change_mask fails
                     possibly because of inappropriate attributes
   @return 0=failure
           1= ok
           2= pardoned
           3= pardoned error worth of single flag try
           4= with bit0: partial success
*/
int Xorriso_local_set_lfa_flags(struct XorrisO *xorriso, char *disk_path,
                                uint64_t lfa_flags, int max_bit,
                                uint64_t change_mask, int *os_errno, int flag)
{
 int ret, partial_success= 0, os_errno_mem, i, count;
 uint64_t single_mask, single_lfa;

 ret= iso_local_set_lfa_flags(disk_path, lfa_flags, max_bit, change_mask,
                              os_errno, 4);
 ret= Xorriso_report_chattr_outcome(xorriso, disk_path, lfa_flags, change_mask,
                                    ret, *os_errno, flag & 1);
 if(ret != 3 || !(flag & 1))
   return(ret);

 /* Try with single flag calls */
 count= 0;
 for(i= 0; i <= max_bit && i < 64; i++)
   if(change_mask & (((uint64_t) 1) << i))
     count++;
 if(count < 2)
   return(ret);  /* was already a single flag call */

 os_errno_mem= *os_errno;
 for(i= 0; i <= max_bit && i < 64; i++) {
   single_mask= ((uint64_t) 1) << i;
   if(!(single_mask & change_mask))
 continue;
   single_lfa= lfa_flags & single_mask;
   *os_errno= 0;
   ret= iso_local_set_lfa_flags(disk_path, single_lfa, max_bit, single_mask,
                                os_errno, 4);
   ret= Xorriso_report_chattr_outcome(xorriso, disk_path, single_lfa,
                                      single_mask, ret, *os_errno, 0);
   if(ret <= 0)
     return(ret);
   if(ret == 1)
     partial_success= 1;
 }
 *os_errno= os_errno_mem;
 return(3 + !!partial_success);
}


uint64_t Xorriso__lfa_bits(char *lfa_text)
{
 int ret;
 uint64_t lfa_bits;

 ret= iso_util_decode_lfa_flags(lfa_text, &lfa_bits, 0);
 if(ret < 0)
   lfa_bits= 0;
 return(lfa_bits);
}


/* @param flag bit0= set i
               bit1= set a
*/
int Xorriso_set_local_chattr_ia(struct XorrisO *xorriso, char *disk_path,
                               int flag)
{
 int ret, max_bit= 31, os_errno;
 uint64_t lfa_flags= 0;
 static uint64_t lfa_i= 0xffffffff, lfa_a= 0;

 if(lfa_i == 0xffffffff) {
   lfa_i= Xorriso__lfa_bits("i");
   lfa_a= Xorriso__lfa_bits("a");
 }
 if(flag & 1)
  lfa_flags|= lfa_i;
 if(flag & 2)
  lfa_flags|= lfa_a;
 if(lfa_flags == 0)
   return(1);
 ret= Xorriso_local_set_lfa_flags(xorriso, disk_path, lfa_flags, max_bit,
                                  lfa_flags, &os_errno, 0);
 if(ret <= 0)
   return(ret);
 return(1);
}


/* If present and enabled: restore chattr i and/or a */
int Xorriso_restore_chattr_ia(struct XorrisO *xorriso, IsoNode *node,
                             char *disk_path, int flag)
{
 int ret, max_bit;
 uint64_t lfa_flags, mask, chattr_flag= 0;
 static uint64_t lfa_i= 0xffffffff, lfa_a= 0;

 if(lfa_i == 0xffffffff) {
   lfa_i= Xorriso__lfa_bits("i");
   lfa_a= Xorriso__lfa_bits("a");
 }

 if((xorriso->do_aaip & (1 << 12))) {
   mask= iso_util_get_effective_lfa_mask(xorriso->lfa_restore_mask,
                                    (!!(xorriso->do_aaip & (1 << 13))) |
                                    ((!!(xorriso->do_aaip & (1 << 14))) << 1));
   if(!(mask & (lfa_i | lfa_a)))
     return(2);
   ret= iso_node_get_lfa_flags(node, &lfa_flags, &max_bit, 0);
   if(ret > 0) {
     if(lfa_flags & lfa_i)
       chattr_flag|= 1;
     if(lfa_flags & lfa_a)
       chattr_flag|= 2;
     if(chattr_flag) {
       ret= Xorriso_set_local_chattr_ia(xorriso, disk_path, chattr_flag);
       if(ret <= 0)
         return(ret);
       return(1);
     }
   }
 }
 return(2);
}


int Xorriso_early_chattr_CF(struct XorrisO *xorriso, IsoNode *node,
                            char *disk_path, int *os_errno, int flag)
{
 int ret, max_bit;
 uint64_t lfa_flags, xorriso_mask, set_mask;
 static uint64_t lfa_C= 0xffffffff, lfa_F= 0;

 if(lfa_C == 0xffffffff) {
   lfa_C= Xorriso__lfa_bits("C");
   lfa_F= Xorriso__lfa_bits("F");
 }

 *os_errno= 0;
 if(!(xorriso->lfa_flags_setting & (1 << 12)))
   return(2);

 ret= iso_node_get_lfa_flags(node, &lfa_flags, &max_bit, 0);
 if(ret <= 0)
   return(3);
 xorriso_mask= iso_util_get_effective_lfa_mask(xorriso->lfa_restore_mask,
                                    (!!(xorriso->do_aaip & (1 << 13))) |
                                    ((!!(xorriso->do_aaip & (1 << 14))) << 1));
 set_mask= 0;
 /* C must be set on empty regular files.
    But if it would be set for a directory, all its files would get set C,
    even if their IsoNode does not have it.
 */
 if(!LIBISO_ISDIR(node))
   set_mask|= lfa_C;
 /* F must be set on empty directories. (It is undefined for regular files) */
 if(LIBISO_ISDIR(node))
   set_mask|= lfa_F;
 set_mask&= xorriso_mask;
 if(set_mask == 0)
   return(4);
 ret= Xorriso_local_set_lfa_flags(xorriso, disk_path, lfa_flags, max_bit,
                                  set_mask, os_errno,
                                  !!(xorriso->do_aaip & (1 << 16)));
 if(ret <= 0)
   return(ret);
 return(1);
}


int Xorriso_restore_timestamps(struct XorrisO *xorriso, char *disk_path,
                               IsoNode *node, int flag)
{
 int ret;
 struct utimbuf utime_buffer;

 utime_buffer.actime= iso_node_get_atime(node);
 utime_buffer.modtime= iso_node_get_mtime(node);
 ret= utime(disk_path, &utime_buffer);
 if(ret == -1) {
   sprintf(xorriso->info_text,
           "Cannot change atime, mtime of disk file ");
   Text_shellsafe(disk_path, xorriso->info_text, 1);
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, "FAILURE", 0);
   return(0);
 }
 return(1);
}


/* @param flag bit0= minimal transfer: access permissions only
               bit1= keep directory open: keep owner, allow rwx for owner
                     and push directory onto xorriso->perm_stack
   @return  1=ok , 2=ok, lfa "i" and/or "a" pushed to permstack, <=0 error
*/
int Xorriso_restore_properties(struct XorrisO *xorriso, char *disk_path,
                                IsoNode *node, int flag)
{
 int ret, is_dir= 0, errno_copy= 0, local_attrs_set= 0, i, err_count;
 int lfa_ia_pushed= 0;
 mode_t mode;
 uid_t uid, disk_uid;
 gid_t gid, disk_gid;
 struct stat stbuf;
 size_t num_attrs= 0, *value_lengths= NULL;
 char **names= NULL, **values= NULL;
 int *errnos= NULL;
 uint64_t lfa_flags= 0, mask= 0, push_mask= 0;
 int max_bit, os_errno;
 uint32_t projid;
 off_t off_t_projid;
 static uint64_t lfa_C= 0xffffffff, lfa_i= 0, lfa_a= 0, lfa_F= 0;

 if(lfa_C == 0xffffffff) {
   lfa_C= Xorriso__lfa_bits("C");
   lfa_F= Xorriso__lfa_bits("F");
   lfa_i= Xorriso__lfa_bits("i");
   lfa_a= Xorriso__lfa_bits("a");
 }

 ret= lstat(disk_path, &stbuf);
 if(ret==-1) {
   sprintf(xorriso->info_text,
           "Cannot obtain properties of disk file ");
   Text_shellsafe(disk_path, xorriso->info_text, 1);
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, "FAILURE", 0);
   {ret= 0; goto ex;}
 }
 disk_uid= uid= stbuf.st_uid;
 disk_gid= stbuf.st_gid;

 is_dir= S_ISDIR(stbuf.st_mode);

 mode= iso_node_get_permissions(node);

 if(xorriso->do_aaip & (2 | 8 | 16)) {
   ret= iso_node_get_attrs(node, &num_attrs, &names, &value_lengths, &values, 
           (!!(xorriso->do_aaip & 2)) | (!(xorriso->do_aaip & (8 | 16))) << 2);
   if (ret < 0) {
     Xorriso_process_msg_queues(xorriso, 0);
     strcpy(xorriso->info_text, "Error with obtaining ACL and xattr for ");
     Text_shellsafe(disk_path, xorriso->info_text, 1);
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
     {ret= 0; goto ex;}
   }
   if(num_attrs > 0) {
     Xorriso_alloc_meM(errnos, int, num_attrs);
     ret= iso_local_set_attrs_errno(disk_path, num_attrs, names, value_lengths,
                                    values, errnos,
                                    ((!(xorriso->do_strict_acl & 1)) << 6) |
                                    ((!!(xorriso->do_aaip & 1024)) << 3) );
     if(ret < 0) {
cannot_set_xattr:;
       errno_copy= errno;
       if(ret != (int) ISO_AAIP_NO_SET_LOCAL)
         errno_copy= 0;
       Xorriso_report_iso_error(xorriso, "", ret,
                                "Error on iso_local_set_attrs",
                                0, "FAILURE", 1 | ((ret == -1)<<2) );
       sprintf(xorriso->info_text, "Disk file ");
       Text_shellsafe(disk_path, xorriso->info_text, 1);
       err_count= 0;
       for(i= 0; (unsigned int) i < num_attrs; i++) {
         if(errnos[i] == 0)
       continue;
         if(err_count >= 3) {
           strcat(xorriso->info_text, " , and more");
       break;
         }
         err_count++;
         errno_copy= 0; /* Detail errno overrides final errno */
         if(names[i][0] == 0)
           sprintf(xorriso->info_text + strlen(xorriso->info_text), " , ACL ");
         else
           sprintf(xorriso->info_text + strlen(xorriso->info_text),
                   " , xattr %s ", names[i]);
         if(errnos[i] < 0)
           Text_shellsafe("Unknown error", xorriso->info_text, 1);
         else
           Text_shellsafe(strerror(errnos[i]), xorriso->info_text, 1);
       }
       Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno_copy,
                           "FAILURE",0);
       {ret= 0; goto ex;}
     }
     local_attrs_set= 1;
   }
   Xorriso_process_msg_queues(xorriso,0);
 }
 if(!(xorriso->do_aaip & 2))
   mode= iso_node_get_perms_wo_acl(node);

 /* Need to assess lfa_flags early because 'i' might need to be pushed to
    the perm stack */
 if(xorriso->do_aaip & (1 << 12)) {
   ret= iso_node_get_lfa_flags(node, &lfa_flags, &max_bit, 0);
   if (ret < 0) {
     Xorriso_process_msg_queues(xorriso, 0);
     strcpy(xorriso->info_text,
            "Error with obtaining file attribut flags for ");
     Text_shellsafe(disk_path, xorriso->info_text, 1);
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
     {ret= 0; goto ex;}
   } else if(ret > 0) {
     mask= iso_util_get_effective_lfa_mask(xorriso->lfa_restore_mask,
                                    (!!(xorriso->do_aaip & (1 << 13))) |
                                    ((!!(xorriso->do_aaip & (1 << 14))) << 1));
     /* Do not set 'C' of non-directories or 'F' of directories here.
        It would be too late. */
     if(!is_dir)
       mask&= ~lfa_C;
     if(is_dir)
       mask&= ~lfa_F;
     push_mask= mask;
     /* Do not set lfa_flags 'i' or 'a' of a directory here.
        It would be too early. */
     if(is_dir)
       mask&= ~(lfa_i | lfa_a);
   }
 }

 if(is_dir && (flag&2)) {
   ret= Xorriso_fake_stbuf(xorriso, "", &stbuf, &node,
                           1 | ((!!(xorriso->do_aaip & 2)) << 3));
   if(ret<=0)
     {ret= 0; goto ex;}
   ret= Permstack_push(&(xorriso->perm_stack), disk_path, &stbuf,
                       (!!(lfa_flags & push_mask & lfa_i)) |
                       ((!!(lfa_flags & push_mask & lfa_a)) << 2));
   if(ret<=0) {
     Xorriso_msgs_submit(xorriso, 0, disk_path, 0, "ERRFILE", 0);
     strcpy(xorriso->info_text,
            "Cannot memorize permissions for disk directory");
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FATAL", 0);
     {ret= -1; goto ex;}
   }
   lfa_ia_pushed= !!(lfa_flags & push_mask & (lfa_i | lfa_a));
   mode|= S_IRUSR|S_IWUSR|S_IXUSR;
 }
 ret= chmod(disk_path, mode);
 if(ret==-1) {
cannot_set_perm:;
   sprintf(xorriso->info_text,
           "Cannot change access permissions of disk file ");
   Text_shellsafe(disk_path, xorriso->info_text, 1);
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, "FAILURE", 0);
   {ret= 0; goto ex;}
 }

 if(flag&1)
   {ret= 1; goto ex;}

 ret= Xorriso_restore_timestamps(xorriso, disk_path, node, 0);
 if(ret <= 0)
   goto ex;
 
 gid= iso_node_get_gid(node);
 if(!(S_ISDIR(stbuf.st_mode) && (flag&2)))
   uid= iso_node_get_uid(node);

 if(uid != disk_uid || gid != disk_gid) {

   ret= chown(disk_path, uid, gid); /* don't complain if it fails */

   /* Check whether xattr are still set and try to set them again if needed.
      E.g. Linux 3.16 removes security.capability on chown(2).
    */
   if(local_attrs_set && (xorriso->do_aaip & 1024)) {
     ret= iso_local_set_attrs_errno(disk_path, num_attrs, names, value_lengths,
                                    values, errnos,
                                    1 | ((!!(xorriso->do_aaip & 1024)) << 3) |
                                    128);
     if(ret < 0)
       goto cannot_set_xattr;
   }

   /* Check whether setuid or setgid bits got reset */
   ret= lstat(disk_path, &stbuf);
   if(ret != -1) {
     if((mode ^ stbuf.st_mode) & (S_ISUID | S_ISGID)) {
       ret= chmod(disk_path, mode);
       if(ret==-1)
         goto cannot_set_perm;
     }
   }
 }

 if(xorriso->do_aaip & (1 << 12)) {
   if(mask != 0) {
     ret= Xorriso_local_set_lfa_flags(xorriso, disk_path, lfa_flags, max_bit,
                                      mask, &os_errno,
                                      !!(xorriso->do_aaip & (1 << 16)));
     if(ret <= 0)
       goto ex;
   }
 }

 if(xorriso->do_aaip & (1 << 17)) {
   ret= Xorriso_get_projid(xorriso, node, NULL, &projid, 0);
   if(ret < 0)
     goto ex;
   ret= Numbermapper_map(xorriso->projid_mapper, (off_t) projid, &off_t_projid,
                         0);
   if(ret < 0)
     goto ex;
   projid= off_t_projid;
   if(projid != 0 || (xorriso->do_aaip & (1 << 18))) {
     ret= iso_local_set_projid(disk_path, projid, &os_errno, 0);
     if(ret < 0) {
       Xorriso_process_msg_queues(xorriso, 0);
       strcpy(xorriso->info_text,
              "Error with setting XFS-style project id for ");
       Text_shellsafe(disk_path, xorriso->info_text, 1);
       Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
       {ret= 0; goto ex;}
     }
   }
 }

 ret= 1 + !!lfa_ia_pushed;
ex:;
 iso_node_get_attrs(node, &num_attrs, &names, &value_lengths, &values,1 << 15);
 if(errnos != NULL)
   free(errnos);
 return(ret);
}


/* @param flag
               bit1= minimal transfer: access permissions only
               bit2= keep directory open: keep owner, allow rwx for owner
                     push to xorriso->perm_stack
*/
int Xorriso_restore_implicit_properties(struct XorrisO *xorriso,
          char *full_disk_path, char *disk_path, char *full_img_path, int flag)
{
 int ret, nfic, ndc, nfdc, d, i;
 char *nfi= NULL, *nd= NULL, *nfd= NULL, *cpt;
 struct stat stbuf;
 IsoNode *node;

 Xorriso_alloc_meM(nfi, char, SfileadrL);
 Xorriso_alloc_meM(nd, char, SfileadrL);
 Xorriso_alloc_meM(nfd, char, SfileadrL);

 ret= Xorriso_normalize_img_path(xorriso, xorriso->wdx, full_disk_path, nfd,
                                 1|2|4);
 if(ret<=0)
   goto ex;
 ret= Xorriso_normalize_img_path(xorriso, xorriso->wdx, disk_path, nd, 1|2);
 if(ret<=0)
   goto ex;
 ret= Xorriso_normalize_img_path(xorriso, xorriso->wdi, full_img_path, nfi,
                                 1|2);
 if(ret<=0)
   goto ex;
 nfdc= Sfile_count_components(nfd, 0);
 ndc= Sfile_count_components(nd, 0);
 nfic= Sfile_count_components(nfi, 0);
 d= nfdc-ndc;
 if(d<0)
   {ret= -1; goto ex;}
 if(d>nfic)
   {ret= 0; goto ex;}
 for(i= 0; i<d; i++) {
   cpt= strrchr(nfi, '/');
   if(cpt==NULL)
     {ret= -1; goto ex;} /* should not happen */
   *cpt= 0;
 }
 if(nfi[0]==0)
   strcpy(nfi, "/");
 ret= Xorriso_fake_stbuf(xorriso, nfi, &stbuf, &node, 0);
 if(ret<=0)
   {ret= 0; goto ex;}
 ret= Xorriso_restore_properties(xorriso, nd, node, ((flag>>1)&3));
 if(ret<=0)
   goto ex;
 sprintf(xorriso->info_text, "Restored properties for ");
 Text_shellsafe(nd, xorriso->info_text, 1);
 strcat(xorriso->info_text, " from ");
 Text_shellsafe(nfi, xorriso->info_text, 1 | 2);
 Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "DEBUG", 0);
 ret= 1;
ex:;
 Xorriso_free_meM(nfi);
 Xorriso_free_meM(nd);
 Xorriso_free_meM(nfd);
 return(ret);
}


/* If defined the position accounting will be done by lseek() and used to
 * verify the position accounting in struct Xorriso_sparse_statE.
 * # def ine Xorriso_check_sparsE yes
 */

struct Xorriso_sparse_statE {
 int use_lseek;
 off_t cur_pos;
 off_t after_last_written;
 int warnings;
};


#ifdef Xorriso_check_sparsE

static int Xorriso_sparse_warn(struct XorrisO *xorriso,
                               struct Xorriso_sparse_statE *sparse_state,
                               int occasion, char *msg, int flag)
{
 if(sparse_state->warnings & (1 << occasion))
   return(1);
 sparse_state->warnings|= 1 << occasion;
 Xorriso_msgs_submit(xorriso, 0, msg, 0, "SORRY", 0);
 return(1);
}

#endif /* Xorriso_check_sparsE */


static int Xorriso_sparse_init(struct XorrisO *xorriso,
                               struct Xorriso_sparse_statE **sparse_state,
                               int write_fd, int flag)
{
 struct Xorriso_sparse_statE *o= NULL;
 off_t cur_pos;
 struct stat stbuf;
 int ret;

 *sparse_state= NULL;

 /* Check whether sparse writing is disabled */
 if(xorriso->sparse_min_gap <= 0)
   {ret= 0; goto ex;}

 /* Analyze write_fd */
 ret= fstat(write_fd, &stbuf);
 if(ret == -1)
   {ret= 0; goto ex;}
 if(!S_ISREG(stbuf.st_mode))
   {ret= 0; goto ex;}
 cur_pos= lseek(write_fd, (off_t) 0, SEEK_CUR);
 if(cur_pos < stbuf.st_size)
   {ret= 0; goto ex;}

 Xorriso_alloc_meM(o, struct Xorriso_sparse_statE, 1);

 /* Initialize sparse_state */
 o->use_lseek= 1;
 o->cur_pos= o->after_last_written= cur_pos;
 o->warnings= 0;

 ret= 1;
ex:;
 if(ret >= 1)
   *sparse_state= o;
 else
   Xorriso_free_meM(o);
 return(ret);
}


static int Xorriso_sparse_zeroize(struct XorrisO *xorriso,
                                  struct Xorriso_sparse_statE *sparse_state,
                                  int write_fd, off_t start, off_t count,
                                  int flag)
{
 int ret, buf_size= 32 * 1024, buf_fill, wret;
 off_t todo, seek_ret;
 char *buf= NULL;

 if(count <= 0)
   {ret= 2; goto ex;}
 Xorriso_alloc_meM(buf, char, buf_size);

 seek_ret= lseek(write_fd, start, SEEK_SET);
 if(seek_ret == -1)
   {ret= -1; goto ex;}
 sparse_state->cur_pos= seek_ret;
 for(todo= count; todo > 0; ) {
   if(buf_size < todo)
     buf_fill= buf_size;
   else
     buf_fill= todo;
   wret= write(write_fd, buf, buf_fill);
   if(wret <= 0)
     {ret= wret; goto ex;}
   todo-= wret;
   sparse_state->cur_pos+= wret;
 }
 ret= 1;
ex:;
 Xorriso_free_meM(buf);
 return(ret);
}


/* @param flag bit0= this is the last buffer of the stream
*/
static int Xorriso_sparse_write(struct XorrisO *xorriso,
                                struct Xorriso_sparse_statE *sparse_state,
                                int write_fd, char *buf, int count,
                                int flag)
{
 int wret, i, ret;
 off_t cur_pos= -1, seek_ret;
 static char zero[32]= {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
                        0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};

 if(sparse_state == NULL)
   goto do_write;
 if(!sparse_state->use_lseek)
   goto do_write;
 if(flag & 1)
   goto do_write;

#ifdef Xorriso_check_sparsE

 cur_pos= lseek(write_fd, (off_t) 0, SEEK_CUR);
 if(cur_pos == -1)
   goto do_write;
 if(cur_pos != sparse_state->cur_pos) {
   Xorriso_sparse_warn(xorriso, sparse_state, 0,
                       "cur_pos deviation in Xorriso_sparse_write:intro", 0);
   sparse_state->cur_pos= cur_pos;
 }

#else
  
 cur_pos= sparse_state->cur_pos;

#endif

 /* Check for all zeros */
 if(count % 32)
   goto do_write;
 for(i= 0; i < count; i+= 32)
   if(memcmp(buf + i, zero, 32))
 break;
 if(i < count)
   goto do_write;

 /* Omit write() until next non-zero buffer or end of writing */

#ifdef Xorriso_check_sparsE

 /* Only for debugging: Do real lseek() instead of write() */
 seek_ret= lseek(write_fd, cur_pos + count, SEEK_SET);
 if(seek_ret == -1)
   return(-1);

#endif

 sparse_state->cur_pos= cur_pos + count;
 return(count);

do_write:
 if(sparse_state != NULL) {
   /* Check whether the gap since after_last_written is too small.
      If so: fill the whole gap by writing zeros.
   */
   if(sparse_state->after_last_written < cur_pos) {
     if(xorriso->sparse_min_gap > cur_pos - sparse_state->after_last_written) {
       ret= Xorriso_sparse_zeroize(xorriso, sparse_state, write_fd,
                                sparse_state->after_last_written,
                                cur_pos - sparse_state->after_last_written, 0);
       if(ret < 0)
         return(ret);
       if(ret == 0) {
         seek_ret= lseek(write_fd, cur_pos, SEEK_SET);
         if(seek_ret == -1)
           return(-1);
         sparse_state->cur_pos= seek_ret;
       }
     }
   }
 }


 if(sparse_state != NULL) {

#ifdef Xorriso_check_sparsE

   cur_pos= lseek(write_fd, (off_t) 0, SEEK_CUR);
   if(cur_pos != sparse_state->cur_pos) {
     Xorriso_sparse_warn(xorriso, sparse_state, 1,
                      "cur_pos deviation in Xorriso_sparse_write:do_write", 0);
     sparse_state->cur_pos= cur_pos;
   }

#else

   /* lseek() has been delayed until now */
   if(sparse_state->after_last_written != sparse_state->cur_pos) {
     seek_ret= lseek(write_fd, sparse_state->cur_pos, SEEK_SET);
     if(seek_ret == -1)
       return(-1);
   }

#endif /* ! Xorriso_check_sparsE */

 }
 wret= write(write_fd, buf, count);
 if(sparse_state != NULL && wret > 0 && cur_pos >= 0)
   sparse_state->cur_pos= sparse_state->after_last_written= cur_pos + wret;
 return(wret);
}


static int Xorriso_sparse_finish(struct XorrisO *xorriso,
                                 struct Xorriso_sparse_statE **sparse_state,
                                 int write_fd, int flag)
{
 int ret;
 off_t cur_pos;
 struct Xorriso_sparse_statE *o;

 if(sparse_state == NULL)
   return(0);
 o= *sparse_state;
 if(o == NULL)
   return(1);
 if(write_fd == -1)
   {ret= 1; goto ex;}

#ifdef Xorriso_check_sparsE

 cur_pos= lseek(write_fd, (off_t) 0, SEEK_CUR);
 if(cur_pos == -1)
   {ret= -1; goto ex;}
 if(cur_pos != o->cur_pos) {
   Xorriso_sparse_warn(xorriso, o, 2,
                       "cur_pos deviation in Xorriso_sparse_finish", 0);
   o->cur_pos= cur_pos;
 }

#else
  
 cur_pos= o->cur_pos;

#endif /* ! Xorriso_check_sparsE */

 if(o->after_last_written < cur_pos) {
   /* Check whether the gap since after_last_written is too small.
      If so: fill the whole gap by writing zeros, else: write a last zero byte.
   */
   if(xorriso->sparse_min_gap > cur_pos - o->after_last_written)
     ret= Xorriso_sparse_zeroize(xorriso, o, write_fd, o->after_last_written,
                                 cur_pos - o->after_last_written, 0);
   else
     ret= Xorriso_sparse_zeroize(xorriso, o, write_fd, cur_pos - 1, (off_t) 1,
                                 0);
   if(ret <= 0)
     goto ex;
 }
 ret= 1;
ex:;
 Xorriso_free_meM(o);
 *sparse_state= NULL;
 return(ret);
}


/* @param flag bit0= Minimal transfer: access permissions only
               bit1= *_offset and bytes are valid for writing to regular file
               bit2= This is not a parameter. Do not report if ignored
               bit3= do not restore properties
               bit4= issue pacifier messages with long lasting copying
               bit7= return 4 if restore fails from denied permission
                     do not issue error message
   @return <0 severe error , 0 failure , 1 success ,
           2 regularly not installed (disallowed device, UNIX domain socket)
           4 with bit7: permission to restore was denied
*/
int Xorriso_tree_restore_node(struct XorrisO *xorriso, IsoNode *node,
                              char *img_path, off_t img_offset,
                              char *disk_path, off_t disk_offset, off_t bytes,
                              int flag)
{
 int ret= 0, write_fd= -1, wanted, wret, open_flags, l_errno= 0;
 int target_deleted= 0, buf_size= 32 * 1024, new_empty= 0;
 char *what= "[unknown filetype]";
 char *buf= NULL, type_text[5], *temp_path= NULL, *buf_pt, *reason;
 char *link_target, *open_path_pt= NULL;
 off_t todo= 0, size, seek_ret, last_p_count= 0, already_done, read_count= 0;
 void *data_stream= NULL;
 mode_t mode;
 dev_t dev= 0;
 struct stat stbuf;
 struct utimbuf utime_buffer;
 IsoImage *volume;
 IsoBoot *bootcat;
 uint32_t lba;
 char *catcontent = NULL;
 off_t catsize, iso_node_size, wanted_size, cap;
 char disk_md5[16], iso_md5[16];
 void *ctx= NULL;
 int use_md5= 0, i, sparse_ret= 3;
 struct Xorriso_sparse_statE *sparse_state= NULL;

 Xorriso_alloc_meM(buf, char, buf_size);
 Xorriso_alloc_meM(temp_path, char, SfileadrL);

 if(!(flag & 2))
   img_offset= bytes= 0;
 if(LIBISO_ISDIR(node)) {
   what= "directory";
   ret= mkdir(disk_path, 0777);
   l_errno= errno;
   if(ret == 0) {
     ret= Xorriso_early_chattr_CF(xorriso, node, disk_path, &l_errno, 0);
     if(ret <= 0)
       goto cannot_restore;
   }
 } else if(LIBISO_ISREG(node) || ISO_NODE_IS_BOOTCAT(node)) {
   if(ISO_NODE_IS_BOOTCAT(node)) {
     what= "boot catalog";
   } else {
     what= "regular file";
     ret= Xorriso_iso_file_open(xorriso, img_path, (void *) node, &data_stream,
                                1);
     if(ret<=0)
       goto ex;
     if((xorriso->do_md5 & 65) == 65 && !(flag & 2)) {
       ret= Xorriso_is_plain_image_file(xorriso, (void *) node, img_path, 0);
       if(ret > 0) {
         ret= Xorriso_get_md5(xorriso, (void *) node, img_path, iso_md5, 1);
         if(ret > 0)
           ret= Xorriso_md5_start(xorriso, &ctx, 0);
         if(ret > 0) {
           use_md5= 1;
         } else if(xorriso->do_md5 & 128) {
           sprintf(xorriso->info_text,
                   "Cannot obtain any recorded MD5 of file ");
           Text_shellsafe(img_path, xorriso->info_text, 1);
           Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "SORRY", 0);
           ret= Xorriso_eval_problem_status(xorriso, 0, 1 | 2);
           if(ret < 0)
             {ret= 0; goto ex;}
         }
       }
     }
   }
   open_path_pt= disk_path;
   ret= stat(open_path_pt, &stbuf);
   if(ret == -1) {
     if(errno == EACCES && (flag & 128))
       {ret= 4; goto ex;}
     new_empty= 1;
   }
   if(flag&2) {
     if(ret != -1) {
       wanted_size= disk_offset + bytes;
       iso_node_size= iso_file_get_size((IsoFile *) node);
       if(wanted_size > disk_offset + iso_node_size)
         wanted_size= disk_offset + iso_node_size;
       cap= wanted_size;
       ret= Xorriso_determine_capacity(xorriso, open_path_pt, &cap, &reason,
                                       3);
       if(ret <= 0 || (cap <= 0 && !S_ISREG(stbuf.st_mode))) {
         if(ret > 0)
           reason= "has addressable range 0";
         sprintf(xorriso->info_text,
                 "Restore offset demanded. But target file (%s) ",
                 Ftypetxt(stbuf.st_mode, 0));
         Text_shellsafe(disk_path, xorriso->info_text, 1);
         strcat(xorriso->info_text, reason);
         Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE",0);            l_errno= 0;
         goto cannot_restore;
       }
       if(cap < wanted_size && !S_ISREG(stbuf.st_mode)) {

         /* >>> non-regular file might be too small to take the interval */;

       }
     }
   } else {
     /* If source and target are the same disk file then do not copy content */
     ret= Xorriso_restore_is_identical(xorriso, (void *) node, img_path,
                                       disk_path, type_text, 1);
     if(ret<0)
       goto ex;
     if(ret==1) {
       /* preliminarily emulate touch (might get overridden later) */
       utime_buffer.actime= stbuf.st_atime;
       utime_buffer.modtime= time(0);
       utime(disk_path,&utime_buffer);
       goto restore_properties;
     }
     if(ret==2) {
       /* Extract to temporary file and rename only after copying */
       ret= Xorriso_make_tmp_path(xorriso, disk_path, temp_path, &write_fd,
                                  128);
       if(ret <= 0 || ret == 4)
         goto ex;
       open_path_pt= temp_path;
       new_empty= 1;
     }
   }
   if(write_fd==-1) {
     open_flags= O_WRONLY|O_CREAT;
     if(disk_offset==0 || !(flag&2))
       open_flags|= O_EXCL;
     write_fd= open(open_path_pt, open_flags | O_BINARY, S_IRUSR | S_IWUSR);
     l_errno= errno;
     if(write_fd == -1 && errno == EACCES && (flag & 128))
       {ret= 4; goto ex;}
     if(write_fd==-1)
       goto cannot_restore;
   }

   if(new_empty && open_path_pt != NULL) {
     ret= Xorriso_early_chattr_CF(xorriso, node, open_path_pt, &l_errno, 0);
     if(ret <= 0)
       goto cannot_restore;
   }

   if(ISO_NODE_IS_BOOTCAT(node)) {
     ret= Xorriso_get_volume(xorriso, &volume, 0);
     if(ret<=0)
       goto ex;
     ret= iso_image_get_bootcat(volume, &bootcat, &lba, &catcontent, &catsize);
     if(ret < 0)
       goto ex;
     todo= size= catsize;
   } else {
     todo= size= iso_file_get_size((IsoFile *) node);
   }
   if(flag&2) {
     if(bytes<size)
       todo= size= bytes;
     seek_ret= lseek(write_fd, disk_offset, SEEK_SET);
     l_errno= errno;
     if(seek_ret == -1) {
       sprintf(xorriso->info_text,
               "Cannot address byte %.f in filesystem path ",
               (double) disk_offset);
       Text_shellsafe(open_path_pt, xorriso->info_text, 1);
       Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, "FAILURE",0);
       goto cannot_restore;
     }
   }
   if(ISO_NODE_IS_FILE(node))
     Xorriso_sparse_init(xorriso, &sparse_state, write_fd, 0);

   while(todo>0) {
     wanted= buf_size;
     if(wanted>todo)
       wanted= todo;
     if(ISO_NODE_IS_BOOTCAT(node)) {
       ret= todo;
       buf_pt= catcontent;
     } else {
       ret= Xorriso_iso_file_read(xorriso, data_stream, buf, wanted, 0);
       buf_pt= buf;
     }
     if(ret<=0) {
       if(xorriso->extract_error_mode == 0 &&
          Xorriso_is_plain_image_file(xorriso, node, "", 0)) {
         close(write_fd);
         write_fd= -1;
         already_done= (size - todo) / (off_t) 2048;
         already_done*= (off_t) 2048;
         sprintf(xorriso->info_text,
                 "Starting best_effort handling on ISO file ");
         Text_shellsafe(img_path, xorriso->info_text, 1);
         sprintf(xorriso->info_text + strlen(xorriso->info_text),
                 " at byte %.f", (double) already_done);
         Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "NOTE", 0);
         ret= Xorriso_read_file_data(xorriso, node, img_path, open_path_pt,
                           already_done, already_done, size - already_done, 2);
         if(ret >= 0)
           xorriso->pacifier_byte_count+= todo;
         if(ret > 0)
           todo= 0;
         else
           todo= -1;
       }
       if(ret <= 0) {
         sprintf(xorriso->info_text, "Cannot read all bytes from ISO file ");
         Text_shellsafe(img_path, xorriso->info_text, 1);
         Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
       }
   break;
     }
     read_count+= ret;
     if(use_md5)
       Xorriso_md5_compute(xorriso, ctx, buf_pt, ret, 0);

     if(img_offset > read_count - ret) {
       /* skip the desired amount of bytes */
       if(read_count <= img_offset)
   continue;
       buf_pt= buf_pt + (img_offset - (read_count - ret));
       ret= read_count - img_offset;
     }

     if(sparse_state == NULL)
       wret= write(write_fd, buf_pt, ret);
     else
       wret= Xorriso_sparse_write(xorriso, sparse_state, write_fd, buf_pt, ret,
                                  0);
     if(wret>=0) {
       todo-= wret;
       xorriso->pacifier_byte_count+= wret;
       if((flag&16) &&
          xorriso->pacifier_byte_count - last_p_count >= 128*1024) {
         Xorriso_pacifier_callback(xorriso, "files restored",
                             xorriso->pacifier_count,
                             xorriso->pacifier_total, "", 2 | 4 | 8);
         last_p_count= xorriso->pacifier_byte_count;
       }
     }
     if(wret != ret) {
       sprintf(xorriso->info_text,
               "Cannot write all bytes to disk filesystem path ");
       Text_shellsafe(open_path_pt,  xorriso->info_text, 1);
       Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, "FAILURE",0);
   break;
     }
   }
  
   if(use_md5) {
     ret= Xorriso_md5_end(xorriso, &ctx, disk_md5, 0);
     if(ret <= 0) {
       sprintf(xorriso->info_text,
        "Internal problem with obtaining computed MD5 for extracted data of ");
       goto bad_md5;
     } else {
       for(i= 0; i < 16; i++)
         if(iso_md5[i] != disk_md5[i])
       break;
       if(i < 16) {
         sprintf(xorriso->info_text,
                 "MD5 of extracted data does not match recorded MD5 of file ");
bad_md5:;
         Text_shellsafe(img_path, xorriso->info_text, 1);
         Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "SORRY", 0);
         ret= Xorriso_eval_problem_status(xorriso, 0, 1 | 2);
         if(ret < 0)
           {ret= 0; goto ex;}
       }
     }
   }

   if(write_fd != -1) {
     sparse_ret= Xorriso_sparse_finish(xorriso, &sparse_state, write_fd, 0);
     close(write_fd);
   }
   write_fd= -1;
   if(todo > 0 && xorriso->extract_error_mode == 2) {
     unlink(open_path_pt);
     target_deleted= 1;
   }
   if(! ISO_NODE_IS_BOOTCAT(node))
     Xorriso_iso_file_close(xorriso, &data_stream, 0);
   data_stream= NULL;
   if(temp_path==open_path_pt && !target_deleted) {
     ret= rename(temp_path, disk_path);
     if(ret==-1) {
       sprintf(xorriso->info_text, "Cannot rename temporary path ");
       Text_shellsafe(temp_path, xorriso->info_text, 1);
       strcat(xorriso->info_text, " to final disk path ");
       Text_shellsafe(disk_path, xorriso->info_text, 1 | 2);
       Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, "FAILURE",0);
       unlink(temp_path);
       ret= 0; goto ex;
     }
   }
   ret= -(todo > 0);
   l_errno= 0;
   if(sparse_ret <= 0) {
     strcpy(xorriso->info_text, "Could not finalize sparse extraction of ");
     Text_shellsafe(disk_path, xorriso->info_text, 1 | 2);
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text,
                         sparse_ret < 0 ? errno : 0, "FAILURE", 0);
   }

 } else if(LIBISO_ISLNK(node)) {
   what= "symbolic link";
   link_target= (char *) iso_symlink_get_dest((IsoSymlink *) node);
   ret= symlink(link_target, disk_path);
   l_errno= errno;

 } else if(LIBISO_ISCHR(node)) {
   what= "character device";
   if(xorriso->allow_restore!=2) {
ignored:;
     if(!(flag&4)) {
       sprintf(xorriso->info_text, "Ignored file type: %s ", what);
       Text_shellsafe(img_path, xorriso->info_text, 1);
       strcat(xorriso->info_text, " = ");
       Text_shellsafe(disk_path, xorriso->info_text, 1 | 2);
       Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "NOTE", 0);
     }
     {ret= 2; goto ex;}
   }
   mode= S_IFCHR | 0777;
   ret= Xorriso_node_get_dev(xorriso, node, img_path, &dev, 0);
   if(ret<=0)
     goto ex;
   if(dev == (dev_t) 1) {
probably_damaged:;
     sprintf(xorriso->info_text,
             "Most probably damaged device file not restored: mknod ");
     Text_shellsafe(disk_path, xorriso->info_text, 1);
     sprintf(xorriso->info_text + strlen(xorriso->info_text), 
             " %s 0 1", LIBISO_ISCHR(node) ? "c" : "b");
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, "FAILURE", 0);
     ret= 0; goto ex;
   }
   ret= iso_local_create_dev(disk_path, mode, dev, &l_errno, 0);
   if(ret < 0)
     ret= -1;

 } else if(LIBISO_ISBLK(node)) {
   what= "block device";
   if(xorriso->allow_restore!=2)
     goto ignored;
   mode= S_IFBLK | 0777;
   ret= Xorriso_node_get_dev(xorriso, node, img_path, &dev, 0);
   if(ret<=0)
     goto ex;
   if(dev == (dev_t) 1)
     goto probably_damaged;
   ret= iso_local_create_dev(disk_path, mode, dev, &l_errno, 0);
   if(ret < 0)
     ret= -1;

 } else if(LIBISO_ISFIFO(node)) {
   what= "named pipe";
   mode= 0777;
   ret= mkfifo(disk_path, mode);
   l_errno= errno;

 } else if(LIBISO_ISSOCK(node)) {
   what= "unix socket";
   /* Restoring a socket file is not possible. One rather needs to restart
      the service which temporarily created the socket. */
   goto ignored;

 } else {
   sprintf(xorriso->info_text, "Cannot restore file type '%s'", what);
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "SORRY", 0);
   ret= 0; goto ex;

 }
 if(ret == -1 && l_errno == EACCES && (flag & 128))
   {ret= 4; goto ex;}
 if(ret==-1) {
cannot_restore:;
   sprintf(xorriso->info_text,
           "Cannot restore %s to disk filesystem: ", what);
   Text_shellsafe(img_path, xorriso->info_text, 1);
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, l_errno, "FAILURE", 0);
   ret= 0; goto ex;
 }

restore_properties:;
 if((flag&8) || LIBISO_ISLNK(node))
   ret= 1;
 else
   ret= Xorriso_restore_properties(xorriso, disk_path, node, flag&1);
 if(todo < 0)
   ret= 0;
ex:;
 if(write_fd != -1) {
   close(write_fd);
   if(ret <= 0 && xorriso->extract_error_mode == 2 && open_path_pt != NULL)
     unlink(open_path_pt);
 }
 Xorriso_free_meM(buf);
 Xorriso_free_meM(temp_path);
 if(catcontent != NULL)
   free(catcontent);
 if(data_stream!=NULL)
   Xorriso_iso_file_close(xorriso, &data_stream, 0);
 if(ctx != NULL)
   Xorriso_md5_end(xorriso, &ctx, disk_md5, 0);
 if(sparse_state != NULL)
   Xorriso_sparse_finish(xorriso, &sparse_state, -1, 0);
 Xorriso_process_msg_queues(xorriso,0);
 return(ret);
}


/* Handle overwrite situation in disk filesystem.
   @param node  intended source of overwriting or NULL
   @param flag
               bit4= return 3 on rejection by exclusion or user
               bit6= permission to call Xorriso_make_accessible()
*/
int Xorriso_restore_overwrite(struct XorrisO *xorriso,
                              IsoNode *node, char *img_path,
                              char *path, char *nominal_path,
                              struct stat *stbuf, int flag)
{
 int ret;
 char type_text[5];

 Xorriso_process_msg_queues(xorriso,0);
 if(xorriso->do_overwrite==1 ||
    (xorriso->do_overwrite==2 && !S_ISDIR(stbuf->st_mode))) {

   ret= Xorriso_restore_is_identical(xorriso, (void *) node, img_path,
                                     path, type_text, (node!=NULL));
   if(ret<0)
     return(ret);
   if(ret>0) /* will be handled properly by restore functions */
     ret= Xorriso_reassure_restore(xorriso, path, 8);
   else
     ret= Xorriso_rmx(xorriso, (off_t) 0, path, 8 | (flag & 64));
   if(ret<=0)
     return(ret);
   if(ret==3) {
     sprintf(xorriso->info_text, "User revoked restoring of (ISO) file: ");
     Text_shellsafe(img_path, xorriso->info_text, 1);
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "NOTE", 0);
     return(3*!!(flag&16));
   }
   return(1);
 }
 Xorriso_msgs_submit(xorriso, 0, nominal_path, 0, "ERRFILE", 0);
 sprintf(xorriso->info_text, "While restoring ");
 Text_shellsafe(nominal_path, xorriso->info_text, 1);
 strcat(xorriso->info_text, " : ");
 if(strcmp(nominal_path, path) == 0)
   strcat(xorriso->info_text, "file object");
 else
   Text_shellsafe(path, xorriso->info_text, 1 | 2);
 strcat(xorriso->info_text, " exists and may not be overwritten");
 Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
 return(0);
}


/*
   @return <0 error,
           bit0= hardlink created
           bit1= siblings with target NULL found
           bit2= siblings with non-NULL target found
*/
int Xorriso_restore_target_hl(struct XorrisO *xorriso, IsoNode *node,
                           char *disk_path, int *node_idx, int flag)
{
 int ret, min_hl, max_hl, i, null_target_sibling= 0, link_sibling= 0;

 if(xorriso->hln_targets == NULL)
   return(0);
 ret= Xorriso_search_hardlinks(xorriso, node, node_idx, &min_hl, &max_hl, 1);
 if(ret < 0)
   return(ret);
 if(ret == 0 || *node_idx < 0 || min_hl == max_hl)
   return(0);
 for(i= min_hl; i <= max_hl; i++) {
   if(xorriso->hln_targets[i] == NULL) {
     if(i != *node_idx)
       null_target_sibling= 1;
 continue;
   }
   link_sibling= 1;
   ret= Xorriso_restore_make_hl(xorriso, xorriso->hln_targets[i], disk_path,
                                !!xorriso->do_auto_chmod);
   if(ret > 0)
     return(1);
 }
 return((null_target_sibling << 1) | (link_sibling << 2));
}


/*
   @return <0 error,
           bit0= hardlink created
           bit2= siblings lower index found
*/
int Xorriso_restore_prefix_hl(struct XorrisO *xorriso, IsoNode *node,
                              char *disk_path, int node_idx, int flag)
{
 int ret, min_hl, max_hl, i, link_sibling= 0, hflag;
 char *old_path= NULL, *img_path= NULL;
 struct Xorriso_lsT *img_prefixes= NULL, *disk_prefixes= NULL;

 Xorriso_alloc_meM(old_path, char, SfileadrL);
 Xorriso_alloc_meM(img_path, char, SfileadrL);

 ret= Xorriso_search_hardlinks(xorriso, node, &node_idx, &min_hl, &max_hl,
                               2 | 4);
 if(ret < 0)
   goto ex;
 if(ret == 0 || min_hl == max_hl)
   {ret= 0; goto ex;}

 for(i= min_hl; i < node_idx; i++) {
   link_sibling= 1;
   ret= Xorriso_path_from_node(xorriso, xorriso->node_array[i], img_path, 0);
   if(ret < 0)
     goto ex;
   if(ret == 0)
 continue; /* Node is deleted from tree (Should not happen here) */
   hflag= 1;
   if(i == min_hl) {
     hflag= 0;
   } else if(xorriso->node_array[i] != xorriso->node_array[i - 1]) {
     hflag= 0;
   }
   if(hflag == 0) {
     img_prefixes= xorriso->node_img_prefixes;
     disk_prefixes= xorriso->node_disk_prefixes;
   }
   ret= Xorriso_make_restore_path(xorriso, &img_prefixes, &disk_prefixes,
                                  img_path, old_path, hflag);
   if(ret <= 0)
     goto ex;
   ret= Xorriso_restore_make_hl(xorriso, old_path, disk_path,
                                !!xorriso->do_auto_chmod);
   if(ret > 0)
     {ret= 1; goto ex;}
 }
 ret= link_sibling << 2;
ex:;
 Xorriso_free_meM(old_path);
 Xorriso_free_meM(img_path);
 return(ret);
}


/* @return <0 = error , 0 = availmem exhausted first time , 1 = ok
                        2 = availmem exhausted repeated
*/ 
int Xorriso_register_node_target(struct XorrisO *xorriso, int node_idx,
                                 char *disk_path, int flag)
{
 int l;

 if(xorriso->node_targets_availmem == 0)
   return(2);
 if(xorriso->hln_targets == NULL || node_idx < 0 ||
    node_idx >= xorriso->hln_count)
   return(0);
 if(xorriso->hln_targets[node_idx] != NULL) {
   xorriso->node_targets_availmem+= strlen(xorriso->hln_targets[node_idx]) +1;
   free(xorriso->hln_targets[node_idx]);
 }
 l= strlen(disk_path);
 if(xorriso->node_targets_availmem <= l + 1) {
   sprintf(xorriso->info_text,
 "Hardlink target buffer exceeds -temp_mem_limit. Hardlinks may get divided.");
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
   xorriso->node_targets_availmem= 0;
   return(0);
 }
 xorriso->hln_targets[node_idx]= strdup(disk_path);
 if(xorriso->hln_targets[node_idx] == NULL) {
   Xorriso_no_malloc_memory(xorriso, NULL, 0);
   return(-1);
 }
 xorriso->node_targets_availmem-= (l + 1);
 return(1);
}


/*
   @param flag bit0= offset and bytes is valid for writing to regular file
               bit1= do not report copied files
               bit2= -follow, -not_*: this is not a command parameter
               bit3= keep directory open: keep owner, allow rwx for owner
               bit4= do not look for hardlinks even if enabled
               bit6= this is a copy action: do not fake times and ownership
               bit7= return 4 if restore fails from denied permission
                     do not issue error message
              bit10= do not restore properties
   @return <=0 = error , 1 = added leaf file object , 2 = added directory ,
             3= regularly not installed (disallowed device, UNIX domain socket)
             4 = with bit7: permission to restore was denied
*/
int Xorriso_restore_disk_object(struct XorrisO *xorriso,
                                char *img_path, IsoNode *node,
                                char *disk_path,
                                off_t offset, off_t bytes, int flag)
{
 int ret, i, split_count= 0, partno, total_parts, leaf_is_split= 0;
 int record_hl_path= 0, node_idx, cannot_register= 0, no_props;
 off_t total_bytes;
 char *part_name, *part_path= NULL, *img_path_pt= NULL;
 IsoImage *volume;
 IsoNode *part_node, *first_part_node= NULL;
 struct SplitparT *split_parts= NULL;
 struct stat stbuf;

 Xorriso_alloc_meM(part_path, char, SfileadrL);

 no_props= !!(flag & 1024);

 ret= Xorriso_get_volume(xorriso, &volume, 0);
 if(ret<=0)
   goto ex;

 if(LIBISO_ISDIR(node) && xorriso->do_concat_split)
   leaf_is_split= Xorriso_identify_split(xorriso, img_path, node,
                                    &split_parts, &split_count, &stbuf, 1 | 4);
 if(leaf_is_split) {
   /* map all files in directory img_path into regular file disk_path */

   for(i=0 ; i<split_count; i++) {
     Splitparts_get(split_parts, i, &part_name, &partno, &total_parts,
                    &offset, &bytes, &total_bytes, 0);

     strcpy(part_path, img_path);
     if(Sfile_add_to_path(part_path, part_name, 0)<=0) {
       Xorriso_much_too_long(xorriso, strlen(img_path)+strlen(part_name)+1, 2);
       goto restoring_failed;
     }
     ret= Xorriso_node_from_path(xorriso, volume, part_path, &part_node, 0);
     if(ret<=0)
       goto restoring_failed;
     if(i==0)
       first_part_node= part_node;
     if(offset+bytes>total_bytes)
             bytes= total_bytes-offset;
     ret= Xorriso_tree_restore_node(xorriso, part_node, part_path, (off_t) 0,
               disk_path, offset, bytes,
               (!!(flag&64)) | 2 | (flag & (4 | 128)) | 8 | ( 16 * !(flag&2)));
     if(ret<=0)
       goto restoring_failed;
     if(ret == 4)
       goto ex;
     if(ret == 2)
       {ret= 3; goto ex;}
   }
   if(first_part_node != NULL && !no_props) {
     ret= Xorriso_restore_properties(xorriso, disk_path, first_part_node,
                                     !!(flag&64));
     if(ret <= 0)
       goto restoring_failed;
   }
   goto went_well;
 }

#ifdef Osirrox_not_yeT

 if(resolve_link) {
   ret= Xorriso_resolve_link(xorriso, disk_path, resolved_disk_path, 0);
   if(ret<=0)
     goto ex;
   disk_path_pt= resolved_disk_path;
 } else

#endif /* Osirrox_not_yeT */

   img_path_pt= img_path;

 if(!((xorriso->ino_behavior & 4) || (flag & (1 | 16)) || LIBISO_ISDIR(node))){
   /* Try to restore as hardlink */
   ret= Xorriso_restore_target_hl(xorriso, node, disk_path, &node_idx,
                                  !!xorriso->do_auto_chmod);
   if(ret < 0) {
     goto ex;
   } else if(ret & 1) {
     /* Success, hardlink was created */
     goto went_well;
   } else if(ret & 2) {
     /* Did not establish hardlink. Hardlink siblings with target NULL found.*/
     record_hl_path= 1;
   }
   if(ret & 4) {
     /* Found siblings with non-NULL target, but did not link. */
     ret= Xorriso_eval_problem_status(xorriso, 1, 1 | 2);
     if(ret < 0)
       {ret= 0; goto ex;}
   }
 }

 ret= Xorriso_tree_restore_node(xorriso, node, img_path_pt, (off_t) 0,
                                disk_path, offset, bytes,
     (flag&(4 | 8 | 128)) | (!!(flag&64)) | ((flag&1)<<1) | ( 16 * !(flag&2))
     | (no_props << 3));
 if(ret == 4)
   goto ex;
 if(ret == 2)
   {ret= 3; goto ex;}
 if(ret > 0 && (flag & 8) && !no_props)
   ret= Xorriso_restore_properties(xorriso, disk_path, node, 2 | !!(flag&64));
 if(ret<=0) {
restoring_failed:;
   sprintf(xorriso->info_text, "Restoring failed:  ");
   Text_shellsafe(img_path, xorriso->info_text, 1);
   strcat(xorriso->info_text, " = ");
   Text_shellsafe(disk_path, xorriso->info_text, 1 | 2);
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "SORRY", 0);
   {ret= 0; goto ex;}
 }
 if(record_hl_path) { /* Start of a disk hardlink family */
   ret= Xorriso_register_node_target(xorriso, node_idx, disk_path, 0);
   if(ret < 0)
     goto ex;
   if(ret == 0)
     cannot_register= 1;
 }

went_well:;
 xorriso->pacifier_count++;
 if(!(flag&2))
   Xorriso_pacifier_callback(xorriso, "files restored",
                             xorriso->pacifier_count,
                             xorriso->pacifier_total, "", 4 | 8);
 ret= 1;
ex:;
 if(split_parts!=NULL) 
   Splitparts_destroy(&split_parts, split_count, 0);
 Xorriso_free_meM(part_path);
 if(ret > 0 && cannot_register)
   ret= 0;
 return(ret);
}


/* @param flag bit0= source is a directory and not to be restored as split file
           >>> bit6= permission to call Xorriso_make_accessible()
   @return <=0 error , 1=collision handled , 2=no collision , 3=revoked by user
*/
int Xorriso_handle_collision(struct XorrisO *xorriso,
                             IsoNode *node, char *img_path,
                             char *disk_path, char *nominal_disk_path,
                             int *stbuf_ret, int flag)
{
 int ret, target_is_dir= 0, target_is_link= 0, stat_ret, made_accessible= 0;
 struct stat target_stbuf, lt_stbuf;
 struct PermiteM *perm_stack_mem;

 perm_stack_mem= xorriso->perm_stack;

 /* does a disk file exist with this name ? */
 *stbuf_ret= lstat(disk_path, &target_stbuf);
 if(*stbuf_ret==-1) {
   if((flag & 64) && errno == EACCES) {
     ret= Xorriso_make_accessible(xorriso, disk_path, 0);
     if(ret < 0)
       goto ex;
     made_accessible= 1;
     *stbuf_ret= lstat(disk_path, &target_stbuf);
   }
   if(*stbuf_ret==-1)
     {ret= 2; goto ex;}
 }
 target_is_link= S_ISLNK(target_stbuf.st_mode);
 if(target_is_link) {
   stat_ret= stat(disk_path, &lt_stbuf);
   if(stat_ret == -1) {
     if((flag & 64) && errno == EACCES && !made_accessible) {
       ret= Xorriso_make_accessible(xorriso, disk_path, 0);
       if(ret < 0)
         goto ex;
       made_accessible= 1;
       stat_ret= stat(disk_path, &lt_stbuf);
     }
   }
   if(stat_ret != -1)
     target_is_dir= S_ISDIR(lt_stbuf.st_mode);
 } else {
   target_is_dir= S_ISDIR(target_stbuf.st_mode);
 }
 if(target_is_dir && (!target_is_link) && !(flag&1)) {
   strcpy(xorriso->info_text, "Attempt to replace DISK directory ");
   Text_shellsafe(nominal_disk_path,
                  xorriso->info_text+strlen(xorriso->info_text), 0);
   strcat(xorriso->info_text, " by ISO file ");
   Text_shellsafe(img_path, xorriso->info_text+strlen(xorriso->info_text), 0);
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
   ret= 0; goto ex;
 }

 if(!(target_is_dir && (flag&1))) {
   Xorriso_process_msg_queues(xorriso,0);
   ret= Xorriso_restore_overwrite(xorriso, node, img_path, disk_path,
                           nominal_disk_path, &target_stbuf, 16 | (flag & 64));
   if(ret==3)
     {ret= 3; goto ex;}
   if(ret<=0) 
     goto ex;
   *stbuf_ret= -1; /* It might still exist but will be handled properly */
 }
 ret= 1;
ex:;
 if(made_accessible)
   Permstack_pop(&(xorriso->perm_stack), perm_stack_mem, xorriso, 0);
 return(ret);
}


/* @param flag bit0= recursion is active
               bit1= do not report restored files
               bit6= this is a copy action: do not fake times and ownership
               bit7+8=
                     0= direct operation
                     1= create only directories,
                        count nodes in xorriso->node_counter
                     2= only register non-directory nodes in
                        xorriso->node_array
                     3= count nodes in xorriso->node_counter,
                        create no directory
*/
int Xorriso_restore_tree(struct XorrisO *xorriso, IsoDir *dir,
                         char *img_dir_path, char *disk_dir_path,
                         off_t boss_mem,
                         struct LinkiteM *link_stack, int flag)
{
 IsoImage *volume;
 IsoNode *node;
 IsoDirIter *iter= NULL;
 IsoNode **node_array= NULL;
 int node_count= 0, node_idx;
 int ret, source_is_dir, fret, was_failure= 0, sret;
 int do_not_dive, source_is_split= 0, len_dp, len_ip, stbuf_ret, hflag, hret;
 char *name, *disk_name, *leaf_name, *srcpt, *stbuf_src= "";
 struct LinkiteM *own_link_stack;
 char *sfe= NULL, *sfe2= NULL;
 char *disk_path= NULL, *img_path= NULL, *link_target= NULL;
 off_t mem;
 struct PermiteM *perm_stack_mem;
 struct stat stbuf, disk_stbuf;
 int dir_create= 0, node_register= 0, do_node_count= 0, normal_mode= 0;
 int target_was_no_dir, dir_is_new;
 struct stat *stack_stbufpt;
 int lfa_ia_pushed= 0, stack_chattr;

 perm_stack_mem= xorriso->perm_stack;
 switch((flag >> 7) & 3) {
 case 0: normal_mode= 1;
 break; case 1: dir_create= 1;
 break; case 2: node_register= 1;
 break; case 3: do_node_count= 1;
 }

 /* Avoiding large local memory objects in order to save stack space */
 sfe= malloc(5*SfileadrL);
 sfe2= malloc(5*SfileadrL);
 disk_path= malloc(2*SfileadrL);
 img_path= malloc(2*SfileadrL);
 link_target= malloc(SfileadrL);
 if(sfe==NULL || sfe2==NULL || disk_path==NULL || img_path==NULL ||
    link_target==NULL) {
   Xorriso_no_malloc_memory(xorriso, &sfe, 0);
   {ret= -1; goto ex;}
 }

 own_link_stack= link_stack;

 ret= Xorriso_get_volume(xorriso, &volume, 0);
 if(ret<=0)
   goto ex;

 stbuf_src= img_dir_path;
 node= (IsoNode *) dir;
 ret= Xorriso_fake_stbuf(xorriso, stbuf_src, &stbuf, &node, 1);
 if(ret<=0) {
   Xorriso_msgs_submit(xorriso, 0, disk_dir_path, 0, "ERRFILE", 0);
   sprintf(xorriso->info_text,"Cannot open as (ISO) source directory: %s",
           Text_shellsafe(img_dir_path, sfe, 0));
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
   {ret= 0; goto ex;}
 }

#ifdef Osirrox_not_yeT

 dev_t dir_dev;
 dir_dev= stbuf.st_dev;

 if(S_ISLNK(stbuf.st_mode)) {
   if(!(xorriso->do_follow_links || (xorriso->do_follow_param && !(flag&1))))
     {ret= 2; goto ex;}
   stbuf_src= disk_dir_path;
   if(stat(disk_dir_path, &stbuf)==-1)
     goto cannot_open_dir;
   if(dir_dev != stbuf.st_dev &&
      !(xorriso->do_follow_mount || (xorriso->do_follow_param && !(flag&1))))
     {ret= 2; goto ex;}
 }

#endif /* Osirrox_not_yeT */

 if(!S_ISDIR(stbuf.st_mode)) {
   Xorriso_msgs_submit(xorriso, 0, disk_dir_path, 0, "ERRFILE", 0);
   sprintf(xorriso->info_text,"Is not a directory in ISO image: %s",
           Text_shellsafe(img_dir_path, sfe, 0));
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
   {ret= 0; goto ex;}
 }

 mem= boss_mem;
 ret= Xorriso_findi_iter(xorriso, dir, &mem, &iter, &node_array, &node_count,
                         &node_idx, &node,
                         1 | 4 * (normal_mode && (xorriso->ino_behavior & 4)));
 if(ret<=0)
   goto ex;

 if(Sfile_str(img_path, img_dir_path,0)<=0) {
much_too_long:;
   Xorriso_much_too_long(xorriso, SfileadrL, 2);
   {ret= 0; goto ex;}
 }
 if(img_path[0]==0 || img_path[strlen(img_path)-1]!='/')
   strcat(img_path,"/");
 name= img_path+strlen(img_path);
 if(Sfile_str(disk_path, disk_dir_path, 0)<=0)
   goto much_too_long;
 if(disk_path[0]==0 || disk_path[strlen(disk_path)-1]!='/')
   strcat(disk_path,"/");
 disk_name= disk_path+strlen(disk_path);

 len_dp= strlen(disk_path);
 len_ip= strlen(img_path);

 while(1) { /* loop over ISO directory content */
   stbuf_src= "";
   target_was_no_dir= 0;
   dir_is_new= 0;

#ifdef Osirrox_not_yeT

   Linkitem_reset_stack(&own_link_stack, link_stack, 0);

#endif

   srcpt= img_path;
   Xorriso_process_msg_queues(xorriso,0);
   ret= Xorriso_findi_iter(xorriso, dir, &mem, &iter, &node_array, &node_count,
                           &node_idx, &node, 0);
   if(ret<0)
     goto ex;
   if(ret==0 || xorriso->request_to_abort)
 break;
   leaf_name=  (char *) iso_node_get_name(node);
   if(Xorriso_much_too_long(xorriso, len_dp + strlen(leaf_name)+1, 0)<=0)
     {ret= 0; goto was_problem;}
   if(Xorriso_much_too_long(xorriso, len_ip + strlen(leaf_name)+1, 0)<=0)
     {ret= 0; goto was_problem;}
   /* name and disk_name are pointers into img_path and disk_path */
   strcpy(name, leaf_name);
   strcpy(disk_name,  leaf_name);
   ret= Xorriso_path_is_excluded(xorriso, disk_path, 2);
   if(ret < 0)
     goto was_problem;
   if(ret > 0)
 continue;

   stbuf_src= srcpt;
   ret= Xorriso_fake_stbuf(xorriso, img_path, &stbuf, &node, 1);
   if(ret<=0)
     goto was_problem;
   source_is_dir= 0;

#ifdef Osirrox_not_yeT

   /* ??? Link following in the image would cause severe problems
          with Xorriso_path_from_node() */

   int source_is_link;

   source_is_link= S_ISLNK(stbuf.st_mode);
   if(xorriso->do_follow_links && source_is_link) {
     /* Xorriso_hop_link checks for wide link loops */
     ret= Xorriso_hop_link(xorriso, srcpt, &own_link_stack, &hstbuf, 0);
     if(ret<0)
       goto was_problem;
     if(ret==1) {
       ret= Xorriso_resolve_link(xorriso, srcpt, link_target, 0);
       if(ret<=0)
         goto was_problem;
       srcpt= link_target;
       stbuf_src= srcpt;
       if(lstat(srcpt, &stbuf)==-1)
         goto cannot_lstat;
     } else {
       if(Xorriso_eval_problem_status(xorriso, 0, 1|2)<0)
         {ret= 0; goto was_problem;} 
     }
   } else if (S_ISLNK(stbuf.st_mode)) {
     ret= Xorriso_resolve_link(xorriso, srcpt, link_target, 1);
     if(ret<=0)
       goto was_problem;
   }

#endif /* Osirrox_not_yeT */

   do_not_dive= 0;
   if(S_ISDIR(stbuf.st_mode))
     source_is_dir= 1;
   source_is_split= 0;
   if(source_is_dir)
     source_is_split= Xorriso_is_split(xorriso, img_path, node, 1 | 2 | 4);
   if(source_is_split)
     do_not_dive= 1;

   if(source_is_dir || !(dir_create || do_node_count || node_register)) {
     ret= Xorriso_handle_collision(xorriso, node, img_path,
                                   disk_path, disk_path, &stbuf_ret,
                                   (source_is_dir && !source_is_split));
     if(ret<=0 || ret==3)
       goto was_problem;
   } else {
     stbuf_ret= -1;
   }

   if(stbuf_ret!=-1) { /* (Can only happen with directory) */
     Xorriso_auto_chmod(xorriso, disk_path, 0);
   } else {
     hflag= 4 | (flag & (2|64));
     if(source_is_dir && !do_not_dive)
       hflag|= 8; /* keep directory open for user */
     if((dir_create || do_node_count) && !source_is_dir) {
       xorriso->node_counter++;
     } else if(node_register && !source_is_dir) {
       if(xorriso->node_counter < xorriso->node_array_size) {
         xorriso->node_array[xorriso->node_counter++]= (void *) node;
         iso_node_ref(node);
       }
     } else if(node_register || do_node_count) {
       ret= 1;
     } else {
       if(source_is_dir) {
         ret= lstat(disk_path, &disk_stbuf);
         if(ret == -1) {
           target_was_no_dir= 1;
         } else {
           if(!S_ISDIR(disk_stbuf.st_mode))
             target_was_no_dir= 1;
         }
       }
       ret= Xorriso_restore_disk_object(xorriso, img_path, node, disk_path,
                                        (off_t) 0, (off_t) 0, hflag);
       if(ret == 3) /* intentionally not restored */
         do_not_dive= 1;
       if(target_was_no_dir) {
         sret= lstat(disk_path, &disk_stbuf);
         if(sret != -1)
           if(S_ISDIR(disk_stbuf.st_mode))
             dir_is_new= 1;
       }
     }
     if(ret<=0)
       goto was_problem;
   }
   ret= 1;
   if(source_is_dir && !do_not_dive) {
     ret= Xorriso_restore_tree(xorriso, (IsoDir *) node,
                               img_path, disk_path, mem,
                               own_link_stack, 1 | (flag & (2 | (3 << 7))));
   }
   if(source_is_dir) {
     /* Restore exact access permissions of directory */
     hret= Permstack_peek(&(xorriso->perm_stack), perm_stack_mem, xorriso,
                          disk_path, &stack_stbufpt, &stack_chattr, 0);
     if(hret == 1 && (stack_chattr & (1 | 4)))
       lfa_ia_pushed= 1;
     hret= Permstack_pop(&(xorriso->perm_stack), perm_stack_mem, xorriso,
                         !!(flag&64));
     if(hret<=0 && hret<ret)
       ret= hret;
     if(dir_is_new && !(dir_create || lfa_ia_pushed)) {
       hret= Xorriso_restore_chattr_ia(xorriso, node, disk_path, 0);
       if(hret <= 0 && hret < ret)
         ret= hret;
     }
   }
   if(ret <= 0)
     goto was_problem;

 continue; /* regular bottom of loop */
was_problem:;
   was_failure= 1;
   fret= Xorriso_eval_problem_status(xorriso, ret, 1|2);
   if(fret<0)
     goto ex;
   Permstack_pop(&(xorriso->perm_stack), perm_stack_mem, xorriso, !!(flag&64));
 }

 ret= 1;
ex:
 Permstack_pop(&(xorriso->perm_stack), perm_stack_mem, xorriso, !!(flag&64));
 if(sfe!=NULL)
   free(sfe);
 if(sfe2!=NULL)
   free(sfe2);
 if(disk_path!=NULL)
   free(disk_path);
 if(img_path!=NULL)
   free(img_path);
 if(link_target!=NULL)
   free(link_target);
 Xorriso_findi_iter(xorriso, dir, &mem, &iter, &node_array, &node_count,
                    &node_idx, &node, (1u<<31));

 Xorriso_process_msg_queues(xorriso,0);

#ifdef Osirrox_not_yeT

 Linkitem_reset_stack(&own_link_stack, link_stack, 0);

#endif

 if(ret<=0)
   return(ret);
 return(!was_failure);
}


/*
   @param flag
           >>> bit0= mkdir: graft in as empty directory, not as copy from iso
               bit1= do not report copied files
               bit2= -follow, -not_*: this is not a command parameter
               bit3= use offset and cut_size for -paste_in
               bit4= return 3 on rejection by exclusion or user
               bit5= if directory then do not add sub tree
               bit6= this is a copy action: do not fake times and ownership
               bit7+8= operation mode
                     0= direct operation
                     1= create only directories,
                        count nodes in xorriso->node_counter
                     2= only register non-directory nodes in
                        xorriso->node_array
                     3= count nodes in xorriso->node_counter,
                        create no directory
               bit9= with operation mode 1 do net register prefixes
              bit10= with bit3: do not restore properties to leaf file
   @return <=0 = error , 1 = added leaf file object , 2 = added directory ,
                         3 = rejected 
*/
int Xorriso_restore(struct XorrisO *xorriso,  
                    char *img_path, char *disk_path,
                    off_t offset, off_t bytes, int flag)
{
 IsoImage *volume;
 char *path= NULL, *apt, *npt;
 IsoNode *node= NULL;
 int done= 0, is_dir= 0, ret, source_is_dir, stbuf_ret, hret;
 int dir_create= 0, node_count= 0, node_register= 0, path_size, l_errno= 0;
 int leaf_is_split= 0, source_is_split= 0, new_dir_made= 0, no_props;
 struct stat stbuf;
 struct PermiteM *perm_stack_mem;
 struct stat *stack_stbufpt;
 int lfa_ia_pushed= 0, stack_chattr;

 perm_stack_mem= xorriso->perm_stack;

 path_size= SfileadrL;
 Xorriso_alloc_meM(path, char, path_size);

 switch((flag >> 7) & 3) {
 case 1: dir_create= 1;
 break; case 2: node_register= 1;
 break; case 3: node_count= 1;
 }
 no_props= (!!(flag & 8)) * (flag & 1024);

 if(dir_create && !(flag & (1 << 9))) {
   ret= Xorriso_lst_append_binary(&(xorriso->node_disk_prefixes),
                                  disk_path, strlen(disk_path) + 1, 0);
   if(ret <= 0)
     goto ex; 
   ret= Xorriso_lst_append_binary(&(xorriso->node_img_prefixes),
                                  img_path, strlen(img_path) + 1, 0);
   if(ret <= 0)
     goto ex; 
 }

 ret= Xorriso_path_is_excluded(xorriso, disk_path, 4 | 2 | !(flag & 4));
 if(ret < 0)
   {ret= 0; goto ex;}
 if(ret > 0) {
   if(!(flag & 4)) {
     strcpy(xorriso->info_text,
            "Excluded from restoring by -not_path or -not_leaf :");
     Text_shellsafe(disk_path, xorriso->info_text, 1);
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "SORRY", 0);
   }
   {ret= 3*!!(flag&16); goto ex;}
 }

 ret= Xorriso_get_volume(xorriso, &volume, 0);
 if(ret<=0)
   goto ex;

 strncpy(path, disk_path, path_size - 1);
 path[path_size - 1]= 0;
 apt= npt= path;

 if(!(flag&1)) {
   ret= Xorriso_fake_stbuf(xorriso, img_path, &stbuf, &node, 0);
   if(ret>0) {
     if(S_ISDIR(stbuf.st_mode))
       is_dir= 1;

#ifdef Osirrox_not_yeT

     /* ??? this would cause severe problems with Xorriso_path_from_node() */

     else if((stbuf.st_mode&S_IFMT)==S_IFLNK &&
             (xorriso->do_follow_links ||
              (xorriso->do_follow_param && !(flag&4)))) {
       resolve_link= 1;
       ret= Xorriso_iso_lstat(xorriso, img_path, &stbuf, 1|2);
       if(ret!=-1) {
         if(S_ISDIR(stbuf.st_mode))
           is_dir= 1;
       }
     }
#endif /* Osirrox_not_yeT */

   } else {
     Xorriso_process_msg_queues(xorriso,0);
     Xorriso_msgs_submit(xorriso, 0, disk_path, 0, "ERRFILE", 0);
     sprintf(xorriso->info_text,
            "Cannot determine attributes of (ISO) source file ");
     Text_shellsafe(img_path, xorriso->info_text, 1);
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, "FAILURE", 0);
     ret= 0; goto ex;
   }
   if(is_dir && xorriso->do_concat_split)
     leaf_is_split= Xorriso_is_split(xorriso, img_path, node, 1 | 2 | 4);
 }
 for(npt= apt; !done; apt= npt+1) {
   npt= strchr(apt, '/');
   if(npt==NULL) {
     npt= apt+strlen(apt);
     done= 1;
   } else
     *npt= 0;
   if(*apt==0) {
     *apt= '/';
     apt++;
     if(done)
       goto attach_source;
 continue;
   }
   source_is_dir= (is_dir || (flag&1) || !done);
   source_is_split= done && leaf_is_split;

   stbuf_ret= -1;
   if((flag&8) && done) {

     /* ??? move down from Xorriso_paste_in() :
            check whether target does not exist or both are suitable */;

   } else if(source_is_dir || !(dir_create || node_count || node_register)) {
     ret= Xorriso_handle_collision(xorriso, node, img_path, path, disk_path,
                              &stbuf_ret, (source_is_dir && !source_is_split));
     if(ret<=0 || ret==3)
       goto ex;
   }

   new_dir_made= 0;
   if(stbuf_ret==-1 && (source_is_dir && !source_is_split) &&
      !(node_count || node_register)) {
                                                         /* make a directory */
     ret= mkdir(path, 0777);
     if(ret==-1) {
       Xorriso_process_msg_queues(xorriso,0);
       Xorriso_msgs_submit(xorriso, 0, disk_path, 0, "ERRFILE", 0);
       sprintf(xorriso->info_text,
              "While restoring '%s' : could not insert '%s'", disk_path, path);
       Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, "FAILURE",0);
       {ret= 0; goto ex;}
     }
     if(done) {
       if(node != NULL) {
         ret= Xorriso_early_chattr_CF(xorriso, node, disk_path, &l_errno, 0);
         if(ret <= 0)
           goto ex;
       }
     } else {
       /* keep rwx for the owner, delay lfa_flags 'i' if present */
       ret= Xorriso_restore_implicit_properties(xorriso, disk_path, path,
                                                img_path, 4);
       if(ret <= 0)
         goto ex;
     }
     new_dir_made= 1;
   } else if((source_is_dir && !source_is_split)) {
     if(!(node_count || node_register))
       Xorriso_auto_chmod(xorriso, path, 0);
   }
   if(done) {
attach_source:;

     if(flag&1) {
       /* directory was created above */;

     } else if(is_dir && !source_is_split) {

       if(!node_register) {
         if(new_dir_made) { /* keep open and push to Permstack */
           ret= Xorriso_restore_properties(xorriso, disk_path, node,
                                      2 | !!(flag&64));
           if(ret <= 0) {
             hret= Xorriso_eval_problem_status(xorriso, ret, 1 | 2);
             if(hret < 0)
               goto ex;
           }
         }
       }
       if(!(flag&32)) {
         ret= Xorriso_restore_tree(xorriso, (IsoDir *) node, img_path, path,
                                  (off_t) 0, NULL, flag & (2 | 64 | (3 << 7)));
         if(ret <= 0) {
           hret= Xorriso_eval_problem_status(xorriso, ret, 1 | 2);
           if(hret < 0)
             goto ex;
         }
       }
       if(new_dir_made && !(flag&64)) {
         /* set timestamps which Permstack_pop() will not set */
         ret= Xorriso_restore_timestamps(xorriso, disk_path, node, 0);
         if(ret <= 0) {
           hret= Xorriso_eval_problem_status(xorriso, ret, 1 | 2);
           if(hret < 0)
             goto ex;
         }
       }
     } else {
       if(dir_create || node_count) {
         xorriso->node_counter++;
       } else if(node_register) {
         if(xorriso->node_counter < xorriso->node_array_size) {
           xorriso->node_array[xorriso->node_counter++]= (void *) node;
           iso_node_ref(node);
         }
       } else {
         ret= Xorriso_restore_disk_object(xorriso, img_path, node, path,
                     offset, bytes, (flag & (2|4|64)) | no_props | !!(flag&8));
         if(ret <= 0) {
           hret= Xorriso_eval_problem_status(xorriso, ret, 1 | 2);
           if(hret < 0)
             goto ex;
         }
       }
     }
   } else {
     *npt= '/';
   }
 }

 if(new_dir_made && !dir_create) {
   ret= 1;
   /* Need to set any properties before possibly setting immutable bit.
      So pop earlier than normal.
   */
   hret= Permstack_peek(&(xorriso->perm_stack), perm_stack_mem, xorriso,
                        disk_path, &stack_stbufpt, &stack_chattr, 0);
   if(hret == 1 && (stack_chattr & (1 | 4)))
     lfa_ia_pushed= 1;
   hret= Permstack_pop(&(xorriso->perm_stack), perm_stack_mem, xorriso,
                       2 | !!(flag&64));
   if(hret <= 0 && hret < ret)
     ret= hret;
   if(!lfa_ia_pushed) {
     hret= Xorriso_restore_chattr_ia(xorriso, node, disk_path, 0);
     if(hret <= 0 && hret < ret)
       ret= hret;
     if(ret <= 0)
       goto ex;
   }
 }

 Xorriso_process_msg_queues(xorriso,0);
 ret= 1 + (is_dir && !leaf_is_split); 
ex:;
 /* restore exact access permissions of stacked paths */
 hret= Permstack_pop(&(xorriso->perm_stack), perm_stack_mem, xorriso,
                     2 | !!(flag&64));
 if(hret<=0 && hret<ret)
   ret= hret;
 Xorriso_free_meM(path);
 return(ret);
}


int Xorriso_restore_node_array(struct XorrisO *xorriso, int flag)
{
 int i, ret, fret, hflag, stbuf_ret, faulty_family= 0;
 struct PermiteM *perm_stack_mem;
 char *img_path= NULL, *disk_path= NULL;
 IsoNode *node;
 struct Xorriso_lsT *img_prefixes= NULL, *disk_prefixes= NULL;

 perm_stack_mem= xorriso->perm_stack;

 Xorriso_alloc_meM(img_path, char, SfileadrL);
 Xorriso_alloc_meM(disk_path, char, SfileadrL);

 Xorriso_sort_node_array(xorriso, 0);

 disk_path[0]= 0;
 for(i= 0; i < xorriso->node_counter; i++) {
   node= (IsoNode *) xorriso->node_array[i];
   ret= Xorriso_path_from_node(xorriso, node, img_path, 0);
   if(ret < 0)
     goto ex;
   if(ret == 0)
 continue; /* Node is deleted from tree (Should not happen here) */
   hflag= 1;
   if(i == 0) {
     hflag= 0;
   } else if(node != xorriso->node_array[i - 1]) {
     hflag= 0;
   }
   if(hflag == 0) {
     img_prefixes= xorriso->node_img_prefixes;
     disk_prefixes= xorriso->node_disk_prefixes;
   }
   ret= Xorriso_make_restore_path(xorriso, &img_prefixes, &disk_prefixes,
                                  img_path, disk_path, hflag);
   if(ret<=0)
     goto was_problem;

   /* (Exclusion must already be handled when the node array gets filled,
       because of no distinction between parameter and tree passenger here.)
    */

   ret= Xorriso_handle_collision(xorriso, node, img_path, disk_path, disk_path,
                                 &stbuf_ret, 64);
   if(ret<=0 || ret==3)
     goto was_problem;
   if(xorriso->hln_array != NULL && !(xorriso->ino_behavior & 16)) {
     /* Eventual lookup of hardlinks will be done in
        Xorriso_restore_disk_object() */;
   } else if(i > 0 && !(xorriso->ino_behavior & 4)) {
     if(Xorriso__findi_sorted_ino_cmp(&(xorriso->node_array[i-1]),
                                      &(xorriso->node_array[i])) == 0) {
       if(faulty_family) {
         sprintf(xorriso->info_text, "Hardlinking omitted with ");
         Text_shellsafe(disk_path, xorriso->info_text, 1);
         Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "WARNING", 0);
       } else {
         /* Try to install hardlink to a sibling */
         ret= Xorriso_restore_prefix_hl(xorriso, node, disk_path, i, 0);
         if(ret < 0) {
           goto was_problem;
         } else if(ret & 1) {
           /* Success, hardlink was created */
           xorriso->pacifier_count++;
 continue;
         }
         if(ret & 4) {
           /* Found elder siblings, but did not link. */
           ret= Xorriso_eval_problem_status(xorriso, 1, 1 | 2);
           if(ret < 0)
             {ret= 0; goto ex;}
         }
       }
     } else
       faulty_family= 0;
   }

   ret= Xorriso_restore_disk_object(xorriso, img_path, node, disk_path,
                                    (off_t) 0, (off_t) 0,
                                    4 | (xorriso->ino_behavior & 16) | 128);
   if(ret<=0)
     goto was_problem;
   if(ret == 4) {
     /* Failed from lack of permission */
     ret= Xorriso_make_accessible(xorriso, disk_path, 0);
     if(ret < 0)
       goto ex;
     ret= Xorriso_restore_disk_object(xorriso, img_path, node, disk_path,
                       (off_t) 0, (off_t) 0, 4 | (xorriso->ino_behavior & 16));
     if(ret<=0)
       goto was_problem;
     Permstack_pop(&(xorriso->perm_stack), perm_stack_mem, xorriso, 0);
   }

 continue; /* regular bottom of loop */
was_problem:;
   faulty_family= 1;
   fret= Xorriso_eval_problem_status(xorriso, ret, 1|2);
   if(fret<0)
     goto ex;
   Permstack_pop(&(xorriso->perm_stack), perm_stack_mem, xorriso, 0);
 }
 ret= 1;
ex:;
 Permstack_pop(&(xorriso->perm_stack), perm_stack_mem, xorriso, 0);
 Xorriso_free_meM(img_path);
 Xorriso_free_meM(disk_path);
 return(ret);
}


/* @param flag bit0= -follow, -not: disk_path is not a command parameter
*/
int Xorriso_paste_in(struct XorrisO *xorriso, char *disk_path,
                off_t startbyte, off_t bytecount, char *iso_rr_path, int flag)
{
 int ret, no_props= 0;
 off_t wanted_size, cap;
 char *eff_source= NULL, *eff_dest= NULL, *reason;
 struct stat stbuf, disk_stbuf;
 IsoNode *node;

 Xorriso_alloc_meM(eff_source, char, SfileadrL);
 Xorriso_alloc_meM(eff_dest, char, SfileadrL);

 ret= Xorriso_normalize_img_path(xorriso, xorriso->wdx, disk_path, eff_dest,
                                 2|4);
 if(ret<=0)
   goto ex;
 ret= Xorriso_path_is_excluded(xorriso, disk_path, 4 | 2 | !(flag & 1));
 if(ret > 0 && !(flag & 1)) {
   strcpy(xorriso->info_text,
          "Excluded from restoring by -not_path or -not_leaf :");
   Text_shellsafe(disk_path, xorriso->info_text, 1);
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "SORRY", 0);
 }
 if(ret!=0)
   {ret= 0; goto ex;}

 ret= Xorriso_normalize_img_path(xorriso, xorriso->wdi, iso_rr_path,
                                 eff_source, 2);
 if(ret<=0)
   goto ex;
 ret= Xorriso_fake_stbuf(xorriso, eff_source, &stbuf, &node, 4);
 if(ret<=0)
   {ret= 0; goto ex;}
 if(!S_ISREG(stbuf.st_mode)) {
   Xorriso_msgs_submit(xorriso, 0, eff_dest, 0, "ERRFILE", 0);
   sprintf(xorriso->info_text, "-paste_in: ISO file ");
   Text_shellsafe(eff_source, xorriso->info_text, 1);
   strcat(xorriso->info_text, " is not a data file");
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, "FAILURE", 0);
   {ret= 0; goto ex;}
 }
 
 /* >>> ??? flag bit to obtain startbyte and bytecount from file name */;

 ret= lstat(eff_dest, &disk_stbuf);
 if(ret != -1) {
   no_props= 1 << 10;
   wanted_size= startbyte + bytecount;
   if(wanted_size > stbuf.st_size)
     wanted_size= stbuf.st_size;
   cap= wanted_size;
   ret= Xorriso_determine_capacity(xorriso, eff_dest, &cap, &reason, 3);
   if(ret <= 0 || (cap <= 0 && !S_ISREG(disk_stbuf.st_mode))) {
     Xorriso_msgs_submit(xorriso, 0, eff_dest, 0, "ERRFILE", 0);
     if(ret > 0)
       reason= "has addressable range 0";
     sprintf(xorriso->info_text, "-paste_in: DISK file (%s) ",
             Ftypetxt(disk_stbuf.st_mode, 0));
     Text_shellsafe(eff_dest, xorriso->info_text, 1); 
     sprintf(xorriso->info_text + strlen(xorriso->info_text),
             " exists but %s", reason);
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
     {ret= 0; goto ex;}
   }
   if(cap < wanted_size && !S_ISREG(disk_stbuf.st_mode)) {

     /* >>> non-regular file might be too small to take the interval */;

   }
 }

 ret= Xorriso_restore(xorriso, eff_source, eff_dest, startbyte, bytecount,
                      8 | no_props);
ex:;
 Xorriso_free_meM(eff_source);
 Xorriso_free_meM(eff_dest);
 return(ret);
}


int Xorriso_extract_cut(struct XorrisO *xorriso,
                        char *img_path, char *disk_path,
                        off_t img_offset, off_t bytes, int flag)
{
 int ret, stbuf_ret, read_raw;
 double mem_lut= 0.0;
 char *eff_img_path= NULL, *eff_disk_path= NULL;
 IsoImage *volume;
 IsoNode *node;

 Xorriso_alloc_meM(eff_img_path, char, SfileadrL);
 Xorriso_alloc_meM(eff_disk_path, char, SfileadrL);

 ret= Xorriso_get_volume(xorriso, &volume, 0); 
 if(ret<=0)
   goto ex;
 ret= Xorriso_normalize_img_path(xorriso, xorriso->wdi,
                                 img_path, eff_img_path, 0);
 if(ret<=0)
   goto ex;
 ret= Xorriso_node_from_path(xorriso, volume, eff_img_path, &node, 0);
 if(ret<=0)
   goto ex;
 ret= Xorriso_normalize_img_path(xorriso, xorriso->wdx,
                                 disk_path, eff_disk_path, 2 | 4);
 if(ret<=0)
   goto ex;
 ret= Xorriso_path_is_excluded(xorriso, eff_disk_path, 1 | 2 | 4);
 if(ret < 0)
   goto ex;
 if(ret > 0) {
   strcpy(xorriso->info_text,
          "Excluded from restoring by -not_path or -not_leaf :");
   Text_shellsafe(eff_disk_path, xorriso->info_text, 1);
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "SORRY", 0);
   {ret= 0; goto ex;}
 }

 /* If it is a non-filtered stream from the ISO image
    and img_offset is a multiple of 2048
    then use Xorriso_read_file_data() for random access offset.
 */
 if(!LIBISO_ISREG(node)) {
   Xorriso_msgs_submit(xorriso, 0, eff_disk_path, 0, "ERRFILE", 0);
   sprintf(xorriso->info_text, "-extract_cut: ISO file ");
   Text_shellsafe(eff_img_path, xorriso->info_text, 1);
   strcat(xorriso->info_text, " is not a data file");
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
   {ret= 0; goto ex;}
 }
 ret= Xorriso_handle_collision(xorriso, node, img_path, eff_disk_path,
                               disk_path, &stbuf_ret, 0);
 if(ret<=0 || ret==3)
   {ret= 0; goto ex;}

 Xorriso_pacifier_reset(xorriso, 0);
 mem_lut= xorriso->last_update_time;
 read_raw= 0;
 if((img_offset % 2048) == 0) {
   ret= Xorriso_is_plain_image_file(xorriso, node, "", 0);
   if(ret > 0)
     read_raw= 1;
 }
 if (read_raw) {
   ret= Xorriso_read_file_data(xorriso, node, eff_img_path, eff_disk_path,
                               img_offset, (off_t) 0, bytes, 0);
   if(ret<=0)
     goto ex;
 } else {
   ret= Xorriso_tree_restore_node(xorriso, node, eff_img_path, img_offset,
                                  eff_disk_path, (off_t) 0, bytes, 2 | 8);
   if(ret<=0)
     goto ex;
   if(ret != 1)
     {ret= 0; goto ex;}
 }

 ret= Xorriso_restore_properties(xorriso, eff_disk_path, node, 0);
 if(ret<=0)
   goto ex;

 if(mem_lut != xorriso->last_update_time)
   Xorriso_pacifier_callback(xorriso, "blocks read",
                             xorriso->pacifier_count, 0, "", 1 | 8 | 16 | 32);
 ret= 1;
ex:;
 Xorriso_free_meM(eff_img_path);
 Xorriso_free_meM(eff_disk_path);
 return(ret);
}


/* @param flag bit0= ignore node and img_path, operate on whole medium
               bit1= for Xorriso_check_interval(): no pacifier messages
*/
int Xorriso_read_file_data(struct XorrisO *xorriso, IsoNode *node,
                           char *img_path, char *disk_path,
                           off_t img_offset, off_t disk_offset,
                           off_t bytes, int flag)
{
 int ret, i, lba_count= 0, read_chunk= 16;
 int quality, bad_extract= 0;
 off_t lba, count, blocks, spot, *start_lbas= NULL, *end_lbas= NULL;
 int data_to_skip= 0;
 off_t indev_blocks;
 off_t size= 0, file_base_bytes= 0, file_processed_bytes= 0, img_adr;
 off_t new_file_base_bytes, upto_file_bytes, start_byte= 0;
 off_t *section_sizes = NULL;
 struct SpotlisT *spotlist= NULL;
 struct CheckmediajoB *job= NULL;

 upto_file_bytes= img_offset + bytes;
 data_to_skip= img_offset % (off_t) 2048;

 if(flag & 1) {
   lba_count= 1;
   Xorriso_alloc_meM(start_lbas, off_t, 1);
   Xorriso_alloc_meM(end_lbas, off_t, 1);
   Xorriso_alloc_meM(section_sizes, off_t, 1);
   start_lbas[0]= 0;
   ret= Xorriso_obtain_indev_readsize(xorriso, &indev_blocks, 0);
   if(ret > 0)
     end_lbas[0]= indev_blocks - 1;
   else
     end_lbas[0]= 0x7ffffffffffffffe;
   size= end_lbas[0] * (off_t) 2048;
   section_sizes[0]= size;
 } else {
   ret= Xorriso__start_end_lbas(node, &lba_count, &start_lbas, &end_lbas,
                                &section_sizes,  &size, 0);
   if(ret <= 0) {
     Xorriso_process_msg_queues(xorriso,0);
     sprintf(xorriso->info_text, "File object ");
     Text_shellsafe(img_path, xorriso->info_text, 1);
     strcat(xorriso->info_text,
            " is currently not a data file from the loaded image");
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
     goto ex;
   }
 }
 if(img_offset + bytes < size && bytes > 0)
   size= img_offset + bytes;

 ret= Checkmediajob_new(&job, 0);
 if(ret <= 0)
   goto ex;
 if(xorriso->check_media_default != NULL)
   Checkmediajob_copy(xorriso->check_media_default, job, 0);
 job->min_lba= -1;
 job->max_lba= -1;
 job->sector_map_path[0]= 0;

 ret= Spotlist_new(&spotlist, 0);
 if(ret <= 0)
   {ret= -1; goto ex;}

 if(Sfile_str(job->data_to_path, disk_path, 0) <= 0)
   {ret= -1; goto ex;}
 ret= Xorriso_open_job_data_to(xorriso, job, 0);
 if(ret <= 0)
   goto ex;

 for(i= 0; i < lba_count && file_base_bytes < upto_file_bytes; i++) {
   lba= start_lbas[i];
   count= end_lbas[i] + 1 - start_lbas[i];
   new_file_base_bytes= file_base_bytes + count * (off_t) 2048;

   /* skip intervals before img_offset */
   if(new_file_base_bytes <= img_offset) {
     file_base_bytes= new_file_base_bytes;
 continue;
   }
   /* Eventually adjust first interval start */
   img_adr= lba * (off_t) 2048;
   if(file_base_bytes < img_offset) {
     img_adr+= img_offset - file_base_bytes;
     lba= img_adr / (off_t) 2048;
     count= end_lbas[i] + 1 - lba;
     file_base_bytes= img_offset;
   }

   /* Omit surplus blocks */
   if(new_file_base_bytes > upto_file_bytes)
     count-= (new_file_base_bytes - upto_file_bytes) / (off_t) 2048;
   /* Adjust job */
   job->data_to_offset= file_processed_bytes - img_adr + disk_offset;
   job->data_to_limit= size - file_base_bytes;
   job->data_to_skip= data_to_skip;
   data_to_skip= 0;
   file_processed_bytes+= count * (off_t) 2048;
   ret= Xorriso_check_interval(xorriso, spotlist, job, lba, count, read_chunk,
                               (off_t) 0, (flag & 2));
   if(ret <= 0)
     goto ex;
   if (ret == 2) {
     sprintf(xorriso->info_text, "Attempt aborted to extract data from ");
     Text_shellsafe(img_path, xorriso->info_text, 1);
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
     ret= 0; goto ex;
   }
   file_base_bytes= new_file_base_bytes;
 } 

 /* Use spotlist to evaluate damage */
 file_base_bytes= 0;
 count= Spotlist_count(spotlist, 0);
 for(spot= 0; spot < count; spot++) {
   ret= Spotlist_get_item(spotlist, spot, &lba, &blocks, &quality, 0);
   if(ret <= 0)
 continue;
   if(quality < Xorriso_read_quality_valiD) {
     for(i= 0; i < lba_count; i++) {
       if(start_lbas[i] <= lba && end_lbas[i] >= lba) {
         start_byte= (lba - start_lbas[i]) * (off_t) 2048 + file_base_bytes;
     break;
       }
       file_base_bytes+= ((off_t) (end_lbas[i] + 1 - start_lbas[i]))
                          * (off_t) 2048;
     }
     if(i < lba_count) {
       sprintf(xorriso->info_text, "Bad extract  : %14.f , %14.f , ",
               (double) start_byte, ((double) blocks) * 2048.0);
       Text_shellsafe(disk_path, xorriso->info_text, 1);
       strcat(xorriso->info_text, "\n");
       Xorriso_info(xorriso, 0);
       bad_extract= 1;
     }
   }
 }

 ret= !bad_extract;
ex:;
 if(start_lbas != NULL)
   free((char *) start_lbas);
 if(end_lbas != NULL)
   free((char *) end_lbas);
 if(section_sizes != NULL)
   free((char *) section_sizes);
 Spotlist_destroy(&spotlist, 0);
 Checkmediajob_destroy(&job, 0);
 return(ret);
}


int Xorriso_extract_boot_images(struct XorrisO *xorriso, char *disk_dir_path,
                                int flag)
{
 int ret, img_count= 0, i, was_problem= 0;
 char **imgs= NULL, *eff_path= NULL, *cpt, *eff_namept;
 struct stat stbuf;
 off_t byte_offset, byte_size;

 Xorriso_alloc_meM(eff_path, char, SfileadrL);
 ret= Xorriso_normalize_img_path(xorriso, xorriso->wdx, disk_dir_path,
                                 eff_path, 2 | 4);
 if(ret <= 0)
   goto ex;
 if(strlen(eff_path) > SfileadrL - 80) {
   sprintf(xorriso->info_text,
           "-extract_boot_images: disk_path is too long (%lu)\n",
           (unsigned long int) strlen(eff_path));
   Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
   ret= 0; goto ex;
 }
 ret= stat(eff_path, &stbuf);
 if(ret == 0) {
   if(!S_ISDIR(stbuf.st_mode)) {
     sprintf(xorriso->info_text,
             "-extract_boot_images: disk_path is not a directory : ");
     Text_shellsafe(eff_path, xorriso->info_text, 1);
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
     ret= 0; goto ex;
   }
 } else {
   ret= mkdir(eff_path, 0777);
   if(ret == -1) {
     sprintf(xorriso->info_text,
             "-extract_boot_images: cannot create directory : ");
     Text_shellsafe(eff_path, xorriso->info_text, 1);
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, errno, "FAILURE", 0);
     ret= 0; goto ex;
   }
 }
 strcat(eff_path, "/");
 eff_namept= eff_path + strlen(eff_path);

 ret= Xorriso_list_boot_images(xorriso, &imgs, &img_count, 0);
 if(ret <= 0)
   goto ex;

 /* Interpret list and create files */
 for(i= 0; i < img_count; i++) {
   ret= Xorriso_eval_problem_status(xorriso, 1, 1 | 2);
   if(ret < 0)
     {ret= 0; goto ex;}
   cpt= strchr(imgs[i], '/');
   if(cpt == NULL)
 continue;
   *cpt= 0;
   cpt+= 2;
   ret= Sfile_text_to_off_t(cpt, &byte_offset, 0);
   if(ret <= 0)
 continue;
   cpt+= ret;
   if(*cpt == 0)
 continue;
   cpt++;
   ret= Sfile_text_to_off_t(cpt, &byte_size, 0);
   if(ret <= 0)
 continue;

   strcpy(eff_namept, imgs[i]);
   sprintf(xorriso->info_text, "%s : offset=%.f size=%.f\n",
           eff_path, (double) byte_offset, (double) byte_size);
   Xorriso_info(xorriso, 0);

   ret= stat(eff_path, &stbuf);
   if(ret != -1) {
     sprintf(xorriso->info_text,
             "-extract_boot_images: File already exists on disk: ");
     Text_shellsafe(eff_path, xorriso->info_text, 1);
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "FAILURE", 0);
 continue;
   }
   ret= Xorriso_read_file_data(xorriso, NULL, NULL, eff_path,
                               byte_offset, (off_t) 0, byte_size, 1);
   if(ret <= 0)
     was_problem= 1;
 }
 ret= Xorriso_eval_problem_status(xorriso, 1, 1 | 2);
 if(ret < 0 || was_problem)
   {ret= 0; goto ex;}
 
 ret= 1;
ex:;
 Xorriso_free_meM(eff_path);
 Xorriso_list_boot_images(xorriso, &imgs, &img_count, 1 << 15);
 return(ret);
}


/* @param node      Opaque handle to IsoNode which is to be inquired instead of                     path if it is not NULL.
   @param path      is used as address if node is NULL.
   @param flag      bit0= do not report to result but only indicate outcome
                          by return value
                    bit1= silently ignore nodes without MD5
                    bit2= do not only report mismatches but also matches
   @return          3= not a data file
                    2= no MD5 attached to node
                    1= ok, MD5 compared and matching
                    0= not ok, MD5 mismatch
                   <0= other error
*/
int Xorriso_check_md5(struct XorrisO *xorriso, void *in_node, char *path,
                      int flag)
{
 int ret, wanted, rret, buffer_size= 64 * 1024;
 IsoImage *image;
 IsoNode *node;
 IsoFile *file;
 char node_md5[16], data_md5[16], *buffer= NULL;
 void *stream= NULL, *ctx= NULL;
 off_t todo;

 Xorriso_alloc_meM(buffer, char, 64 * 1024);

 node= (IsoNode *) in_node;
 if(node == NULL) {
   ret= Xorriso_get_node_by_path(xorriso, path, NULL, &node, 0);
   if(ret<=0)
     {ret= -1; goto ex;}
 }
 if(!LIBISO_ISREG(node)) {
   strcpy(xorriso->info_text, "-check_md5: Not a data file: ");
   Text_shellsafe(path, xorriso->info_text, 1);
   if(!(flag & 2))
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "NOTE", 0);
   ret= 3; goto ex;
 }
 file= (IsoFile *) node;

 /* obtain MD5 */
 ret= Xorriso_get_volume(xorriso, &image, 0);
 if(ret <= 0)
   {ret= -1; goto ex;}
 ret= iso_file_get_md5(image, file, node_md5, 0);
 Xorriso_process_msg_queues(xorriso,0);
 if(ret < 0)
   {ret= -1; goto ex;}
 if(ret == 0) {
   strcpy(xorriso->info_text, "-check_md5: No MD5 recorded with file: ");
   Text_shellsafe(path, xorriso->info_text, 1);
   if(!(flag & 2))
     Xorriso_msgs_submit(xorriso, 0, xorriso->info_text, 0, "NOTE", 0);
   ret= 2; goto ex;
 }

 /* Read file and compute MD5 */;
 ret= Xorriso_iso_file_open(xorriso, path, (void *) node, &stream, 1 | 2);
 if(ret <= 0)
   {ret= -1; goto ex;}
 ret= iso_md5_start(&ctx);
 if(ret < 0)
     goto ex;
 todo= iso_stream_get_size(stream);
 while(todo > 0) {
   if(todo < buffer_size)
     wanted= todo;
   else
     wanted= buffer_size;
   rret = Xorriso_iso_file_read(xorriso, stream, buffer, wanted, 0);
   if(rret <= 0)
     {ret= -1; goto ex;}
   todo-= rret;
   ret = iso_md5_compute(ctx, buffer, rret);
   if(ret < 0)
     goto ex;
   xorriso->pacifier_count+= rret; 
   xorriso->pacifier_byte_count+= rret;
   Xorriso_pacifier_callback(xorriso, "content bytes read",
                             xorriso->pacifier_count, 0, "", 8);
   ret= Xorriso_check_for_abort(
             xorriso,
             xorriso->check_media_default != NULL 
                ? xorriso->check_media_default->abort_file_path
                : "/var/opt/xorriso/do_abort_check_media",
             Sfile_microtime(0), &xorriso->last_abort_file_time, 0);
   if(ret == 1)
     {ret= -2; goto ex;}
 }
 ret= iso_md5_end(&ctx, data_md5);
 if(ret < 0)
     goto ex;

 /* Report outcome */
 Xorriso_process_msg_queues(xorriso,0);
 if(! iso_md5_match(node_md5, data_md5)) {
   sprintf(xorriso->result_line, "MD5 MISMATCH: ");
   Text_shellsafe(path, xorriso->result_line, 1);
   strcat(xorriso->result_line, "\n");
   if(!(flag & 1))
     Xorriso_result(xorriso,0);
   ret= 0;
 } else {
   sprintf(xorriso->result_line, "md5 match   : ");
   Text_shellsafe(path, xorriso->result_line, 1);
   strcat(xorriso->result_line, "\n");
   if(flag & 4)
     Xorriso_result(xorriso,0);
   ret= 1;
 }

ex:;
 Xorriso_process_msg_queues(xorriso,0);
 Xorriso_iso_file_close(xorriso, &stream, 0);
 if(ctx != NULL)
   iso_md5_end(&ctx, data_md5);
 Xorriso_free_meM(buffer);
 if(ret < 0) {
   if(ret == -2)
     sprintf(xorriso->result_line, "Aborted at: ");
   else
     sprintf(xorriso->result_line, "NOT READABLE: ");
   Text_shellsafe(path, xorriso->result_line, 1);
   strcat(xorriso->result_line, "\n");
   if(!(flag & 1))
     Xorriso_result(xorriso,0);
   if(ret == -2)
     xorriso->request_to_abort= 1;
 }
 return(ret);
}

