/*
  $Id: drive_exceptions.h,v 1.6 2008/06/13 19:26:23 flameeyes Exp $

  Copyright (C) 2004, 2008 Rocky Bernstein <rocky@gnu.org>
  Copyright (C) 1998 Monty xiphmont@mit.edu
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

extern int scsi_enable_cdda(cdrom_drive_t *d, int);
extern long scsi_read_mmc(cdrom_drive_t *d, void *,long,long);
extern long scsi_read_D4_10(cdrom_drive_t *, void *,long,long);
extern long scsi_read_D4_12(cdrom_drive_t *, void *,long,long);
extern long scsi_read_D8(cdrom_drive_t *, void *,long,long);
extern long scsi_read_28(cdrom_drive_t *, void *,long,long);
extern long scsi_read_A8(cdrom_drive_t *, void *,long,long);

typedef struct exception {
  const char *model;
  int atapi; /* If the ioctl doesn't work */
  unsigned char density;
  int  (*enable)(cdrom_drive_t *,int);
  long (*read)(cdrom_drive_t *,void *, long, long);
  int  bigendianp;
} exception_t;

/* specific to general */

#ifdef FINISHED_DRIVE_EXCEPTIONS
extern long scsi_read_mmc2(cdrom_drive_t *d, void *,long,long);
#else 
#define scsi_read_mmc2 NULL
#endif

int dummy_exception (cdrom_drive_t *d,int Switch);

#if HAVE_LINUX_MAJOR_H
extern const exception_t atapi_list[];
#endif

#ifdef NEED_MMC_LIST
extern const exception_t mmc_list[];
#endif

#ifdef NEED_SCSI_LIST
extern const exception_t scsi_list[];
#endif
