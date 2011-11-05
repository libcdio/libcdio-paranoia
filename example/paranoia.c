/*
  Copyright (C) 2005, 2006, 2008, 2009, 2010, 2011
   Rocky Bernstein <rocky@gnu.org>

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

/* Simple program to show using libcdio's version of the CD-DA paranoia. 
   library. */

/* config.h has to come first else _FILE_OFFSET_BITS are redefined in
   say opensolaris. */
#ifdef HAVE_CONFIG_H
# include "config.h"
# define __CDIO_CONFIG_H__ 1
#endif

#include <cdio/paranoia.h>
#include <cdio/cd_types.h>
#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <fcntl.h>

static void 
put_num(long int num, int f, int bytes)
{
  unsigned int i;
  unsigned char c;

  for (i=0; bytes--; i++) {
    c = (num >> (i<<3)) & 0xff;
    if (write(f, &c, 1)==-1) {
      perror("Could not write to output.");
      exit(1);
    }
  }
}

#define writestr(fd, s) \
  write(fd, s, sizeof(s)-1)  /* Subtract 1 for trailing '\0'. */

/* Write a the header for a WAV file. */
static void 
write_WAV_header(int fd, int32_t i_bytecount){
  ssize_t bytes_ret;
  /* quick and dirty */
  bytes_ret = writestr(fd, "RIFF");     /*  0-3 */
  put_num(i_bytecount+44-8, fd, 4);     /*  4-7 */
  bytes_ret = writestr(fd, "WAVEfmt "); /*  8-15 */
  put_num(16, fd, 4);                   /* 16-19 */
  put_num(1, fd, 2);                    /* 20-21 */
  put_num(2, fd, 2);                    /* 22-23 */
  put_num(44100, fd, 4);                /* 24-27 */
  put_num(44100*2*2, fd, 4);            /* 28-31 */
  put_num(4, fd, 2);                    /* 32-33 */
  put_num(16, fd, 2);                   /* 34-35 */
  bytes_ret = writestr(fd, "data");     /* 36-39 */
  put_num(i_bytecount, fd, 4);          /* 40-43 */
}

int
main(int argc, const char *argv[])
{
  cdrom_drive_t *d = NULL; /* Place to store handle given by cd-paranoia. */
  char **ppsz_cd_drives;  /* List of all drives with a loaded CDDA in it. */

  /* See if we can find a device with a loaded CD-DA in it. */
  ppsz_cd_drives = cdio_get_devices_with_cap(NULL, CDIO_FS_AUDIO, false);

  if (ppsz_cd_drives) {
    /* Found such a CD-ROM with a CD-DA loaded. Use the first drive in
       the list. */
    d=cdda_identify(*ppsz_cd_drives, 1, NULL);
  } else {
    printf("Unable find or access a CD-ROM drive with an audio CD in it.\n");
    exit(77);
  }

  /* Don't need a list of CD's with CD-DA's any more. */
  cdio_free_device_list(ppsz_cd_drives);

  if ( !d ) {
    printf("Unable to identify audio CD disc.\n");
    exit(77);
  }

  /* We'll set for verbose paranoia messages. */
  cdda_verbose_set(d, CDDA_MESSAGE_PRINTIT, CDDA_MESSAGE_PRINTIT);

  if ( 0 != cdda_open(d) ) {
    printf("Unable to open disc.\n");
    exit(77);
  }

  /* Okay now set up to read up to the first 300 frames of the first
     audio track of the Audio CD. */
  { 
    cdrom_paranoia_t *p = paranoia_init(d);
    lsn_t i_first_lsn = cdda_disc_firstsector(d);

    if ( -1 == i_first_lsn ) {
      printf("Trouble getting starting LSN\n");
    } else {
      lsn_t   i_cursor;
      ssize_t bytes_ret;
      track_t i_track    = cdda_sector_gettrack(d, i_first_lsn);
      lsn_t   i_last_lsn = cdda_track_lastsector(d, i_track);
      int     fd         = creat("track1s.wav", 0644);
      if (-1 == fd) {
        printf("Unable to create track1s.wav\n");
        exit(1);
      }

      /* For demo purposes we'll read only 300 frames (about 4
	 seconds).  We don't want this to take too long. On the other
	 hand, I suppose it should be something close to a real test.
       */
      if ( i_last_lsn - i_first_lsn > 300) i_last_lsn = i_first_lsn + 299;

      printf("Reading track %d from LSN %ld to LSN %ld\n", i_track, 
	     (long int) i_first_lsn, (long int) i_last_lsn);

      /* Set reading mode for full paranoia, but allow skipping sectors. */
      paranoia_modeset(p, PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP);
      paranoia_seek(p, i_first_lsn, SEEK_SET);
      write_WAV_header(fd, (i_last_lsn-i_first_lsn+1) * CDIO_CD_FRAMESIZE_RAW);

      for ( i_cursor = i_first_lsn; i_cursor <= i_last_lsn; i_cursor ++) {
	/* read a sector */
	int16_t *p_readbuf=paranoia_read(p, NULL);
	char *psz_err=cdda_errors(d);
	char *psz_mes=cdda_messages(d);

	if (psz_mes || psz_err)
	  printf("%s%s\n", psz_mes ? psz_mes: "", psz_err ? psz_err: "");

	free(psz_err);
	free(psz_mes);
	if( !p_readbuf ) {
	  printf("paranoia read error. Stopping.\n");
	  break;
	}
	bytes_ret = write(fd, p_readbuf, CDIO_CD_FRAMESIZE_RAW);
      }
      close(fd);
    }
    paranoia_free(p);
  }

  cdda_close(d);

  exit(0);
}
