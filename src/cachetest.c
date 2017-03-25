/*
  Copyright (C) 2014 Robert Kausch <robert.kausch@freac.org>
  Copyright (C) 2008 Monty <monty@xiph.org>

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

/* we can ask most drives what their various caches' sizes are, but no
   drive will tell if it caches redbook data.  None should, many do,
   and there's no way in (eg) MMC/ATAPI to tell a cdrom drive not to
   cache when accessing audio.  SCSI drives have a FUA facility, but
   it's not clear how many ignore it.  MMC does specify some cache
   side effect as part of SET READ AHEAD, but it's not clear we can
   rely on them.  For that reason, we need to empirically determine
   cache size and strategy used for reads. */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <cdio/paranoia/cdda.h>
#include <cdio/paranoia/paranoia.h>
#include <cdio/paranoia/version.h>
#include "cachetest.h"

/* not strictly just seeks, but also recapture and read resume when
   reading/readahead is suspended and idling */
#define MIN_SEEK_MS 6

#define reportC(...) {if(progress){fprintf(progress, __VA_ARGS__);}	\
    if(log){fprintf(log, __VA_ARGS__);}}
#define printC(...) {if(progress){fprintf(progress, __VA_ARGS__);}}
#define logC(...) {if(log){fprintf(log, __VA_ARGS__);}}

static int time_drive(cdrom_drive_t *d, FILE *progress, FILE *log, int lba, int len, int initial_seek){
  int i,x;
  int latency=0;
  double sum=0;
  double sumsq=0;
  int sofar;

  logC("\n");

  for(i=0,sofar=0;sofar<len;i++){
    int toread = (i==0 && initial_seek?1:len-sofar);
    int ret;
    /* first read should also trigger a short seek; one sector so seek duration dominates */
    if((ret=cdio_cddap_read_timed(d,NULL,lba+sofar,toread,&x))<=0){
      /* media error! grr!  retry elsewhere */
      if(ret==-404)return -404;
      return -1;
    }

    if(x>9999)x=9999;
    if(x<0)x=0;
    logC("%d:%d:%d ",lba+sofar,ret,x);

    sofar+=ret;
    if(i || !initial_seek){
      sum+=x;
      sumsq+= x*x /(float)ret;
    }else
      latency=x;
  }

  /* we count even the upper outliers because the drive is almost
     certainly reading ahead and that will work itself out as we keep
     reading to catch up.  Besides-- the tests would rather see too
     slow a timing than too fast; the timing data is used as an
     optimization when sleeping. */
  {
    double mean = sum/(float)(len-1);
    double stddev = sqrt( (sumsq/(float)(len-1) - mean*mean));

    if(initial_seek){
      printC("%4dms seek, %.2fms/sec read [%.1fx]",latency,mean,1000./75./mean);
      logC("\n\tInitial seek latency (%d sectors): %dms",len,latency);
    }

    logC("\n\tAverage read latency: %.2fms/sector (raw speed: %.1fx)",mean,1000./75./mean);
    logC("\n\tRead latency standard deviation: %.2fms/sector",stddev);

    return sum;
  }
}

static float retime_drive(cdrom_drive_t *d, FILE *progress, FILE *log, int lba, int readahead, float oldmean){
  int sectors = 2000;
  int total;
  float newmean;
  if(sectors*oldmean > 5000) sectors=5000/oldmean;
  readahead*=10;
  readahead/=9;
  if(readahead>sectors)sectors=readahead;

  printC("\bo");
  logC("\n\tRetiming drive...                               ");

  total = time_drive(d,NULL,log,lba,sectors,1);
  newmean = total/(float)sectors;

  logC("\n\tOld mean=%.2fms/sec, New mean=%.2fms/sec\n",oldmean,newmean);
  printC("\b");

  if(newmean>oldmean)return newmean;
  return oldmean;
}

int analyze_cache(cdrom_drive_t *d, FILE *progress, FILE *log, int speed){

  /* Some assumptions about timing:

     We can't perform cache determination timing based on looking at
     average transfer times; on slow setups, the speed of a drive
     reading sectors via PIO will not be reliably distinguishable from
     the same drive returning data from the cache via pio.  We need
     something even more noticable and reliable: the seek time. It is
     unlikely we'd ever see a seek latency of under ~10ms given the
     synchronization requirements of a CD and the maximum possible
     rotational velocity. A cache hit would always be faster, even
     with PIO.

     Further complicating things, we have to watch the data collection
     carefully as we're not always going to be on an unloaded system,
     and we even have to guard against other apps accessing the drive
     (something that should never happen on purpose, but could happen
     by accident).  As we know in our testing when seeks should never
     occur, a sudden seek-sized latency popping up in the middle of a
     collection is an indication that collection is possibly invalid.

     A second cause of 'spurious latency' would be media damage; if
     we're consistently hitting latency on the same sector during
     initial collection, may need to move past it. */

  int i,j,ret=0,x;
  int firstsector=-1;
  int lastsector=-1;
  int firsttest=-1;
  int lasttest=-1;
  int offset;
  int warn=0;
  int current=1000;
  int hi=15000;
  int cachesize=0;
  int readahead=0;
  int rollbehind=0;
  int cachegran=0;
  float mspersector=0;
  if(speed<=0)speed=-1;

  reportC("\n=================== Checking drive cache/timing behavior ===================\n");
  d->error_retry=0;

  /* verify the lib and cache analysis match */
  if(strcmp(LIBCDIO_PARANOIA_VERSION,paranoia_version())){
    reportC("\nWARNING: cdparanoia application (and thus the cache tests) does not match the"
	    "\ninstalled (or in use) libcdda_paranoia.so library.  The final verdict of this"
	    "\ntesting may or may not be accurate for the actual version of the paranoia"
	    "library.  Continuing anyway...\n\n");
  }

  /* find the longest stretch of available audio data */

  for(i=0;i<d->tracks;i++){
    if(cdda_track_audiop(d,i+1)==1){
      if(firsttest == -1)
	firsttest=cdda_track_firstsector(d,i+1);
      lasttest=cdda_track_lastsector(d,i+1);
      if(lasttest-firsttest > lastsector-firstsector){
	firstsector=firsttest;
	lastsector=lasttest;
      }
    }else{
      firsttest=-1;
      lasttest=-1;
    }
  }

  if(firstsector==-1){
    reportC("\n\tNo audio on disc; Cannot determine timing behavior...");
    return -1;
  }

  /* Dump some initial timing data to give a little context for human
     eyes.  Take readings ten minutes apart (45000 sectors) and at end of disk. */
  {
    int best=0;
    int bestcount=0;
    int iterating=0;

    offset = lastsector-firstsector-current-1;

    reportC("\nSeek/read timing:\n");

    while(offset>=firstsector){
      int m = offset/4500;
      int s = (offset-m*4500)/75;
      int f = offset-m*4500-s*75;
      int sofar;

      if(iterating){
	reportC("\n");
      }else{
	printC("\r");
	logC("\n");
      }
      reportC("\t[%02d:%02d.%02d]: ",m,s,f);

      /* initial seek to put at at a small offset past end of upcoming reads */
      if((ret=cdda_read(d,NULL,offset+current+1,1))<0){
	/* media error! grr!  retry elsewhere */
	if(ret==-404)return -1;
	reportC("\n\tWARNING: media error during read; continuing at next offset...");
	offset = (offset-firstsector+44999)/45000*45000+firstsector;
	offset-=45000;
	continue;
      }

      sofar=time_drive(d,progress, log, offset, current, 1);
      if(offset==firstsector)mspersector = sofar/(float)current;
      if(sofar==-404)
	return -1;
      else if(sofar<0){
	reportC("\n\tWARNING: media error during read; continuing at next offset...");
	offset = (offset-firstsector+44999)/45000*45000+firstsector;
	offset-=45000;
	continue;
      }else{
	if(!iterating){
	  if(best==0 || sofar*1.01<best){
	    best= sofar;
	    bestcount=0;
	  }else{
	    bestcount+=sofar;
	    if(bestcount>sofar && bestcount>4000)
	      iterating=1;
	  }
	}
      }

      if(iterating){
	offset = (offset-firstsector+44999)/45000*45000+firstsector;
	offset-=45000;
	printC("                 ");
      }else{
	offset--;
	printC(" spinning up...  ");
      }
    }
  }

  reportC("\n\nAnalyzing cache behavior...\n");

  /* search on cache size; cache hits are fast, seeks are not, so a
     linear search through cache hits up to a miss are faster than a
     bisection */
  {
    int under=1;
    int onex=0;
    current=0;
    offset = firstsector+10;

    while(current <= hi && under){
      int i,j;
      under=0;
      current++;

      if(onex){
	if(speed==-1){
	  logC("\tAttempting to reset read speed to full... ");
	}else{
	  logC("\tAttempting to reset read speed to %dx... ",speed);
	}
	if(cdda_speed_set(d,speed)){
	  logC("failed.\n");
	}else{
	  logC("drive said OK\n");
	}
	onex=0;
      }

      printC("\r");
      reportC("\tFast search for approximate cache size... %d sectors            ",current-1);
      logC("\n");

      for(i=0;i<25 && !under;i++){
	for(j=0;;j++){
	  int ret1=0,ret2=0;
	  if(i>=15){
	    int sofar=0;

	    if(i==15){
	      printC("\r");
	      reportC("\tSlow verify for approximate cache size... %d sectors",current-1);
	      logC("\n");

	      logC("\tAttempting to reduce read speed to 1x... ");
	      if(cdda_speed_set(d,1)){
		logC("failed.\n");
	      }else{
		logC("drive said OK\n");
	      }
	      onex=1;
	    }
	    printC(".");
	    logC("\t\t>>> ");

	    while(sofar<current){
	      ret1 = cdda_read_timed(d,NULL,offset+sofar,current-sofar,&x);
	      logC("slow_read=%d:%d:%d ",offset+sofar,ret1,x);
	      if(ret1<=0)break;
	      sofar+=ret1;
	    }
	  }else{
	    ret1 = cdda_read_timed(d,NULL,offset+current-1,1,&x);
	    logC("\t\t>>> fast_read=%d:%d:%d ",offset+current-1,ret1,x);

	    /* Some drives are 'easily distracted' when readahead is
	       running ahead of the read cursor, causing accesses of
	       the earliest sectors in the cache to show bursty
	       latency. If there's no seek here after a borderline
	       long read of the earliest sector in the cache, then the
	       cache must not have been dumped yet. */

	    if (ret==1 && i && x<MIN_SEEK_MS) {
	      under=1;
	      logC("\n");
	      break;
	    }
	  }
	  ret2 = cdda_read_timed(d,NULL,offset,1,&x);
	  logC("seek_read=%d:%d:%d\n",offset,ret2,x);

	  if(ret1<=0 || ret2<=0){
	    offset+=current+100;
	    if(j==10 || offset+current>lastsector){
	      reportC("\n\tToo many read errors while performing drive cache checks;"
		      "\n\t  aborting test.\n\n");
	      return(-1);
	    }
	    reportC("\n\tRead error while performing drive cache checks;"
		    "\n\t  choosing new offset and trying again.\n");
	  }else{
	    if(x==-1){
	      reportC("\n\tTiming error while performing drive cache checks; aborting test.\n");
	      return(-1);
	    }else{
	      if(x<MIN_SEEK_MS){
		under=1;
	      }
	      break;
	    }
	  }
	}
      }
    }
  }
  cachesize=current-1;

  printC("\r");
  if(cachesize==hi){
    reportC("\tWARNING: Cannot determine drive cache size or behavior!          \n");
    return 1;
  }else if(cachesize){
    reportC("\tApproximate random access cache size: %d sector(s)               \n",cachesize);
  }else{
    reportC("\tDrive does not cache nonlinear access                            \n");
    return 0;
  }

  /* does the readahead cache exceed the maximum Paranoia currently expects? */
  {
    cdrom_paranoia *p=paranoia_init(d);
    if(cachesize > paranoia_cachemodel_size(p,-1)){
      reportC("\nWARNING: This drive appears to be caching more sectors of\n"
	      "           readahead than Paranoia can currently handle!\n");
      warn=1;

    }
    paranoia_free(p);
  }
  if(speed==-1){
    logC("\tAttempting to reset read speed to full... ");
  }else{
    logC("\tAttempting to reset read speed to %d... ",speed);
  }
  if(cdda_speed_set(d,speed)){
    logC("failed.\n");
  }else{
    logC("drive said OK\n");
  }

  /* This is similar to the Fast search above, but just in case the
     cache is being tracked as multiple areas that are treated
     differently if non-contiguous.... */
  {
    int seekoff = cachesize*3;
    int under=0;
    reportC("\tVerifying that cache is contiguous...");

    for(i=0;i<20 && !under;i++){
      printC(".");
      for(j=0;;j++){
	int ret1,ret2;

	if(offset+seekoff>lastsector){
	  reportC("\n\tOut of readable space on CDROM while performing drive checks;"
		  "\n\t  aborting test.\n\n");
	  return(-1);
	}


	ret1 = cdda_read_timed(d,NULL,offset+seekoff,1,&x);
	logC("\t\t>>> %d:%d:%d ",offset+seekoff,ret1,x);
	ret2 = cdda_read_timed(d,NULL,offset,1,&x);
	logC("seek_read:%d:%d:%d\n",offset,ret2,x);

	if(ret1<=0 || ret2<=0){
	  offset+=cachesize+100;
	  if(j==10){
	    reportC("\n\tToo many read errors while performing drive cache checks;"
		    "\n\t  aborting test.\n\n");
	    return(-1);
	  }
	  reportC("\n\tRead error while performing drive cache checks;"
		  "\n\t  choosing new offset and trying again.\n");
	}else{
	  if(x==-1){
	    reportC("\n\tTiming error while performing drive cache checks; aborting test.\n");
	    return(-1);
	  }else{
	    if(x<MIN_SEEK_MS)under=1;
	    break;
	  }
	}
      }
    }
    printC("\r");
    if(under){
      reportC("\nWARNING: Drive cache does not appear to be contiguous!\n");
      warn=1;
    }else{
      reportC("\tDrive cache tests as contiguous                           \n");
    }
  }

  /* The readahead cache size ascertained above is likely qualified by
     background 'rollahead'; that is, the drive's readahead process is
     often working ahead of our actual linear reads, and if reads stop
     or are interrupted, readahead continues and overflows the cache.
     It is also the case that the cache size we determined above is
     slightly too low because readahead is probably always working
     ahead of reads.

     Determine the rollahead size a few ways (which may disagree:
     1) Read number of sectors equal to cache size; pause; read backward until seek
     2) Read sectors equal to cache-rollahead; verify reading back to beginning does not seek
     3) Read sectors equal to cache; pause; read ahead until seek delay
  */

  {
    int lower=0;
    int gran=64;
    int it=3;
    int tests=0;
    int under=1;
    readahead=0;

    while(gran>1 || under){
      tests++;
      if(tests>8 && gran<64){
	gran<<=3;
	tests=0;
	it=3;
      }
      if(gran && !under){
	gran>>=3;
	tests=0;
	if(gran==1)it=10;
      }

      under=0;
      readahead=lower+gran;

      printC("\r");
      logC("\n");
      reportC("\tTesting background readahead past read cursor... %d",readahead);
      printC("           \b\b\b\b\b\b\b\b\b\b\b");
      for (i=0;i<it;i++) {
	int sofar=0, ret=0;
	logC("\n\t\t%d >>> ",i);

	while (sofar<cachesize) {
	  ret = cdda_read_timed(d, NULL, offset+sofar,
				     cachesize-sofar, &x);
	  if (ret<=0) goto error;
	  logC("%d:%d:%d ", offset+sofar, ret, x);

	  /* Some drives can lose sync and perform an internal resync,
	     which can also cause readahead to restart.  If we see
	     seek-like delays during the initial cahe load, retry the
	     preload. */

	  sofar += ret;
	}

	printC(".");

	/* what we'd predict is needed to let the readahead process work. */
	{
	  int usec=mspersector*(readahead)*(6+i)*200;
	  int max= 13000*2*readahead; /* corresponds to .5x */
	  if(usec>max)usec=max;
	  logC("sleep=%dus ",usec);
	  usleep(usec);
	}

	/* seek to offset+cachesize+readahead */
	ret = cdda_read_timed(d,NULL,offset+cachesize+readahead-1,1,&x);
	if(ret<=0)break;
	logC("seek=%d:%d:%d",offset+cachesize+readahead-1,ret,x);
	if(x<MIN_SEEK_MS){
	  under=1;
	  break;
	}else if(i%3==1){
	  /* retime the drive just to be conservative */
	  mspersector=retime_drive(d, progress, log, offset, readahead, mspersector);
	}
      }

      if(under)
	lower=readahead;

    }
    readahead=lower;
  }
  logC("\n");
  printC("\r");
  if(readahead==0){
    reportC("\tDrive does not read ahead past read cursor (very strange)     \n");
  }else{
    reportC("\tDrive readahead past read cursor: %d sector(s)                \n",readahead);
  }

   reportC("\tTesting cache tail cursor...");

  while(1){
    rollbehind=cachesize;

    for(i=0;i<10 && rollbehind;){
      int sofar=0,ret=0,retry=0;
      logC("\n\t\t>>> ");
      printC(".");
      while(sofar<cachesize){
	ret = cdda_read_timed(d,NULL,offset+sofar,cachesize-sofar,&x);
	if(ret<=0)goto error;
	logC("%d:%d:%d ",offset+sofar,ret,x);
	sofar+=ret;
      }

      /* Pause what we'd predict is needed to let the readahead process work. */
      {
	int usec=mspersector*readahead*1500;
	logC("\n\t\tsleeping %d microseconds",usec);
	usleep(usec);
      }

      /* read backwards until we seek */
      logC("\n\t\t<<< ");
      sofar=rollbehind;
      while(sofar>0){
	sofar--;
	ret = cdda_read_timed(d,NULL,offset+sofar,1,&x);
	if(ret<=0)break;
	logC("%d:%d:%d ",sofar,ret,x);
	if(x>=MIN_SEEK_MS){
	  if(rollbehind != sofar+1){
	    rollbehind=sofar+1;
	    i=0;
	  }else{
	    i++;
	  }
	  break;
	}
	if(sofar==0)rollbehind=0;
      }

    error:
      if(ret<=0){
	offset+=cachesize;
	retry++;
	if(retry>10 || offset+cachesize>lastsector){
	  reportC("\n\tToo many read errors while performing drive cache checks;"
		  "\n\t  aborting test.\n\n");
	  return(-1);
	}
	reportC("\n\tRead error while performing drive cache checks;"
		"\n\t  choosing new offset and trying again.\n");
	continue;
      }
    }

    /* verify that the drive timing didn't suddenly change */
    {
      float newms=retime_drive(d, progress, log, offset, readahead, mspersector);
      if(newms > mspersector*1.2){
	mspersector=newms;
	printC("\r");
	reportC("\tDrive timing changed during test; retrying...");
	continue;
      }
    }
    break;

  }

  logC("\n");
  printC("\r");
  if(rollbehind==0){
    reportC("\tCache tail cursor tied to read cursor                      \n");
  }else{
    reportC("\tCache tail rollbehind: %d sector(s)                        \n",rollbehind);
  }
  reportC("\tTesting granularity of cache tail");

  while(1){
    cachegran=cachesize+1;
    for(i=0;i<10 && cachegran;){
      int sofar=0,ret=0,retry=0;
      logC("\n\t\t>>> ");
      printC(".");
      while(sofar<cachesize+1){
	ret = cdda_read_timed(d,NULL,offset+sofar,cachesize-sofar+1,&x);
	if(ret<=0)goto error2;
	logC("%d:%d:%d ",offset+sofar,ret,x);
	sofar+=ret;
      }

      /* Pause what we'd predict is needed to let the readahead process work. */
      {
	int usec=mspersector*readahead*1500;
	logC("\n\t\tsleeping %d microseconds",usec);
	usleep(usec);
      }

      /* read backwards until we seek */
      logC("\n\t\t<<< ");
      sofar=cachegran;
      while(sofar){
	sofar--;
	ret = cdda_read_timed(d,NULL,offset+sofar,1,&x);
	if(ret<=0)break;
	logC("%d:%d:%d ",offset+sofar,ret,x);
	if(x>=MIN_SEEK_MS){
	  if(cachegran == sofar+1){
	    i++;
	  }else{
	    cachegran=sofar+1;
	    i=0;
	  }
	  break;
	}
	if(sofar==0)cachegran=0;
      }

    error2:
      if(ret<=0){
	offset+=cachesize;
	retry++;
	if(retry>10 || offset+cachesize>lastsector){
	  reportC("\n\tToo many read errors while performing drive cache checks;"
		  "\n\t  aborting test.\n\n");
	  return(-1);
	}
	reportC("\n\tRead error while performing drive cache checks;"
		"\n\t  choosing new offset and trying again.\n");
	continue;
      }
    }

    /* verify that the drive timing didn't suddenly change */
    {
      float newms=retime_drive(d, progress, log, offset, readahead, mspersector);
      if(newms > mspersector*1.2){
	mspersector=newms;
	printC("\r");
	reportC("\tDrive timing changed during test; retrying...");
	continue;
      }
    }
    break;

  }

  cachegran -= rollbehind;

  logC("\n");
  printC("\r");
  reportC("\tCache tail granularity: %d sector(s)                      \n",cachegran);


  /* Verify that a read that begins before the cached readahead dumps
     the entire readahead cache */

  /* This is tricky because we can't simply read a one sector back
     seek, then rely on timing/seeking of subsequent sectors; the
     drive may well not seek ahead if reading linearly would be faster
     (and it often will be), and simply reading ahead after the seek
     and watching timing will be inaccurate because the drive may roll
     some readahead into the initial seek/read before returning the
     first block. */

  /* we will need to use the timing of reading from media in one form
     or another and thus need to guard against slow bus transfer times
     [eg, no DMA] swamping the actual read time from media. */

  /* sample cache access for five realtime seconds. */
  {
    float cachems;
    float readms;
    int readsize = cachesize-rollbehind-8;
    int retry=0;

    if(readsize>cachesize-1)readsize=cachesize-1;

    if(readsize<7){
      reportC("\tCache size (considering rollbehind) too small to test cache speed.\n");
    }else{
      reportC("\tTesting cache transfer speed...");

      /* cache timing isn't dependent on rotational speed, so get a good
	 read and then just hammer the cache; we will only need to do it once */

      /* we need to time the cache using the most conservative
	 possible read pattern; many drives will flush cache on *any*
	 nonlinear access, but not if the read starts from the same
	 point.  The original cache size verification algo knows this,
	 and we need to do it the same way here (this the '0' for
	 'initial_seek' on time_drve */

      while(1){
	int ret=time_drive(d, NULL, log, offset, readsize, 0);
	if(ret==-404) return -1;
	if(ret>0)break;
	retry++;
	if(retry==10){
	  reportC("\n\tToo many read errors while performing drive cache checks;"
		  "\n\t  aborting test.\n\n");
	  return(-1);
	}
	reportC("\n\tRead error while performing drive cache checks;"
		"\n\t  choosing new offset and trying again.\n");
      }

      {
	int elapsed=0;
	int sectors=0;
	int spinner=0;
	while(elapsed<5000){
	  sectors += readsize;
	  elapsed += time_drive(d, NULL, log, offset, readsize, 0);
	  spinner = elapsed*5/1000%4;
	  printC("\b%c",(spinner==0?'o':(spinner==1?'O':(spinner==2?'o':'.'))));
	}
	printC("\r");
	logC("\n");
	cachems = elapsed/(float)sectors;
	reportC("\t        Cache read speed: %.2fms/sector [%dx]\n",
		cachems,(int)(1000./75./cachems));
      }

      if(cachems*3>mspersector){
	reportC("\tCache access insufficiently faster than media access to\n"
		"\t\tperform cache backseek tests\n\n");
      }else{

	/* now do the read/backseek combo */
	reportC("\tTesting that backseek flushes cache...");
	{
	  int total=0;
	  int elapsed=0;
	  int sectors=0;
	  int spinner=0;
	  int retry=0;
	  while(elapsed<5000 && total<25){ /* don't kill the drive */
	    int ret;
	    while(1){
	      /* need to force seek/flush, but don't kill the drive */
	      int seekpos = offset+cachesize+20000;
	      if(seekpos>lastsector-150)seekpos=lastsector-150;
	      ret=cdda_read(d, NULL, seekpos, 1);
	      if(ret>0) ret=time_drive(d, NULL, log, offset+1, readsize, 1);
	      if(ret>=0) ret=time_drive(d, NULL, log, offset, readsize, 1);

	      if(ret<=0){
		retry++;
		if(retry==10){
		  reportC("\n\tToo many read errors while performing drive cache checks;"
			  "\n\t  aborting test.\n\n");
		  return(-1);
		}
		reportC("\n\tRead error while performing drive cache checks; retrying...");
	      }else
		break;
	    }

	    sectors += (readsize-1);
	    elapsed += ret;
	    total++;

	    spinner = elapsed*5/1000%4;
	    printC("\b%c",(spinner==0?'o':(spinner==1?'O':(spinner==2?'o':'.'))));
	  }

	  printC("\r");
	  logC("\n");
	  readms = elapsed/(float)sectors;
	  reportC("\t        Access speed after backseek: %.2fms/sector [%dx]\n",
		  readms,(int)(1000./75./readms));
	  if(readms*2. < mspersector ||
	     cachems*2. > readms){
	    reportC("\tWARNING: Read timing after backseek faster than expected!\n"
		    "\t         It's possible/likely that this drive is not\n"
		    "\t         flushing the readahead cache on backward seeks!\n\n");
	    warn=1;
	  }else{
	    reportC("\tBackseek flushes the cache as expected\n");
	  }
	}
      }
    }
  }

  return warn;
}
