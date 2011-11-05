/*
  $Id: scsi_interface.c,v 1.5 2008/06/25 08:01:53 rocky Exp $

  Copyright (C) 2004, 2008 Rocky Bernstein <rocky@gnu.org>
  Original interface.c Copyright (C) 1994-1997 
             Eissfeldt heiko@colossus.escape.de
  Current blenderization Copyright (C) 1998-1999 Monty xiphmont@mit.edu
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
 * Generic SCSI interface specific code.
 *
 ******************************************************************/

#define NEED_STRERROR_TR
#define NEED_MMC_LIST
#define NEED_SCSI_LIST

#include "low_interface.h"
#include "common_interface.h"
#include "utils.h"
#include <cdio/scsi_mmc.h>
#include <cdio/bytesex.h>

#include "drive_exceptions.h"

static void 
tweak_SG_buffer(cdrom_drive_t *d){
  int table,reserved;
  char buffer[256];

  /* maximum transfer size? */
  if(ioctl(d->cdda_fd,SG_GET_RESERVED_SIZE,&reserved)){
    /* Up, guess not. */
    cdmessage(d,"\tCould not get scatter/gather buffer size.\n");
    return;
  }

  if(ioctl(d->cdda_fd,SG_GET_SG_TABLESIZE,&table))table=1;
  {
    int cur;

    sprintf(buffer,"\tDMA scatter/gather table entries: %d\n\t"
	    "table entry size: %d bytes\n\t"
	    "maximum theoretical transfer: %d sectors\n",
	    table,reserved,table*reserved/CDIO_CD_FRAMESIZE_RAW);
    cdmessage(d,buffer);

    cur=table*reserved;

    /* not too much; new kernels have trouble with DMA allocation, so
       be more conservative: 32kB max until I test more thoroughly */
    cur=(cur>1024*32?1024*32:cur);
    d->nsectors=cur/CDIO_CD_FRAMESIZE_RAW;
    d->bigbuff=cur;

    sprintf(buffer,"\tSetting default read size to %d sectors (%d bytes).\n\n",
	    d->nsectors,d->nsectors*CDIO_CD_FRAMESIZE_RAW);
    cdmessage(d,buffer);
  } 

  /* Disable command queue; we don't need it, no reason to have it on */
  reserved=0;
  if(ioctl(d->cdda_fd,SG_SET_COMMAND_Q,&reserved)){
    cdmessage(d,"\tCouldn't disable command queue!  Continuing anyway...\n");
  }

}

static void 
reset_scsi(cdrom_drive_t *d)
{
  int arg;
  d->enable_cdda(d,0);

  cdmessage(d,"sending SG SCSI reset... ");
  if(ioctl(d->cdda_fd,SG_SCSI_RESET,&arg))
    cdmessage(d,"FAILED: EBUSY\n");
  else
    cdmessage(d,"OK\n");
  
  d->enable_cdda(d,1);
}

static void 
clear_garbage(cdrom_drive_t *d)
{
  fd_set fdset;
  struct timeval tv;
  struct sg_header *sg_hd=(struct sg_header *)d->sg;
  int flag=0;

  /* clear out any possibly preexisting garbage */
  FD_ZERO(&fdset);
  FD_SET(d->cdda_fd,&fdset);
  tv.tv_sec=0;
  tv.tv_usec=0;

  /* I like select */
  while(select(d->cdda_fd+1,&fdset,NULL,NULL,&tv)==1){
    
    sg_hd->twelve_byte = 0;
    sg_hd->result = 0;
    sg_hd->reply_len = SG_OFF;
    read(d->cdda_fd, sg_hd, 1);

    /* reset for select */
    FD_ZERO(&fdset);
    FD_SET(d->cdda_fd,&fdset);
    tv.tv_sec=0;
    tv.tv_usec=0;
    if(!flag && d->report_all)
      cdmessage(d,"Clearing previously returned data from SCSI buffer\n");
    flag=1;
  }
}

/* process a complete scsi command. */
static int 
handle_scsi_cmd(cdrom_drive_t *d, 
		unsigned int cmd_len,  unsigned int in_size, 
		unsigned int out_size, unsigned char bytefill,
		int bytecheck)
{
  int status = 0;
  struct sg_header *sg_hd=(struct sg_header *)d->sg;
  long writebytes=SG_OFF+cmd_len+in_size;

  /* generic scsi device services */

  /* clear out any possibly preexisting garbage */
  clear_garbage(d);

  memset(sg_hd,0,sizeof(sg_hd)); 
  sg_hd->twelve_byte = cmd_len == 12;
  sg_hd->result = 0;
  sg_hd->reply_len = SG_OFF + out_size;

  /* The following is one of the scariest hacks I've ever had to use.
     The idea is this: We want to know if a command fails.  The
     generic scsi driver (as of now) won't tell us; it hands back the
     uninitialized contents of the preallocated kernel buffer.  We
     force this buffer to a known value via another bug (nonzero data
     length for a command that doesn't take data) such that we can
     tell if the command failed.  Scared yet? */

  if(bytecheck && out_size>in_size){
    memset(d->sg_buffer+cmd_len+in_size,bytefill,out_size-in_size); 
    /* the size does not remove cmd_len due to the way the kernel
       driver copies buffers */
    writebytes+=(out_size-in_size);
  }

  {
    /* Select on write with a 5 second timeout.  This is a hack until
       a better error reporting layer is in place in alpha 10; right
       now, always print a message. */

    fd_set fdset;
    struct timeval tv;

    FD_ZERO(&fdset);
    FD_SET(d->cdda_fd,&fdset);
    tv.tv_sec=60; /* Increased to 1m for plextor, as the drive will
                     try to get through rough spots on its own and
                     this can take time 19991129 */
    tv.tv_usec=0;

    while(1){
      int ret=select(d->cdda_fd+1,NULL,&fdset,NULL,&tv);
      if(ret>0)break;
      if(ret<0 && errno!=EINTR)break;
      if(ret==0){
	fprintf(stderr,"\nSCSI transport error: timeout waiting to write"
		" packet\n\n");
	return(TR_EWRITE);
      }
    }
  }

  sigprocmask (SIG_BLOCK, &(d->sigset), NULL );
  errno=0;
  status = write(d->cdda_fd, sg_hd, writebytes );

  if (status<0 || status != writebytes ) {
    sigprocmask ( SIG_UNBLOCK, &(d->sigset), NULL );
    if(errno==0)errno=EIO;
    return(TR_EWRITE);
  }
  
  {
    /* Select on read (and write; this signals an error) with a 5
       second timeout.  This is a hack until a better error reporting
       layer is in place in alpha 10; right now, always print a
       message. */

    fd_set rset;
    struct timeval tv;

    FD_ZERO(&rset);
    FD_SET(d->cdda_fd,&rset);
    tv.tv_sec=60; /* Increased to 1m for plextor, as the drive will
                     try to get through rough spots on its own and
                     this can take time 19991129 */
    tv.tv_usec=0;

    while(1){
      int ret=select(d->cdda_fd+1,&rset,NULL,NULL,&tv);
      if(ret<0 && errno!=EINTR)break;
      if(ret==0){
	sigprocmask ( SIG_UNBLOCK, &(d->sigset), NULL );
	fprintf(stderr,"\nSCSI transport error: timeout waiting to read"
		" packet\n\n");
	return(TR_EREAD);
      }
      if(ret>0){
	/* is it readable or something else? */
	if(FD_ISSET(d->cdda_fd,&rset))break;
	sigprocmask ( SIG_UNBLOCK, &(d->sigset), NULL );
	fprintf(stderr,"\nSCSI transport: error reading packet\n\n");
	return(TR_EREAD);
      }
    }
  }

  errno=0;
  status = read(d->cdda_fd, sg_hd, SG_OFF + out_size);
  sigprocmask ( SIG_UNBLOCK, &(d->sigset), NULL );

  if (status<0)return(TR_EREAD);

  if(status != SG_OFF + out_size || sg_hd->result){
    if(errno==0)errno=EIO;
    return(TR_EREAD);
  }

  if(sg_hd->sense_buffer[0]){
    char key=sg_hd->sense_buffer[2]&0xf;
    char ASC=sg_hd->sense_buffer[12];
    char ASCQ=sg_hd->sense_buffer[13];
    switch(key){
    case 0:
      if(errno==0)errno=EIO;
      return(TR_UNKNOWN);
    case 1:
      break;
    case 2:
      if(errno==0)errno=EBUSY;
      return(TR_BUSY);
    case 3: 
      if(ASC==0x0C && ASCQ==0x09){
	/* loss of streaming */
	if(errno==0)errno=EIO;
	return(TR_STREAMING);
      }else{
	if(errno==0)errno=EIO;
	return(TR_MEDIUM);
      }
    case 4:
      if(errno==0)errno=EIO;
      return(TR_FAULT);
    case 5:
      if(errno==0)errno=EINVAL;
      return(TR_ILLEGAL);
    default:
      if(errno==0)errno=EIO;
      return(TR_UNKNOWN);
    }
  }

  /* still not foolproof; the following doesn't guarantee that we got
     all the data, just that the command was not rejected. */

  /* Why do this with the above sense stuff?  For some reason,
     commands still get through.  Perhaps no data comes back even
     though the target reports success? */

  if(bytecheck && in_size+cmd_len<out_size){
    long i,flag=0;
    for(i=in_size;i<out_size;i++)
      if(d->sg_buffer[i]!=bytefill){
	flag=1;
	break;
      }
    
    if(!flag){
      errno=EINVAL;
      return(TR_ILLEGAL);
    }
  }

  errno=0;
  return(0);
}

/* Group 1 (10b) command */

static int 
mode_sense_atapi(cdrom_drive_t *d, int size, int page)
{
  memcpy(d->sg_buffer,  
	 (char [])  {CDIO_MMC_GPCMD_MODE_SENSE_10,  
		       0x00, /* reserved */
		       0x00, /* page */
		       0,    /* reserved */
		       0,    /* reserved */
		       0,    /* reserved */
		       0,    /* reserved */
		       0,    /* MSB (0) */
		       0,    /* sizeof(modesense - SG_OFF) */
		       0},   /* reserved */ 
         10);

  d->sg_buffer[1]=d->lun<<5;
  d->sg_buffer[2]=0x3F&page;
  d->sg_buffer[8]=size+4;

  if (handle_scsi_cmd (d, 10, 0, size+4,'\377',1)) return(1);

  {
    char *b=d->sg_buffer;
    if(b[0])return(1); /* Handles only up to 256 bytes */
    if(b[6])return(1); /* Handles only up to 256 bytes */

    b[0]=b[1]-3;
    b[1]=b[2];
    b[2]=b[3];
    b[3]=b[7];

    memmove(b+4,b+8,size);
  }
  return(0);
}

/* group 0 (6b) command */

static int 
mode_sense_scsi(cdrom_drive_t *d, int size, int page)
{
  memcpy(d->sg_buffer,  
	 (char [])  {CDIO_MMC_GPCMD_MODE_SENSE, 
		       0x00, /* return block descriptor/lun */
		       0x00, /* page */
		       0,    /* reserved */
		       0,   /* sizeof(modesense - SG_OFF) */
		       0},   /* control */ 
         6);
  
  d->sg_buffer[1]=d->lun<<5;
  d->sg_buffer[2]=(0x3F&page);
  d->sg_buffer[4]=size;

  if (handle_scsi_cmd (d, 6, 0, size, '\377',1)) return(1);
  return(0);
}

static int mode_sense(cdrom_drive_t *d,int size,int page){
  if(d->is_atapi)
    return(mode_sense_atapi(d,size,page));
  return(mode_sense_scsi(d,size,page));
}

static int mode_select(cdrom_drive_t *d,int density,int secsize){
  /* short circut the way Heiko does it; less flexible, but shorter */
  if(d->is_atapi){
    unsigned char *mode = d->sg_buffer + 18;

    memcpy(d->sg_buffer,
	   (char []) { CDIO_MMC_GPCMD_MODE_SELECT,
			 0x10, /* no save page */
			 0, /* reserved */
			 0, /* reserved */
			 0, /* reserved */
			 0, /* reserved */
			 0, /* reserved */
			 0, /* reserved */
			 12, /* sizeof(mode) */
			 0, /* reserved */

			 /* mode parameter header */
			 0, 0, 0, 0,  0, 0, 0, 
			 8, /* Block Descriptor Length */

			 /* descriptor block */
			 0,       /* Density Code */
			 0, 0, 0, /* # of Blocks */
			 0,       /* reserved */
			 0, 0, 0},/* Blocklen */
	   26);

    d->sg_buffer[1]|=d->lun<<5;

    /* prepare to read cds in the previous mode */
    mode [0] = density;
    mode [6] =  secsize >> 8;   /* block length "msb" */
    mode [7] =  secsize & 0xFF; /* block length lsb */

    /* do the scsi cmd */
    return(handle_scsi_cmd (d,10, 16, 0,0,0));

  }else{
    unsigned char *mode = d->sg_buffer + 10;

    memcpy(d->sg_buffer,
	   (char []) { CDIO_MMC_GPCMD_MODE_SELECT, 
			 0x10, /* no save page */
			 0, /* reserved */
			 0, /* reserved */
			 12, /* sizeof(mode) */
			 0, /* reserved */
			 /* mode section */
			 0, 
			 0, 0, 
			 8,       /* Block Descriptor Length */
			 0,       /* Density Code */
			 0, 0, 0, /* # of Blocks */
			 0,       /* reserved */
			 0, 0, 0},/* Blocklen */
	   18);

    /* prepare to read cds in the previous mode */
    mode [0] = density;
    mode [6] =  secsize >> 8;   /* block length "msb" */
    mode [7] =  secsize & 0xFF; /* block length lsb */

    /* do the scsi cmd */
    return(handle_scsi_cmd (d,6, 12, 0,0,0));
  }
}

/* get current sector size from SCSI cdrom drive */
static unsigned int 
get_orig_sectorsize(cdrom_drive_t *d)
{
  if(mode_sense(d,12,0x01))return(-1);

  d->orgdens = d->sg_buffer[4];
  return(d->orgsize = ((int)(d->sg_buffer[10])<<8)+d->sg_buffer[11]);
}

/* switch CDROM scsi drives to given sector size  */
static int set_sectorsize (cdrom_drive_t *d,unsigned int secsize)
{
  return(mode_select(d,d->orgdens,secsize));
}

/* switch Toshiba/DEC and HP drives from/to cdda density */
int 
scsi_enable_cdda (cdrom_drive_t *d, int fAudioMode)
{
  if (fAudioMode) {
    if(mode_select(d,d->density,CDIO_CD_FRAMESIZE_RAW)){
      if(d->error_retry)
	cderror(d,"001: Unable to set CDROM to read audio mode\n");
      return(-1);
    }
  } else {
    if(mode_select(d,d->orgdens,d->orgsize)){
      if(d->error_retry)
	cderror(d,"001: Unable to set CDROM to read audio mode\n");
      return(-1);
    }
  }
  return(0);
}

typedef struct scsi_TOC {  /* structure of scsi table of contents (cdrom) */
  unsigned char reserved1;
  unsigned char bFlags;
  unsigned char bTrack;
  unsigned char reserved2;
  signed char start_MSB;
  unsigned char start_1;
  unsigned char start_2;
  unsigned char start_LSB;
} scsi_TOC;


/* read the table of contents from the cd and fill the TOC array */
/* Do it like the kernel ioctl driver; the 'all at once' approach
   fails on at least one Kodak drive. */

static int 
scsi_read_toc (cdrom_drive_t *d)
{
  int i,first,last;
  track_t i_tracks;

  /* READTOC, MSF format flag, res, res, res, res, Start track, len msb,
     len lsb, flags */

  /* read the header first */
  memcpy(d->sg_buffer, (char []){ CDIO_MMC_GPCMD_READ_TOC, 
	                          0, 0, 0, 0, 0, 1, 0, 12, 0}, 10);
  d->sg_buffer[1]=d->lun<<5;

  if (handle_scsi_cmd (d,10, 0, 12,'\377',1)){
    cderror(d,"004: Unable to read table of contents header\n");
    return(-4);
  }

  first=d->sg_buffer[2];
  last=d->sg_buffer[3];
  i_tracks=last-first+1;

  if (last > MAXTRK || first > MAXTRK || last<0 || first<0) {
    cderror(d,"003: CDROM reporting illegal number of tracks\n");
    return(-3);
  }

  for (i = first; i <= last; i++){
    memcpy(d->sg_buffer, (char []){ CDIO_MMC_GPCMD_READ_TOC, 
	                            0, 0, 0, 0, 0, 0, 0, 12, 0}, 10);
    d->sg_buffer[1]=d->lun<<5;
    d->sg_buffer[6]=i;
    
    if (handle_scsi_cmd (d,10, 0, 12,'\377',1)){
      cderror(d,"005: Unable to read table of contents entry\n");
      return(-5);
    }
    {
      scsi_TOC *toc=(scsi_TOC *)(d->sg_buffer+4);

      d->disc_toc[i-first].bFlags=toc->bFlags;
      d->disc_toc[i-first].bTrack=i;
      d->disc_toc[i-first].dwStartSector= d->adjust_ssize * 
	(((int)(toc->start_MSB)<<24) | 
	 (toc->start_1<<16)|
	 (toc->start_2<<8)|
	 (toc->start_LSB));
    }
  }

  memcpy(d->sg_buffer, (char []){ CDIO_MMC_GPCMD_READ_TOC, 
	                          0, 0, 0, 0, 0, 0, 0, 12, 0}, 10);
  d->sg_buffer[1]=d->lun<<5;
  d->sg_buffer[6]=0xAA;
    
  if (handle_scsi_cmd (d,10, 0, 12,'\377',1)){
    cderror(d,"002: Unable to read table of contents lead-out\n");
    return(-2);
  }
  {
    scsi_TOC *toc=(scsi_TOC *)(d->sg_buffer+4);
    
    d->disc_toc[i-first].bFlags=toc->bFlags;
    d->disc_toc[i-first].bTrack=0xAA;
    d->disc_toc[i-first].dwStartSector= d->adjust_ssize * 
	(((int)(toc->start_MSB)<<24) | 
	 (toc->start_1<<16)|
	 (toc->start_2<<8)|
	 (toc->start_LSB));
  }

  d->cd_extra = FixupTOC(d,i_tracks+1); /* include lead-out */
  return(i_tracks);
}

/* a contribution from Boris for IMS cdd 522 */
/* check this for ACER/Creative/Foo 525,620E,622E, etc? */
static int 
scsi_read_toc2 (cdrom_drive_t *d)
{
  uint32_t foo,bar;

  int i;
  track_t i_tracks;

  memcpy(d->sg_buffer, (char[]){ 0xe5, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 10);
  d->sg_buffer[5]=1;
  d->sg_buffer[8]=255;

  if (handle_scsi_cmd (d,10, 0, 256,'\377',1)){
    cderror(d,"004: Unable to read table of contents header\n");
    return(-4);
  }

  /* copy to our structure and convert start sector */
  i_tracks = d->sg_buffer[1];
  if (i_tracks > MAXTRK) {
    cderror(d,"003: CDROM reporting illegal number of tracks\n");
    return(-3);
  }

  for (i = 0; i < i_tracks; i++){
    memcpy(d->sg_buffer, (char[]){ 0xe5, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 10);
    d->sg_buffer[5]=i+1;
    d->sg_buffer[8]=255;
    
    if (handle_scsi_cmd (d,10, 0, 256,'\377',1)){
      cderror(d,"005: Unable to read table of contents entry\n");
      return(-5);
    }
    
    d->disc_toc[i].bFlags = d->sg_buffer[10];
    d->disc_toc[i].bTrack = i + 1;

    d->disc_toc[i].dwStartSector= d->adjust_ssize * 
	(((signed char)(d->sg_buffer[2])<<24) | 
	 (d->sg_buffer[3]<<16)|
	 (d->sg_buffer[4]<<8)|
	 (d->sg_buffer[5]));
  }

  d->disc_toc[i].bFlags = 0;
  d->disc_toc[i].bTrack = i + 1;
  memcpy (&foo, d->sg_buffer+2, 4);
  memcpy (&bar, d->sg_buffer+6, 4);
  d->disc_toc[i].dwStartSector = 
    d->adjust_ssize * (UINT32_FROM_BE(foo) + UINT32_FROM_BE(bar));

  d->disc_toc[i].dwStartSector= d->adjust_ssize * 
    ((((signed char)(d->sg_buffer[2])<<24) | 
      (d->sg_buffer[3]<<16)|
      (d->sg_buffer[4]<<8)|
      (d->sg_buffer[5]))+
     
     ((((signed char)(d->sg_buffer[6])<<24) | 
       (d->sg_buffer[7]<<16)|
       (d->sg_buffer[8]<<8)|
       (d->sg_buffer[9]))));


  d->cd_extra = FixupTOC(d,i_tracks+1);
  return(i_tracks);
}

/* These do one 'extra' copy in the name of clean code */

static int 
i_read_28 (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  int ret;
  memcpy(d->sg_buffer,(char []){
	   CDIO_MMC_GPCMD_READ_10, 
	     0, 0, 0, 0, 0, 0, 0, 0, 0},
	 10);

  if(d->fua)
    d->sg_buffer[1]=0x08;

  d->sg_buffer[1]|=d->lun<<5;

  d->sg_buffer[3] = (begin >> 16) & 0xFF;
  d->sg_buffer[4] = (begin >> 8) & 0xFF;
  d->sg_buffer[5] = begin & 0xFF;
  d->sg_buffer[8] = sectors;
  if((ret=handle_scsi_cmd(d,10,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

static int 
i_read_A8 (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  int ret;
  memcpy(d->sg_buffer,(char []){
	   CDIO_MMC_GPCMD_READ_12, 
	     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	 12);

  if(d->fua)
    d->sg_buffer[1]=0x08;

  d->sg_buffer[1]|=d->lun<<5;

  d->sg_buffer[3] = (begin >> 16) & 0xFF;
  d->sg_buffer[4] = (begin >> 8) & 0xFF;
  d->sg_buffer[5] = begin & 0xFF;
  d->sg_buffer[9] = sectors;
  if((ret=handle_scsi_cmd(d,12,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

static int 
i_read_D4_10 (cdrom_drive_t *d, void *p, long begin, long sectors)
{
  int ret;
  memcpy(d->sg_buffer,(char []){0xd4, 0, 0, 0, 0, 0, 0, 0, 0, 0},10);

  if(d->fua)
    d->sg_buffer[1]=0x08;

  d->sg_buffer[1]|=d->lun<<5;
  d->sg_buffer[3] = (begin >> 16) & 0xFF;
  d->sg_buffer[4] = (begin >> 8) & 0xFF;
  d->sg_buffer[5] = begin & 0xFF;
  d->sg_buffer[8] = sectors;
  if((ret=handle_scsi_cmd(d,10,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

static int i_read_D4_12 (cdrom_drive_t *d, void *p, long begin, long sectors)
{
  int ret;
  memcpy(d->sg_buffer,(char []){0xd4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},12);

  if(d->fua)
    d->sg_buffer[1]=0x08;

  d->sg_buffer[1]|=d->lun<<5;
  d->sg_buffer[3] = (begin >> 16) & 0xFF;
  d->sg_buffer[4] = (begin >> 8) & 0xFF;
  d->sg_buffer[5] = begin & 0xFF;
  d->sg_buffer[9] = sectors;
  if((ret=handle_scsi_cmd(d,12,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

static int 
i_read_D5 (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  int ret;
  memcpy(d->sg_buffer,(char []){0xd5, 0, 0, 0, 0, 0, 0, 0, 0, 0},10);

  if(d->fua)
    d->sg_buffer[1]=0x08;

  d->sg_buffer[1]|=d->lun<<5;
  d->sg_buffer[3] = (begin >> 16) & 0xFF;
  d->sg_buffer[4] = (begin >> 8) & 0xFF;
  d->sg_buffer[5] = begin & 0xFF;
  d->sg_buffer[8] = sectors;
  if((ret=handle_scsi_cmd(d,10,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

static int 
i_read_D8 (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  int ret;
  memcpy(d->sg_buffer,(char []){0xd8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},12);

  if(d->fua)
    d->sg_buffer[1]=0x08;

  d->sg_buffer[1]|=d->lun<<5;
  d->sg_buffer[3] = (begin >> 16) & 0xFF;
  d->sg_buffer[4] = (begin >> 8) & 0xFF;
  d->sg_buffer[5] = begin & 0xFF;
  d->sg_buffer[9] = sectors;
  if((ret=handle_scsi_cmd(d,12,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

static int 
i_read_mmc (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  int ret;
  /*  if(begin<=12007 && begin+sectors>12000){
    errno=EIO;
    return(TR_ILLEGAL);
  }*/

  memcpy(d->sg_buffer,(char []){
	   CDIO_MMC_GPCMD_READ_CD, 
	   0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0, 0},12);

  d->sg_buffer[3] = (begin >> 16) & 0xFF;
  d->sg_buffer[4] = (begin >> 8) & 0xFF;
  d->sg_buffer[5] = begin & 0xFF;
  d->sg_buffer[8] = sectors;
  if((ret=handle_scsi_cmd(d,12,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

static int 
i_read_mmc2 (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  int ret;
  memcpy(d->sg_buffer,(char []){
	   CDIO_MMC_GPCMD_READ_CD, 
	     0, 0, 0, 0, 0, 0, 0, 0, 0xf8, 0, 0},12);

  d->sg_buffer[3] = (begin >> 16) & 0xFF;
  d->sg_buffer[4] = (begin >> 8) & 0xFF;
  d->sg_buffer[5] = begin & 0xFF;
  d->sg_buffer[8] = sectors;
  if((ret=handle_scsi_cmd(d,12,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

static int 
i_read_mmc3 (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  int ret;
  memcpy(d->sg_buffer,(char []){
	   CDIO_MMC_GPCMD_READ_CD, 
	     4, 0, 0, 0, 0, 0, 0, 0, 0xf8, 0, 0},12);

  d->sg_buffer[3] = (begin >> 16) & 0xFF;
  d->sg_buffer[4] = (begin >> 8) & 0xFF;
  d->sg_buffer[5] = begin & 0xFF;
  d->sg_buffer[8] = sectors;
  if((ret=handle_scsi_cmd(d,12,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

/* straight from the MMC3 spec */
static inline void 
LBA_to_MSF(lba_t lba, unsigned char *M, unsigned char *S, unsigned char *F)
{
  if (lba>=-CDIO_PREGAP_SECTORS){
    *M   = ( lba+CDIO_PREGAP_SECTORS)/(CDIO_CD_FRAMES_PER_MIN);
    lba -= (*M)*(CDIO_CD_FRAMES_PER_MIN);
    *S   = (lba+CDIO_PREGAP_SECTORS)/CDIO_CD_FRAMES_PER_SEC;
    lba -= (*S)*CDIO_CD_FRAMES_PER_SEC;
    *F   = (lba+CDIO_PREGAP_SECTORS);
  } else {
    *M   = (lba+450150)/(CDIO_CD_FRAMES_PER_MIN);
    lba -= (*M)*(CDIO_CD_FRAMES_PER_MIN);
    *S   = (lba+450150)/CDIO_CD_FRAMES_PER_SEC;
    lba -= (*S)*CDIO_CD_FRAMES_PER_SEC;
    *F   = (lba+450150);
  }
}


static int 
i_read_msf (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  int ret;
  memcpy(d->sg_buffer,(char []){
	   CDIO_MMC_GPCMD_READ_MSF, 
	     0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0, 0},12);

  LBA_to_MSF(begin,d->sg_buffer+3,d->sg_buffer+4,d->sg_buffer+5);
  LBA_to_MSF(begin+sectors,d->sg_buffer+6,d->sg_buffer+7,d->sg_buffer+8);

  if((ret=handle_scsi_cmd(d,12,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

static int 
i_read_msf2 (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  int ret;
  memcpy(d->sg_buffer,(char []){
	   CDIO_MMC_GPCMD_READ_MSF, 
	     0, 0, 0, 0, 0, 0, 0, 0, 0xf8, 0, 0},12);

  LBA_to_MSF(begin,d->sg_buffer+3,d->sg_buffer+4,d->sg_buffer+5);
  LBA_to_MSF(begin+sectors,d->sg_buffer+6,d->sg_buffer+7,d->sg_buffer+8);

  if((ret=handle_scsi_cmd(d,12,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

static int 
i_read_msf3 (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  int ret;
  memcpy(d->sg_buffer,(char []){
	   CDIO_MMC_GPCMD_READ_MSF, 
	     4, 0, 0, 0, 0, 0, 0, 0, 0xf8, 0, 0},12);

  LBA_to_MSF(begin,d->sg_buffer+3,d->sg_buffer+4,d->sg_buffer+5);
  LBA_to_MSF(begin+sectors,d->sg_buffer+6,d->sg_buffer+7,d->sg_buffer+8);

  if((ret=handle_scsi_cmd(d,12,0,sectors * CDIO_CD_FRAMESIZE_RAW,'\177',1)))
    return(ret);
  if(p)memcpy(p,d->sg_buffer,sectors*CDIO_CD_FRAMESIZE_RAW);
  return(0);
}

static long 
scsi_read_map (cdrom_drive_t *d, void *p, long int begin, long int sectors,
	       int (*map)(cdrom_drive_t *, void *, long, long))
{
  int retry_count,err;
  char *buffer=(char *)p;

  /* read d->nsectors at a time, max. */
  sectors=(sectors>d->nsectors?d->nsectors:sectors);
  sectors=(sectors<1?1:sectors);

  retry_count=0;
  
  while(1) {
    if((err=map(d,(p?buffer:NULL),begin,sectors))){
      if(d->report_all){
	struct sg_header *sg_hd=(struct sg_header *)d->sg;
	char b[256];

	sprintf(b,"scsi_read error: sector=%ld length=%ld retry=%d\n",
		begin,sectors,retry_count);
	cdmessage(d,b);
	sprintf(b,"                 Sense key: %x ASC: %x ASCQ: %x\n",
		(int)(sg_hd->sense_buffer[2]&0xf),
		(int)(sg_hd->sense_buffer[12]),
		(int)(sg_hd->sense_buffer[13]));
	cdmessage(d,b);
	sprintf(b,"                 Transport error: %s\n",strerror_tr[err]);
	cdmessage(d,b);
	sprintf(b,"                 System error: %s\n",strerror(errno));
	cdmessage(d,b);

	fprintf(stderr,"scsi_read error: sector=%ld length=%ld retry=%d\n",
		begin,sectors,retry_count);
	fprintf(stderr,"                 Sense key: %x ASC: %x ASCQ: %x\n",
		(int)(sg_hd->sense_buffer[2]&0xf),
		(int)(sg_hd->sense_buffer[12]),
		(int)(sg_hd->sense_buffer[13]));
	fprintf(stderr,"                 Transport error: %s\n",strerror_tr[err]);
	fprintf(stderr,"                 System error: %s\n",strerror(errno));
      }

      if(!d->error_retry)return(-7);
      switch(errno){
      case EINTR:
	usleep(100);
	continue;
      case ENOMEM:
	/* D'oh.  Possible kernel error. Keep limping */
	usleep(100);
	if(sectors==1){
	  /* Nope, can't continue */
	  cderror(d,"300: Kernel memory error\n");
	  return(-300);  
	}
	if(d->report_all){
	  char b[256];
	  sprintf(b,"scsi_read: kernel couldn't alloc %ld bytes.  "
		  "backing off...\n",sectors*CDIO_CD_FRAMESIZE_RAW);
	    
	  cdmessage(d,b);
	}
	sectors--;
	continue;
      default:
	if(sectors==1){
	  if(errno==EIO)
	    if(d->fua==-1) /* testing for FUA support */
	      return(-7);
	  
	  /* *Could* be I/O or media error.  I think.  If we're at
	     30 retries, we better skip this unhappy little
	     sector. */
	  if(retry_count>MAX_RETRIES-1){
	    char b[256];
	    sprintf(b,"010: Unable to access sector %ld\n",
		    begin);
	    cderror(d,b);
	    return(-10);
	    
	  }
	  break;
	}

	/* Hmm.  OK, this is just a tad silly.  just in case this was
           a timeout and a reset happened, we need to set the drive
           back to cdda */
	reset_scsi(d);
      }
    }else{

      /* Did we get all the bytes we think we did, or did the kernel
         suck? */
      if(buffer){
	long i;
	for(i=sectors*CDIO_CD_FRAMESIZE_RAW;i>1;i-=2)
	  if(buffer[i-1]!='\177' || buffer[i-2]!='\177')
	    break;

	i/=CDIO_CD_FRAMESIZE_RAW;
	if(i!=sectors){
	  if(d->report_all){
	    char b[256];
	    sprintf(b,"scsi_read underrun: pos=%ld len=%ld read=%ld retry=%d\n",
		    begin,sectors,i,retry_count);
	    
	    cdmessage(d,b);
	  }
	  reset_scsi(d);
	}
	
	if(i>0)return(i);
      }else
	break;
    }
    
    retry_count++;
    if(sectors==1 && retry_count>MAX_RETRIES){
      cderror(d,"007: Unknown, unrecoverable error reading data\n");
      return(-7);
    }
    if(sectors>1)sectors=sectors/2;
    d->enable_cdda(d,0);
    d->enable_cdda(d,1);

  }
  return(sectors);
}

long int 
scsi_read_28 (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_28));
}

long int 
scsi_read_A8 (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_A8));
}

long int 
scsi_read_D4_10 (cdrom_drive_t *d, void *p, long int begin, long int sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_D4_10));
}

long int 
scsi_read_D4_12 (cdrom_drive_t *d, void *p, long int begin, 
			  long int sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_D4_12));
}

static long int 
scsi_read_D5 (cdrom_drive_t *d, void *p, long begin, long sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_D5));
}

long scsi_read_D8 (cdrom_drive_t *d, void *p, long begin, 
			       long sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_D8));
}

long scsi_read_mmc (cdrom_drive_t *d, void *p, long begin, 
			       long sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_mmc));
}

long scsi_read_mmc2 (cdrom_drive_t *d, void *p, long begin, 
			       long sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_mmc2));
}

static long 
scsi_read_mmc3 (cdrom_drive_t *d, void *p, long begin, long sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_mmc3));
}

static long 
scsi_read_msf (cdrom_drive_t *d, void *p, long begin, long sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_msf));
}

static long 
scsi_read_msf2 (cdrom_drive_t *d, void *p, long begin, long sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_msf2));
}

static long 
scsi_read_msf3 (cdrom_drive_t *d, void *p, long begin, long sectors)
{
  return(scsi_read_map(d,p,begin,sectors,i_read_msf3));
}


/* Some drives, given an audio read command, return only 2048 bytes
   of data as opposed to 2352 bytes.  Look for bytess at the end of the
   single sector verification read */

static int 
count_2352_bytes(cdrom_drive_t *d)
{
  long int i;
  for (i=CDIO_CD_FRAMESIZE_RAW-1; i>=0; i--)
    if(d->sg_buffer[i]!=(unsigned char)'\177')
      return(((i+3)>>2)<<2);

  return(0);
}

static int 
verify_nonzero(cdrom_drive_t *d)
{
  long i,flag=0;
  for (i=0; i<CDIO_CD_FRAMESIZE_RAW ;i++)
    if(d->sg_buffer[i]!=0){
      flag=1;
      break;
    }
  
  return(flag);
}

/* So many different read commands, densities, features...
   Verify that our selected 'read' command actually reads 
   nonzero data, else search through other possibilities */

static int 
verify_read_command(cdrom_drive_t *d)
{
  int i,j,k;
  int audioflag=0;

  int  (*enablecommand)  (cdrom_drive_t *d, int speed);
  long (*readcommand)   (cdrom_drive_t *d, void *p, long begin, 
		       long int sectors);
  unsigned char density;
  
  int16_t *buff=malloc(CDIO_CD_FRAMESIZE_RAW);

  cdmessage(d,"Verifying CDDA command set...\n");

  /* try the expected command set; grab the center of each track, look
     for data */

  if(d->enable_cdda(d,1)==0){
    
    for(i=1;i<=d->tracks;i++){
      if(cdda_track_audiop(d,i)==1){
	long firstsector=cdda_track_firstsector(d,i);
	long lastsector=cdda_track_lastsector(d,i);
	long sector=(firstsector+lastsector)>>1;
	audioflag=1;

	if(d->read_audio(d,buff,sector,1)>0){
	  if(count_2352_bytes(d)==CDIO_CD_FRAMESIZE_RAW){
	    cdmessage(d,"\tExpected command set reads OK.\n");
	    d->enable_cdda(d,0);
	    free(buff);
	    return(0);
	  }
	}
      }
    }
    
    d->enable_cdda(d,0);
  }

  if(!audioflag){
    cdmessage(d,"\tCould not find any audio tracks on this disk.\n");
    return(-403);
  }


  {
    const char *es="",*rs="";
    d->bigendianp=-1;
    density=d->density;
    readcommand=d->read_audio;
    enablecommand=d->enable_cdda;


    /* No nonzeroes?  D'oh.  Exhaustive search */
    cdmessage(d,"\tExpected command set FAILED!\n"
	      "\tPerforming full probe for CDDA command set...\n");
    
    /* loops:  
       density/enable no,  0x0/org,  0x04/org, 0x82/org
       read command read_10 read_12 read_nec read_sony read_mmc read_mmc2 */

    /* NEC test must come before sony; the nec drive expects d8 to be
       10 bytes, and a 12 byte verson (Sony) crashes the drive */

    for(j=0;j>=0;j++){
      int densitypossible=1;

      switch(j){
      case 0:
	d->read_audio=scsi_read_28;
	rs="28 0x,00";
	break;
      case 1:
	d->read_audio=scsi_read_A8;
	rs="a8 0x,00";
	break;
      case 2:
	d->read_audio=scsi_read_mmc;
	rs="be 00,10";
	densitypossible=0;
	break;
      case 3:
	d->read_audio=scsi_read_mmc2;
	rs="be 00,f8";
	densitypossible=0;
	break;
      case 4:
	d->read_audio=scsi_read_mmc3;
	rs="be 04,f8";
	densitypossible=0;
	break;

      case 5:
	d->read_audio=scsi_read_msf;
	rs="b9 00,10";
	densitypossible=0;
	break;
      case 6:
	d->read_audio=scsi_read_msf2;
	rs="b9 00,f8";
	densitypossible=0;
	break;
      case 7:
	d->read_audio=scsi_read_msf3;
	rs="b9 04,f8";
	densitypossible=0;
	break;

      case 8:
	d->read_audio=scsi_read_D4_10;
	rs="d4(10)0x";
	break;
      case 9:
	d->read_audio=scsi_read_D4_12;
	rs="d4(12)0x";
	break;
      case 10:
	d->read_audio=scsi_read_D5;
	rs="d5 0x,00";
	break;
      case 11:
	d->read_audio=scsi_read_D8;
	rs="d8 0x,00";
	j=-2;
	break;
      }
      
      for(i=0;i>=0;i++){
	switch(i){
	case 0:
	  d->density=0;
	  d->enable_cdda=dummy_exception;
	  es="none    ";
	  if(!densitypossible)i=-2; /* short circuit MMC style commands */
	  break;
	case 1:
	  d->density=0;
	  d->enable_cdda=scsi_enable_cdda;
	  es="yes/0x00";
	  break;
	case 2:
	  d->density=0x04;
	  d->enable_cdda=scsi_enable_cdda;
	  es="yes/0x04";
	  break;
	case 3:
	  d->density=0x82;
	  d->enable_cdda=scsi_enable_cdda;
	  es="yes/0x82";
	case 4:
	  d->density=0x81;
	  d->enable_cdda=scsi_enable_cdda;
	  es="yes/0x81";
	  i=-2;
	  break;
	}

	cdmessage(d,"\ttest -> density: [");
	cdmessage(d,es);
	cdmessage(d,"]  command: [");
	cdmessage(d,rs);
	cdmessage(d,"]\n");

	{
	  int densityflag=0;
	  int rejectflag=0;
	  int zeroflag=0;
	  int lengthflag=0;

	  if(d->enable_cdda(d,1)==0){
	    for(k=1;k<=d->tracks;k++){
	      if(cdda_track_audiop(d,k)==1){
		long firstsector=cdda_track_firstsector(d,k);
		long lastsector=cdda_track_lastsector(d,k);
		long sector=(firstsector+lastsector)>>1;
		
		if(d->read_audio(d,buff,sector,1)>0){
		  if((lengthflag=count_2352_bytes(d))==CDIO_CD_FRAMESIZE_RAW){
		    if(verify_nonzero(d)){
		      cdmessage(d,"\t\tCommand set FOUND!\n");
		      
		      free(buff);
		      d->enable_cdda(d,0);
		      return(0);
		    }else{
		      zeroflag++;
		    }
		  }
		}else{
		  rejectflag++;
		  break;
		}
	      }
	    }
	    d->enable_cdda(d,0);
	  }else{
	    densityflag++;
	  }
	  
	  if(densityflag)
	    cdmessage(d,"\t\tDrive rejected density set\n");
	  if(rejectflag){
	    char buffer[256];
	    sprintf(buffer,"\t\tDrive rejected read command packet(s)\n");
	    cdmessage(d,buffer);
	  }
	  if(lengthflag>0 && lengthflag<CDIO_CD_FRAMESIZE_RAW){
	    char buffer[256];
	    sprintf(buffer,"\t\tDrive returned at least one packet, but with\n"
		        "\t\tincorrect size (%d)\n",lengthflag);
	    cdmessage(d,buffer);
	  }
	  if(zeroflag){
	    char buffer[256];
	    sprintf(buffer,"\t\tDrive returned %d packet(s), but contents\n"
		        "\t\twere entirely zero\n",zeroflag);
	    cdmessage(d,buffer);
	  }
	}
      }
    }

    /* D'oh. */
    d->density=density;
    d->read_audio=readcommand;
    d->enable_cdda=enablecommand;

    cdmessage(d,"\tUnable to find any suitable command set from probe;\n"
	      "\tdrive probably not CDDA capable.\n");

    cderror(d,"006: Could not read any data from drive\n");

  }
  free(buff);
  return(-6);
}

static void 
check_fua_bit(cdrom_drive_t *d){
  long i;

  if(d->read_audio==scsi_read_mmc)return;
  if(d->read_audio==scsi_read_mmc2)return;
  if(d->read_audio==scsi_read_mmc3)return;

  cdmessage(d,"This command set may use a Force Unit Access bit.");
  cdmessage(d,"\nChecking drive for FUA bit support...\n");
  
  d->enable_cdda(d,1);
  d->fua=1;

  {
    int16_t *buff=malloc(CDIO_CD_FRAMESIZE_RAW);
    for(i=1;i<=d->tracks;i++){
      if(cdda_track_audiop(d,i)==1){
        long firstsector=cdda_track_firstsector(d,i);
        long lastsector=cdda_track_lastsector(d,i);
        long sector=(firstsector+lastsector)>>1;
      
        if(d->read_audio(d,buff,sector,1)>0){
	  cdmessage(d,"\tDrive accepted FUA bit.\n");
	  d->enable_cdda(d,0);
	  free(buff);
	  return;
        }
      }
    }
  
    d->fua=0;
    cdmessage(d,"\tDrive rejected FUA bit.\n");
    free(buff);
    return;
  }
}

static int 
check_atapi(cdrom_drive_t *d){
  int atapiret=-1;
  int fd = d->cdda_fd; /* this is the correct fd (not ioctl_fd), as the 
			  generic device is the device we need to check */
			  
  cdmessage(d,"\nChecking for SCSI emulation...\n");

  if (ioctl(fd,SG_EMULATED_HOST,&atapiret)){
    cderror(d,"\tSG_EMULATED_HOST ioctl() failed!\n");
    return(-1);
  } else {
    if(atapiret==1){
      cdmessage(d,"\tDrive is ATAPI (using SCSI host adaptor emulation)\n");
      /* Disable kernel SCSI command translation layer for access through sg */
      if (ioctl(fd,SG_SET_TRANSFORM,0))
	cderror(d,"\tCouldn't disable kernel command translation layer\n");
      d->is_atapi=1;
    }else{
      cdmessage(d,"\tDrive is SCSI\n");
      d->is_atapi=0;
    }

    return(d->is_atapi);
  }
}  

static int check_mmc(cdrom_drive_t *d){
  char *b;
  cdmessage(d,"\nChecking for MMC style command set...\n");

  d->is_mmc=0;
  if(mode_sense(d,22,0x2A)==0){
  
    b=d->sg_buffer;
    b+=b[3]+4;
    
    if((b[0]&0x3F)==0x2A){
      /* MMC style drive! */
      d->is_mmc=1;
      
      if(b[1]>=4){
	if(b[5]&0x1){
	  cdmessage(d,"\tDrive is MMC style\n");
	  return(1);
	}else{
	  cdmessage(d,"\tDrive is MMC, but reports CDDA incapable.\n");
	  cdmessage(d,"\tIt will likely not be able to read audio data.\n");
	  return(1);
	}
      }
    }
  }
  
  cdmessage(d,"\tDrive does not have MMC CDDA support\n");
  return(0);
}

static void 
check_exceptions(cdrom_drive_t *d, exception_t *list)
{
  int i=0;
  while(list[i].model){
    if(!strncmp(list[i].model,d->drive_model,strlen(list[i].model))){
      if(list[i].density)d->density=list[i].density;
      if(list[i].enable)d->enable_cdda=list[i].enable;
      if(list[i].read)d->read_audio=list[i].read;
      if(list[i].bigendianp!=-1)d->bigendianp=list[i].bigendianp;
      return;
    }
    i++;
  }
}

/* request vendor brand and model */
unsigned char *
scsi_inquiry(cdrom_drive_t *d){
  memcpy(d->sg_buffer,(char[]){ 0x12,0,0,0,56,0},6);

  if(handle_scsi_cmd(d,6, 0, 56,'\377',1)) {
    cderror(d,"008: Unable to identify CDROM model\n");
    return(NULL);
  }
  return (d->sg_buffer);
}


int 
scsi_init_drive(cdrom_drive_t *d){
  int ret;

  check_atapi(d);
  check_mmc(d);

  /* generic Sony type defaults; specialize from here */
  d->density = 0x0;
  d->enable_cdda = dummy_exception;
  d->read_audio = scsi_read_D8;
  d->fua=0x0;
  if(d->is_atapi)d->lun=0; /* it should already be; just to make sure */
      
  if(d->is_mmc){

    d->read_audio = scsi_read_mmc2;
    d->bigendianp=0;

    check_exceptions(d,mmc_list);

  }else{
    
    if(d->is_atapi){
      /* Not MMC maybe still uses CDIO_MMC_GPCMD_READ_CD */

      d->read_audio = scsi_read_mmc2;
      d->bigendianp=0;

      check_exceptions(d,atapi_list);

    }else{

      check_exceptions(d,scsi_list);

    }
  }

  /* we really do want the sector size at 2048 to begin.*/  
  if (!d->is_atapi)
    set_sectorsize(d, CDIO_CD_FRAMESIZE); 

  d->enable_cdda(d,0);

  d->read_toc = (!memcmp(d->drive_model, "IMS", 3) && !d->is_atapi) 
    ? scsi_read_toc2 : scsi_read_toc;
  d->set_speed = NULL;
  

  if (!d->is_atapi) {
    unsigned int sector_size= get_orig_sectorsize(d);
    
    if ( (sector_size < CDIO_CD_FRAMESIZE)
	 && set_sectorsize(d, CDIO_CD_FRAMESIZE) )
      d->adjust_ssize = CDIO_CD_FRAMESIZE / sector_size;
    else
      d->adjust_ssize = 1;
  } else
    d->adjust_ssize = 1;
  
  d->tracks=d->read_toc(d);
  if(d->tracks<1)
    return(d->tracks);

  tweak_SG_buffer(d);
  d->opened=1;

  if((ret=verify_read_command(d)))return(ret);
  check_fua_bit(d);

  d->error_retry=1;
  d->sg=realloc(d->sg,d->nsectors*CDIO_CD_FRAMESIZE_RAW + SG_OFF + 128);
  d->sg_buffer=d->sg+SG_OFF;
  d->report_all=1;
  return(0);
}

