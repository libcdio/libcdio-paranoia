/*
  $Id: paranoia.cpp,v 1.3 2008/03/24 15:30:56 karl Exp $

  Copyright (C) 2005, 2008, 2009 Rocky Bernstein <rocky@gnu.org>
  
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
   library.  */

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <iomanip>
using namespace std;

extern "C"{
  #include <cdio/paranoia.h>
  #include <cdio/cd_types.h>
  #include <stdio.h>

#ifdef HAVE_STDLIB_H
  #include <stdlib.h>
#endif

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
    d = cdda_identify(*ppsz_cd_drives, 1, NULL);
  } else {
    cerr << "Unable to access to a CD-ROM drive with audio CD in it";
    return -1;
  }

  /* Don't need a list of CD's with CD-DA's any more. */
  cdio_free_device_list(ppsz_cd_drives);

  if ( !d ) {
    cerr << "Unable to identify audio CD disc.\n";
    return -1;
  }

  /* We'll set for verbose paranoia messages. */
  cdda_verbose_set(d, CDDA_MESSAGE_PRINTIT, CDDA_MESSAGE_PRINTIT);

  if ( 0 != cdda_open(d) ) {
    cerr << "Unable to open disc.\n";
    return -1;
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
      track_t i_track    = cdda_sector_gettrack(d, i_first_lsn);
      lsn_t   i_last_lsn = cdda_track_lastsector(d, i_track);

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
      //Get the track size in bytes and conver it to string
      unsigned int byte_count = 
	( i_last_lsn - i_first_lsn + 1 ) * CDIO_CD_FRAMESIZE_RAW;

      // Open the output file
      ofstream outfile ("track01.wav",
			ofstream::binary | ofstream::app | ofstream::out);
       
      // Write format header specification
      const int waweChunkLength   = byte_count + 44 - 8;
      const int fmtChunkLength    = 16;
      const int compressionCode   = 1;
      const int numberOfChannels  = 2;
      const int sampleRate        = 44100;  // Hz
      const int blockAlign        = sampleRate*2*2;
      const int significantBps    = 4;
      const int extraFormatBytes  = 16;

#define writestr(str) outfile.write(str, sizeof(str)-1)	
      writestr("RIFF");
      outfile.write((char*)&waweChunkLength, 4);
      writestr("WAVEfmt ");
      outfile.write((char*) &fmtChunkLength, 4);
      outfile.write((char*) &compressionCode, 2);
      outfile.write((char*) &numberOfChannels, 2);
      outfile.write((char*) &sampleRate, 4);
      outfile.write((char*) &blockAlign, 4);
      outfile.write((char*) &significantBps, 2);
      outfile.write((char*) &extraFormatBytes, 2);
      writestr("data");
      outfile.write((char*) &byte_count,4);

      for ( i_cursor = i_first_lsn; i_cursor <= i_last_lsn; i_cursor ++) {
	/* read a sector */
	int16_t *p_readbuf=paranoia_read(p, NULL);
	char *psz_err=cdda_errors(d);
	char *psz_mes=cdda_messages(d);

	if (psz_mes || psz_err)
	  cerr << psz_err << psz_mes;

	if (psz_err) free(psz_err);
	if (psz_mes) free(psz_mes);
	if( !p_readbuf ) {
	  cerr << "paranoia read error. Stopping.\n";
	  break;
	}

	char *temp= (char*) p_readbuf;
	outfile.write(temp, CDIO_CD_FRAMESIZE_RAW);

      }
    }
    paranoia_free(p);
  }

  cdda_close(d);

  exit(0);
}
