/*
  Copyright (C) 2004, 2008 Rocky Bernstein <rocky@gnu.org>
  Copyright (C) 2014 Robert Kausch <robert.kausch@freac.org>
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
/******************************************************************
 *
 * Fake interface backend for testing paranoia layer
 * NOTE: THIS CODE HAVE BEEN FOLDED INTO THE MAINLINE CODE SO IT IS
 * NOT USED ANYMORE. (It's not clear that it had been used
 * for a while in the non-libcdio cdparaonoia either.)
 *
 ******************************************************************/

#ifdef CDDA_TEST
#include "low_interface.h"
#include "utils.h"

/* Build which test model? */
#define  CDDA_TEST_OK
#undef  CDDA_TEST_JITTER_SMALL
#undef  CDDA_TEST_JITTER_LARGE
#undef  CDDA_TEST_JITTER_MASSIVE
#undef  CDDA_TEST_FRAG_SMALL
#undef  CDDA_TEST_FRAG_LARGE
#undef  CDDA_TEST_FRAG_MASSIVE
#undef  CDDA_TEST_BOGUS_BYTES
#undef  CDDA_TEST_DROPDUPE_BYTES
#undef  CDDA_TEST_SCRATCH
#undef  CDDA_TEST_UNDERRUN

#undef  CDDA_TEST_ALLJITTER
#undef  CDDA_TEST_SOMEJITTER
#undef CDDA_TEST_SEEKJITTER

static int test_readtoc (cdrom_drive *d){
  int tracks=0;
  long bytes;
  long sectors;

  /* only one track, as many sectors as the file */

  bytes=lseek(d->cdda_fd,0,SEEK_END);
  lseek(d->cdda_fd,0,SEEK_SET);
  sectors=bytes/CDIO_CD_FRAMESIZE_RAW;

  d->disc_toc[0].bFlags = 0;
  d->disc_toc[0].bTrack = 1;
  d->disc_toc[0].dwStartSector = 37;

  d->disc_toc[1].bFlags = 0x4;
  d->disc_toc[1].bTrack = CDROM_LEADOUT;
  d->disc_toc[1].dwStartSector = sectors+37;

  tracks=2;
  d->cd_extra=0;
  return(--tracks);  /* without lead-out */
}

/* we emulate jitter, scratches, atomic jitter and bogus bytes on
   boundaries, etc */

static long 
test_read(cdrom_drive *d, void *p, long begin, long sectors)
{

#if defined(CDDA_TEST_SEEKJITTER) \
  || defined(CDDA_TEST_ALLJITTER) \
  || defined(CDDA_TEST_SOMEJITTER)
  int jitter_flag;
#endif

  int los_flag;
  static int jitter=0;
  int bytes_so_far=0;
  long bytestotal;
  static FILE *fd=NULL;
  static long lastread=0;

  if(!fd)fd=fdopen(d->cdda_fd,"r");

  if(begin<lastread)
    d->last_milliseconds=20;
  else
    d->last_milliseconds=sectors;

#ifdef CDDA_TEST_UNDERRUN
  sectors-=1;
#endif

#ifdef CDDA_TEST_SEEKJITTER
  if(lastread!=begin)jitter_flag=1;
#else
#ifdef CDDA_TEST_ALLJITTER
  jitter_flag=1;
#else
#ifdef CDDA_TEST_SOMEJITTER
  jitter_flag=(drand48()>.9?1:0);
  los_flag=(drand48()>.9?1:0);
#else
  los_flag=1;
#endif
#endif
#endif

  lastread=begin+sectors;
  bytestotal=sectors*CDIO_CD_FRAMESIZE_RAW;

  begin*=CDIO_CD_FRAMESIZE_RAW;

  while(bytes_so_far<bytestotal){
    int inner_bytes=bytestotal-bytes_so_far;
    char *inner_buf=(char *)(p ? p + bytes_so_far : NULL);
    long seeki;
    long rbytes;
    long this_bytes=inner_bytes;

#ifdef CDDA_TEST_OK
    
#else
#ifdef CDDA_TEST_JITTER_SMALL
    if(jitter_flag)jitter=4*(int)((drand48()-.5)*CDIO_CD_FRAMESIZE_RAW/8);

#else
#ifdef CDDA_TEST_JITTER_LARGE
    if(jitter_flag)jitter=32*(int)((drand48()-.5)*CDIO_CD_FRAMESIZE_RAW/8);

#else
#ifdef CDDA_TEST_JITTER_MASSIVE
    if(jitter_flag)jitter=128*(int)((drand48()-.5)*CDIO_CD_FRAMESIZE_RAW/8);

#else
#ifdef CDDA_TEST_FRAG_SMALL
    if(los_flag)this_bytes=256*(int)(drand48()*CDIO_CD_FRAMESIZE_RAW/8);
    if(jitter_flag)jitter=4*(int)((drand48()-.5)*CDIO_CD_FRAMESIZE_RAW/8);

#else
#ifdef CDDA_TEST_FRAG_LARGE
    if(los_flag)this_bytes=16*(int)(drand48()*CDIO_CD_FRAMESIZE_RAW/8);
    if(jitter_flag)jitter=4*(int)((drand48()-.5)*CDIO_CD_FRAMESIZE_RAW/8);

#else
#ifdef CDDA_TEST_FRAG_MASSIVE
    if(los_flag)this_bytes=8*(int)(drand48()*CDIO_CD_FRAMESIZE_RAW/8);
    if(jitter_flag)jitter=32*(int)((drand48()-.5)*CDIO_CD_FRAMESIZE_RAW/8);

#else
#ifdef CDDA_TEST_DROPDUPE_BYTES
    if(los_flag)this_bytes=CDIO_CD_FRAMESIZE_RAW;
    if(jitter_flag)
      if (drand48()>.8)
	this_jitter=32;
      else
	this_jitter=0;

#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif


    if(begin+jitter+bytes_so_far<0)jitter=0;    
    seeki=begin+bytes_so_far+jitter;

    if(fseek(fd,seeki,SEEK_SET)<0){
      return(0);
    }
    if(!inner_buf){
      char *temp = malloc(this_bytes);
      rbytes=fread(temp,1,this_bytes,fd);
      free(temp);
    }else
      rbytes=fread(inner_buf,1,this_bytes,fd);


    bytes_so_far+=rbytes;
    if(rbytes==0)break;

#ifdef CDDA_TEST_SEEKJITTER
    jitter_flag=0;
    los_flag=0;
#else
#ifdef CDDA_TEST_ALLJITTER
    jitter_flag=1;
    los_flag=0;
#else
#ifdef CDDA_TEST_SOMEJITTER
    jitter_flag=(drand48()>.9?1:0);
    los_flag=(drand48()>.9?1:0);
#else
    los_flag=1;
#endif
#endif
#endif

  }

#ifdef CDDA_TEST_SCRATCH
  {
    long location=300*CDIO_CD_FRAMESIZE_RAW+(drand48()*56)+512;

    if(begin<=location && begin+bytestotal>location){
      if(p)memset(p+location-begin,(int)(drand48()*256),1100);
    }
  }
#endif

  return(sectors);
}

/* set function pointers to use the ioctl routines */
int test_init_drive (cdrom_drive *d){

  d->nsectors=13;
  d->enable_cdda = dummy_exception;
  d->read_audio = test_read;
  d->read_toc = test_readtoc;
  d->set_speed = dummy_exception;
  d->tracks=d->read_toc(d);
  if(d->tracks==-1)
    return(d->tracks);
  d->opened=1;
  srand48(0);
  return(0);
}

#endif

