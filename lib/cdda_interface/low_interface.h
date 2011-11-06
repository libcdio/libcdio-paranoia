/*
  Copyright (C) 2004, 2005, 2008 Rocky Bernstein <rocky@gnu.org>
  Copyright (C) 1998 Monty xiphmont@mit.edu
*/
/** internal include file for cdda interface kit for Linux */

#ifndef _CDDA_LOW_INTERFACE_
#define _CDDA_LOW_INTERFACE_

#ifdef HAVE_STDIO_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_LINUX_VERSION_H
#include <linux/version.h>
#endif

#include <cdio/paranoia/paranoia.h>
#include <cdio/paranoia/cdda.h>

/* some include file locations have changed with newer kernels */

#ifndef CDROMAUDIOBUFSIZ      
#define CDROMAUDIOBUFSIZ        0x5382 /* set the audio buffer size */
#endif

#ifdef HAVE_LINUX_CDROM_H
#include <linux/cdrom.h>
#endif

#ifdef HAVE_LINUX_MAJOR_H
#include <linux/major.h>
#endif

#define MAX_RETRIES 8
#define MAX_BIG_BUFF_SIZE 65536
#define MIN_BIG_BUFF_SIZE 4096
#define SG_OFF sizeof(struct sg_header)

extern int  cddap_init_drive (cdrom_drive_t *d);
#endif /*_CDDA_LOW_INTERFACE_*/

