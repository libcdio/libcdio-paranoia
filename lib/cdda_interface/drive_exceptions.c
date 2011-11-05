/*
  Copyright (C) 2004, 2008, 2011 Rocky Bernstein <rocky@gnu.org>
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

#include "common_interface.h"
#include "drive_exceptions.h"

int dummy_exception (cdrom_drive_t *d,int Switch)
{
  return(0);
}

#if HAVE_LINUX_MAJOR_H
/* list of drives that affect autosensing in ATAPI specific portions of code 
   (force drives to detect as ATAPI or SCSI, force ATAPI read command */

const exception_t atapi_list[]={
  {"SAMSUNG SCR-830 REV 2.09 2.09 ", 1,   0, dummy_exception,scsi_read_mmc2,0},
  {"Memorex CR-622",                 1,   0, dummy_exception,          NULL,0},
  {"SONY CD-ROM CDU-561",            0,   0, dummy_exception,          NULL,0},
  {"Chinon CD-ROM CDS-525",          0,   0, dummy_exception,          NULL,0},
  {NULL,0,0,NULL,NULL,0}};
#endif /*HAVE_LINUX_MAJOR_H*/

/* list of drives that affect MMC default settings */

#ifdef NEED_MMC_LIST
static exception_t mmc_list[]={
  {"SAMSUNG SCR-830 REV 2.09 2.09 ", 1,   0, dummy_exception,scsi_read_mmc2,0},
  {"Memorex CR-622",                 1,   0, dummy_exception,          NULL,0},
  {"SONY CD-ROM CDU-561",            0,   0, dummy_exception,          NULL,0},
  {"Chinon CD-ROM CDS-525",          0,   0, dummy_exception,          NULL,0},
  {"KENWOOD CD-ROM UCR",            -1,   0,            NULL,scsi_read_D8,  0},
  {NULL,0,0,NULL,NULL,0}};
#endif /*NEED_MMC_LIST*/

/* list of drives that affect SCSI default settings */

#ifdef NEED_SCSI_LIST
static exception_t scsi_list[]={
  {"TOSHIBA",                     -1,0x82,scsi_enable_cdda,scsi_read_28,  0},
  {"IBM",                         -1,0x82,scsi_enable_cdda,scsi_read_28,  0},
  {"DEC",                         -1,0x82,scsi_enable_cdda,scsi_read_28,  0},
  
  {"IMS",                         -1,   0,scsi_enable_cdda,scsi_read_28,  1},
  {"KODAK",                       -1,   0,scsi_enable_cdda,scsi_read_28,  1},
  {"RICOH",                       -1,   0,scsi_enable_cdda,scsi_read_28,  1},
  {"HP",                          -1,   0,scsi_enable_cdda,scsi_read_28,  1},
  {"PHILIPS",                     -1,   0,scsi_enable_cdda,scsi_read_28,  1},
  {"PLASMON",                     -1,   0,scsi_enable_cdda,scsi_read_28,  1},
  {"GRUNDIG CDR100IPW",           -1,   0,scsi_enable_cdda,scsi_read_28,  1},
  {"MITSUMI CD-R ",               -1,   0,scsi_enable_cdda,scsi_read_28,  1},
  {"KENWOOD CD-ROM UCR",          -1,   0,            NULL,scsi_read_D8,  0},

  {"YAMAHA",                      -1,   0,scsi_enable_cdda,        NULL,  0},

  {"PLEXTOR",                     -1,   0,            NULL,        NULL,  0},
  {"SONY",                        -1,   0,            NULL,        NULL,  0},

  {"NEC",                         -1,   0,           NULL,scsi_read_D4_10,0},

  /* the 7501 locks up if hit with the 10 byte version from the
     autoprobe first */
  {"MATSHITA CD-R   CW-7501",     -1,   0,           NULL,scsi_read_D4_12,-1},

  {NULL,0,0,NULL,NULL,0}};

#endif /* NEED_SCSI_LIST*/
