/*
  Copyright (C) 2004, 2005, 2008, 2009, 2010, 2011
  Rocky Bernstein <rocky@gnu.org>
  Copyright (C) 1998 Monty xiphmont@mit.edu
*/

#ifndef _CDDA_COMMON_INTERFACE_H_
#define _CDDA_COMMON_INTERFACE_H_

#if defined(HAVE_CONFIG_H) && !defined(__CDIO_CONFIG_H__)
# include "config.h"
# define __CDIO_CONFIG_H__ 1
#endif

#include <cdio/types.h>
#include "low_interface.h"

#if defined(HAVE_LSTAT) && !defined(HAVE_WIN32_CDROM) && !defined(HAVE_OS2_CDROM)
/* Define this if the CD-ROM device is a file in the filesystem
   that can be lstat'd
*/
#define DEVICE_IN_FILESYSTEM 1
#else 
#undef DEVICE_IN_FILESYSTEM
#endif

/** Test for presence of a cdrom by pinging with the 'CDROMVOLREAD' ioctl() */
extern int ioctl_ping_cdrom(int fd);

extern char *atapi_drive_info(int fd);

/*! Here we fix up a couple of things that will never happen.  yeah,
   right.  

   rocky OMITTED FOR NOW:
   The multisession stuff is from Hannu's code; it assumes it knows
   the leadout/leadin size.
*/
extern int FixupTOC(cdrom_drive_t *d, track_t tracks);

#endif /*_CDDA_COMMON_INTERFACE_H_*/
