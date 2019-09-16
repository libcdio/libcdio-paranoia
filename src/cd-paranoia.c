/*
  Copyright (C) 2004-2012, 2014-2015, 2017 Rocky Bernstein <rocky@gnu.org>
  Copyright (C) 2014 Robert Kausch <robert.kausch@freac.org>
  Copyright (C) 1998 Monty <xiphmont@mit.edu>

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

  See ChangeLog for recent changes.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
# define __CDIO_CONFIG_H__ 1
#endif

#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif

#ifdef HAVE_STDARG_H
# include <stdarg.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifndef PATH_MAX
# ifdef MAXPATHLEN
#  define PATH_MAX MAXPATHLEN
# else
#  define PATH_MAX 4096
# endif
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "getopt.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifndef ENOMEDIUM
#define ENOMEDIUM -1
#endif

#include <math.h>
#include <sys/time.h>

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if !defined(HAVE_GETTIMEOFDAY)
/* MinGW uses sys/time.h and sys/timeb.h to roll its own gettimeofday() */
# if defined(HAVE_SYS_TIME_H) && defined(HAVE_SYS_TIMEB_H)
# include <sys/time.h>
# include <sys/timeb.h>
static void gettimeofday(struct timeval* tv, void* timezone);
# endif
#endif /* !defined(HAVE_GETTIMEOFDAY) */

#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/paranoia/cdda.h>
#include <cdio/paranoia/paranoia.h>
#include <cdio/bytesex.h>
#include <cdio/mmc.h>
#include "utils.h"
#include "report.h"
#include "version.h"
#include "header.h"
#include "buffering_write.h"
#include "cachetest.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* I wonder how many alignment issues this is gonna trip in the
   future...  it shouldn't trip any...  I guess we'll find out :) */

static int
bigendianp(void)
{
  int test=1;
  char *hack=(char *)(&test);
  if(hack[0])return(0);
  return(1);
}

static long
parse_offset(cdrom_drive_t *d, char *offset, int begin)
{
  track_t i_track= CDIO_INVALID_TRACK;
  long hours   = -1;
  long minutes = -1;
  long seconds = -1;
  long sectors = -1;
  char *time   = NULL;
  char *temp   = NULL;
  long ret;

  if (!offset) return -1;

  /* separate track from time offset */
  temp=strchr(offset,']');
  if(temp){
    *temp='\0';
    temp=strchr(offset,'[');
    if(temp==NULL){
      report("Error parsing span argument");
      exit(1);
    }
    *temp='\0';
    time=temp+1;
  }

  /* parse track */
  {
    int chars=strspn(offset,"0123456789");
    if(chars>0){
      offset[chars]='\0';
      i_track=atoi(offset);
      if (i_track >= cdio_get_first_track_num(d->p_cdio) + d->tracks) {
        /*take track i_first_track-1 as pre-gap of 1st track*/
	report("Track #%d does not exist.",i_track);
        exit(1);
      }
    }
  }

  while(time){
    long val,chars;
    char *sec=strrchr(time,'.');
    if(!sec)sec=strrchr(time,':');
    if(!sec)sec=time-1;

    chars=strspn(sec+1,"0123456789");
    if(chars)
      val=atoi(sec+1);
    else
      val=0;

    switch(*sec){
    case '.':
      if(sectors!=-1){
        report("Error parsing span argument");
        exit(1);
      }
      sectors=val;
      break;
    default:
      if(seconds==-1)
        seconds=val;
      else
        if(minutes==-1)
          minutes=val;
        else
          if(hours==-1)
            hours=val;
          else{
            report("Error parsing span argument");
            exit(1);
          }
      break;
    }

    if (sec<=time) break;
    *sec='\0';
  }

  if (i_track == CDIO_INVALID_TRACK) {
    if (seconds==-1 && sectors==-1) return -1;
    if (begin==-1) {
      ret=cdda_disc_firstsector(d);
    } else
      ret = begin;
  } else {
    if ( seconds==-1 && sectors==-1 ) {
      if (begin==-1){
        /* first half of a span */
        return(cdda_track_firstsector(d, i_track));
      }else{
        return(cdda_track_lastsector(d, i_track));
      }
    } else {
      /* relative offset into a track */
      ret=cdda_track_firstsector(d, i_track);
    }
  }

  /* OK, we had some sort of offset into a track */

  if (sectors != -1) ret += sectors;
  if (seconds != -1) ret += seconds*CDIO_CD_FRAMES_PER_SEC;
  if (minutes != -1) ret += minutes*CDIO_CD_FRAMES_PER_MIN;
  if (hours   != -1) ret += hours  *60*CDIO_CD_FRAMES_PER_MIN;

  /* We don't want to outside of the track; if it's relative, that's OK... */
  if( i_track != CDIO_INVALID_TRACK ){
    if (cdda_sector_gettrack(d,ret) != i_track) {
      report("Time/sector offset goes beyond end of specified track.");
      exit(1);
    }
  }

  /* Don't pass up end of session */

  if( ret>cdda_disc_lastsector(d) ) {
    report("Time/sector offset goes beyond end of disc.");
    exit(1);
  }

  return(ret);
}

static void
display_toc(cdrom_drive_t *d)
{
  long audiolen=0;
  track_t i;

  report("\nTable of contents (audio tracks only):\n"
         "track        length               begin        copy pre ch\n"
         "===========================================================");

  for( i=cdio_get_first_track_num(d->p_cdio);
       i<=cdio_get_last_track_num(d->p_cdio); i++)
    if ( cdda_track_audiop(d,i) > 0 ) {

      lsn_t sec=cdda_track_firstsector(d,i);
      lsn_t off=cdda_track_lastsector(d,i)-sec+1;

      report("%3d.  %7ld [%02d:%02d.%02d]  %7ld [%02d:%02d.%02d]  %s %s %s",
	     i,
	     (long int) off,
             (int) (off/(CDIO_CD_FRAMES_PER_MIN)),
             (int) ((off/CDIO_CD_FRAMES_PER_SEC) % CDIO_CD_SECS_PER_MIN),
             (int) (off % CDIO_CD_FRAMES_PER_SEC),
	     (long int) sec,
             (int) (sec/(CDIO_CD_FRAMES_PER_MIN)),
             (int) ((sec/CDIO_CD_FRAMES_PER_SEC) % CDIO_CD_SECS_PER_MIN),
             (int) (sec % CDIO_CD_FRAMES_PER_SEC),
	     cdda_track_copyp(d,i)?"  OK":"  no",
	     cdda_track_preemp(d,i)?" yes":"  no",
	     cdda_track_channels(d,i)==2?" 2":" 4");
      audiolen+=off;
    }
  report("TOTAL %7ld [%02d:%02d.%02d]    (audio only)",
	 audiolen,
         (int) (audiolen/(CDIO_CD_FRAMES_PER_MIN)),
         (int) ((audiolen/CDIO_CD_FRAMES_PER_SEC) % CDIO_CD_SECS_PER_MIN),
	 (int) (audiolen % CDIO_CD_FRAMES_PER_SEC));
  report(" ");
}

#include "usage.h"
static void usage(FILE *f)
{
  fprintf( f, usage_help);
}

static long callbegin;
static long callend;
static long callscript=0;

static int skipped_flag=0;
static int abort_on_skip=0;
static FILE *logfile = NULL;
static int logfile_open=0;
static int reportfile_open=0;

#if TRACE_PARANOIA
static void
callback(long int inpos, paranoia_cb_mode_t function)
{
}
#else
static const char *callback_strings[16]={
  "read",
  "verify",
  "jitter",
  "correction",
  "scratch",
  "scratch repair",
  "skip",
  "drift",
  "backoff",
  "overlap",
  "dropped",
  "duped",
  "transport error",
  "cache error",
  "wrote",
  "finished",
};

static void
callback(long int inpos, paranoia_cb_mode_t function)
{
  /*

 (== PROGRESS == [--+!---x-------------->           | 007218 01 ] == :-) . ==)

 */

  int graph=30;
  char buffer[256];
  static long c_sector=0, v_sector=0;
  static char dispcache[]="                              ";
  static int last=0;
  static long lasttime=0;
  long int sector, osector=0;
  struct timeval thistime;
  static char heartbeat=' ';
  int position=0,aheadposition=0;
  static int overlap=0;
  static int printit=-1;

  static int slevel=0;
  static int slast=0;
  static int stimeout=0;
  static int cacheerr=0;
  const char *smilie="= :-)";

  if (callscript)
    fprintf(stderr, "##: %d [%s] @ %ld\n",
            function, ((int) function >= 0 && (int) function < 16 ?
                       callback_strings[function] : ""),
            inpos);
  else{
    if(function==PARANOIA_CB_CACHEERR){
      if(!cacheerr){
	fprintf(stderr,
		"\rWARNING: The CDROM drive appears to be seeking impossibly quickly.\n"
		"This could be due to timer bugs, a drive that really is improbably fast,\n"
		"or, most likely, a bug in cdparanoia's cache modelling.\n\n"
		"Please consider using the -A option to perform an analysis run, then mail\n"
		"the cdparanoia.log file produced by the analysis to paranoia-dev@xiph.org\n"
		"to assist developers in correcting the problem.\n\n");
      }
      cacheerr++;
    }
  }

  if(!quiet){
    long test;
    osector=inpos;
    sector=inpos/CD_FRAMEWORDS;

    if(printit==-1){
       if(isatty(STDERR_FILENO) || (logfile != NULL)){
        printit=1;
      }else{
        printit=0;
      }
    }

    if(printit==1){  /* else don't bother; it's probably being
                        redirected */
      position=((float)(sector-callbegin)/
                (callend-callbegin))*graph;

      aheadposition=((float)(c_sector-callbegin)/
                     (callend-callbegin))*graph;

      if(function == PARANOIA_CB_WROTE){
        v_sector=sector;
        return;
      }
      if (function == PARANOIA_CB_FINISHED) {
        last=8;
        heartbeat='*';
        slevel=0;
        v_sector=sector;
      } else
        if(position<graph && position>=0)
          switch(function){
          case PARANOIA_CB_VERIFY:
            if(stimeout>=30){
              if(overlap>CD_FRAMEWORDS)
                slevel=2;
              else
                slevel=1;
            }
            break;
          case PARANOIA_CB_READ:
            if(sector>c_sector)c_sector=sector;
            break;

          case PARANOIA_CB_FIXUP_EDGE:
            if(stimeout>=5){
              if(overlap>CD_FRAMEWORDS)
                slevel=2;
              else
                slevel=1;
            }
            if(dispcache[position]==' ')
              dispcache[position]='-';
            break;
          case PARANOIA_CB_FIXUP_ATOM:
            if(slevel<3 || stimeout>5)slevel=3;
            if(dispcache[position]==' ' ||
               dispcache[position]=='-')
              dispcache[position]='+';
            break;
          case PARANOIA_CB_READERR:
            slevel=6;
	    if(dispcache[position]!='V' && dispcache[position]!='C')
              dispcache[position]='e';
            break;
	  case PARANOIA_CB_CACHEERR:
	    slevel=8;
	    dispcache[position]='C';
	    break;
          case PARANOIA_CB_SKIP:
            slevel=8;
	    if(dispcache[position]!='C')
	      dispcache[position]='V';
            break;
          case PARANOIA_CB_OVERLAP:
            overlap=osector;
            break;
          case PARANOIA_CB_SCRATCH:
            slevel=7;
            break;
          case PARANOIA_CB_DRIFT:
            if(slevel<4 || stimeout>5)slevel=4;
            break;
          case PARANOIA_CB_FIXUP_DROPPED:
          case PARANOIA_CB_FIXUP_DUPED:
            slevel=5;
            if(dispcache[position]==' ' ||
               dispcache[position]=='-' ||
               dispcache[position]=='+')
              dispcache[position]='!';
            break;
          case PARANOIA_CB_REPAIR:
          case PARANOIA_CB_BACKOFF:
            break;
          case PARANOIA_CB_WROTE:
          case PARANOIA_CB_FINISHED:
	    /* Handled above */
	    ;
          }

      switch(slevel){
      case 0:  /* finished, or no jitter */
        if(skipped_flag)
          smilie=" 8-X";
        else
          smilie=" :^D";
        break;
      case 1:  /* normal.  no atom, low jitter */
        smilie=" :-)";
        break;
      case 2:  /* normal, overlap > 1 */
        smilie=" :-|";
        break;
      case 4:  /* drift */
        smilie=" :-/";
        break;
      case 3:  /* unreported loss of streaming */
        smilie=" :-P";
        break;
      case 5:  /* dropped/duped bytes */
        smilie=" 8-|";
        break;
      case 6:  /* scsi error */
        smilie=" :-0";
        break;
      case 7:  /* scratch */
        smilie=" :-(";
        break;
      case 8:  /* skip */
        smilie=" ;-(";
        skipped_flag=1;
        break;

      }

      gettimeofday(&thistime,NULL);
      test=thistime.tv_sec*10+thistime.tv_usec/100000;

      if (lasttime!=test || function == PARANOIA_CB_FINISHED
	  || slast!=slevel ) {
        if (lasttime!=test || function == PARANOIA_CB_FINISHED) {
          last++;
          lasttime=test;
          if(last>7)last=0;
          stimeout++;
          switch(last){
          case 0:
            heartbeat=' ';
            break;
          case 1:case 7:
            heartbeat='.';
            break;
          case 2:case 6:
            heartbeat='o';
            break;
          case 3:case 5:
            heartbeat='0';
            break;
          case 4:
            heartbeat='O';
            break;
          }
          if(function == PARANOIA_CB_FINISHED)
            heartbeat='*';

        }
        if(slast!=slevel){
          stimeout=0;
        }
        slast=slevel;

        if (abort_on_skip && skipped_flag
	    && function != PARANOIA_CB_FINISHED ) {
          sprintf(buffer,
                  "\r (== PROGRESS == [%s| %06ld %02d ] ==%s %c ==)   ",
                  "  ...aborting; please wait... ",
                  v_sector,overlap/CD_FRAMEWORDS,smilie,heartbeat);
        }else{
          if(v_sector==0)
            sprintf(buffer,
                    "\r (== PROGRESS == [%s| ...... %02d ] ==%s %c ==)   ",
                    dispcache,overlap/CD_FRAMEWORDS,smilie,heartbeat);

          else
            sprintf(buffer,
                    "\r (== PROGRESS == [%s| %06ld %02d ] ==%s %c ==)   ",
                    dispcache,v_sector,overlap/CD_FRAMEWORDS,smilie,heartbeat);

          if ( aheadposition>=0 && aheadposition<graph &&
	       !(function == PARANOIA_CB_FINISHED) )
            buffer[aheadposition+19]='>';
        }

        if(isatty(STDERR_FILENO))
            fprintf(stderr, "%s", buffer);

        if (logfile != NULL && function == PARANOIA_CB_FINISHED) {
          fprintf(logfile, "%s", buffer+1);
          fprintf(logfile,"\n\n");
          fflush(logfile);
        }
      }
    }
  }

  /* clear the indicator for next batch */
  if(function == PARANOIA_CB_FINISHED)
    memset(dispcache,' ',graph);
}
#endif /* !TRACE_PARANOIA */

static const char optstring[] = "aBcCd:eEfFg:k:hi:l:L:Am:n:o:O:pqQrRsS:Tt:VvwWx:XYZz::";

static const struct option options [] = {
        {"abort-on-skip",             no_argument,       NULL, 'X'},
	{"analyze-drive",             no_argument,       NULL, 'A'},
        {"batch",                     no_argument,       NULL, 'B'},
        {"disable-extra-paranoia",    no_argument,       NULL, 'Y'},
        {"disable-fragmentation",     no_argument,       NULL, 'F'},
        {"disable-paranoia",          no_argument,       NULL, 'Z'},
        {"force-cdrom-big-endian",    no_argument,       NULL, 'C'},
        {"force-cdrom-device",        required_argument, NULL, 'd'},
        {"force-cdrom-little-endian", no_argument,       NULL, 'c'},
        {"force-default-sectors",     required_argument, NULL, 'n'},
        {"force-cooked-device",       required_argument, NULL, 'k'},
        {"force-generic-device",      required_argument, NULL, 'g'},
        {"force-read-speed",          required_argument, NULL, 'S'},
        {"force-search-overlap",      required_argument, NULL, 'o'},
	{"force-overread",            no_argument,       NULL, 'E'},
        {"help",                      no_argument,       NULL, 'h'},
        {"log-summary",               required_argument, NULL, 'l'},
        {"log-debug",                 required_argument, NULL, 'L'},
        {"mmc-timeout",               required_argument, NULL, 'm'},
        {"never-skip",                optional_argument, NULL, 'z'},
        {"output-aifc",               no_argument,       NULL, 'a'},
        {"output-aiff",               no_argument,       NULL, 'f'},
        {"output-raw",                no_argument,       NULL, 'p'},
        {"output-raw-big-endian",     no_argument,       NULL, 'R'},
        {"output-raw-little-endian",  no_argument,       NULL, 'r'},
        {"output-wav",                no_argument,       NULL, 'w'},
        {"query",                     no_argument,       NULL, 'Q'},
        {"quiet",                     no_argument,       NULL, 'q'},
        {"sample-offset",             required_argument, NULL, 'O'},
        {"stderr-progress",           no_argument,       NULL, 'e'},
        {"test-mode",                 required_argument, NULL, 'x'},
        {"toc-bias",                  no_argument,       NULL, 'T'},
        {"toc-offset",                required_argument, NULL, 't'},
        {"verbose",                   no_argument,       NULL, 'v'},
        {"version",                   no_argument,       NULL, 'V'},

        {NULL,0,NULL,0}
};

static cdrom_drive_t    *d        = NULL;
static cdrom_paranoia_t *p        = NULL;
static char             *span     = NULL;
static char *force_cdrom_device   = NULL;

#define free_and_null(p) \
  free(p);               \
  p=NULL;

#if !defined(HAVE_GETTIMEOFDAY) && defined(HAVE_SYS_TIME_H) && defined(HAVE_SYS_TIMEB_H)
static void
gettimeofday(struct timeval* tv, void* timezone)
{
  struct timeb timebuffer;
  ftime( &timebuffer );
  tv->tv_sec=timebuffer.time;
  tv->tv_usec=1000*timebuffer.millitm;
}
#endif

/* This is run automatically before leaving the program.
   Free allocated resources.
*/
static void
cleanup (void)
{
  if (p) paranoia_free(p);
  if (d) cdda_close(d);
  free_and_null(force_cdrom_device);
  free_and_null(span);
  if(logfile_open) {
      fclose(logfile);
      logfile = NULL;
    }
  if(reportfile_open) {
      fclose(reportfile);
      reportfile = NULL;
    }
}

/* Returns true if we have an integer argument.
   If so, pi_arg is set.
   If no argument or integer argument found, we give an error
   message and return false.
*/
static bool
get_int_arg(char c, long int *pi_arg)
{
  long int i_arg;
  char *p_end;
  if (!optarg) {
    /* This shouldn't happen, but we'll check anyway. */
    fprintf(stderr,
            "An (integer) argument for option -%c was expected "
            " but not found. Option ignored\n", c);
    return false;
  }
  errno = 0;
  i_arg = strtol(optarg, &p_end, 10);
  if ( (LONG_MIN == i_arg || LONG_MAX == i_arg) && (0 != errno) ) {
    fprintf(stderr,
            "Value '%s' for option -%c out of range. Value %ld "
            "used instead.\n", optarg, c, i_arg);
    *pi_arg = i_arg;
    return false;
  } else if (*p_end) {
    fprintf(stderr,
            "Can't convert '%s' for option -%c completely into an integer. "
            "Option ignored.\n", optarg, c);
    return false;
  } else {
    *pi_arg = i_arg;
    return true;
  }
}

int
main(int argc,char *argv[])
{
  int   toc_bias             =  0;
  int   force_cdrom_endian   = -1;
  int   output_type          =  1; /* 0=raw, 1=wav, 2=aifc */
  int   output_endian        =  0; /* -1=host, 0=little, 1=big */
  int   query_only           =  0;
  int   batch                =  0;
  int   run_cache_test       =  0;
  long int force_cdrom_overlap  = -1;
  long int force_cdrom_sectors  = -1;
  long int force_cdrom_speed    =  0;
  long int force_overread       =  0;
  long int sample_offset        =  0;
  long int test_flags           =  0;
  long int toc_offset           =  0;
  long int max_retries          = 20;

  char *logfile_name=NULL;
  char *reportfile_name=NULL;

  /* full paranoia, but allow skipping */
  int paranoia_mode=PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP;

  int out;

  int c,long_option_index;

  atexit(cleanup);

  while((c=getopt_long(argc,argv,optstring,options,&long_option_index))!=EOF){
    switch(c){
    case 'a':
      output_type=2;
      output_endian=1;
      break;
    case 'B':
      batch=1;
      break;
    case 'c':
      force_cdrom_endian=0;
      break;
    case 'C':
      force_cdrom_endian=1;
      break;
    case 'e':
      callscript=1;
      fprintf(stderr,
              "Sending all callback output to stderr for wrapper script\n");
      break;
    case 'f':
      output_type=3;
      output_endian=1;
      break;
    case 'F':
      paranoia_mode&=~(PARANOIA_MODE_FRAGMENT);
      break;
    case 'g':
    case 'k':
    case 'd':
      if (force_cdrom_device) {
        fprintf(stderr,
                "Multiple cdrom devices given. Previous device %s ignored\n",
                force_cdrom_device);
        free(force_cdrom_device);
      }
      force_cdrom_device=strdup(optarg);
      break;
    case 'h':
      usage(stdout);
      exit(0);
    case 'l':
      if(logfile_name)free(logfile_name);
      logfile_name=NULL;
      if(optarg)
	logfile_name=strdup(optarg);
      logfile_open=1;
      break;
    case 'L':
      if(reportfile_name)free(reportfile_name);
      reportfile_name=NULL;
      if(optarg)
	reportfile_name=strdup(optarg);
      reportfile_open=1;
      break;
    case 'm':
      {
        long int mmc_timeout_sec;
        if (get_int_arg(c, &mmc_timeout_sec)) {
          mmc_timeout_ms = 1000*mmc_timeout_sec;
        }
      }
      break;
    case 'n':
      get_int_arg(c, &force_cdrom_sectors);
      break;
    case 'o':
      get_int_arg(c, &force_cdrom_overlap);
      break;
    case 'O':
      get_int_arg(c, &sample_offset);
      break;
    case 'p':
      output_type=0;
      output_endian=-1;
      break;
    case 'r':
      output_type=0;
      output_endian=0;
      break;
    case 'q':
      verbose=CDDA_MESSAGE_FORGETIT;
      quiet=1;
      break;
    case 'Q':
      query_only=1;
      break;
    case 'R':
      output_type=0;
      output_endian=1;
      break;
    case 'S':
      get_int_arg(c, &force_cdrom_speed);
      break;
    case 't':
      get_int_arg(c, &toc_offset);
      break;
    case 'T':
      toc_bias=-1;
      break;
    case 'v':
      verbose=CDDA_MESSAGE_PRINTIT;
      quiet=0;
      break;
    case 'V':
      fprintf(stderr,PARANOIA_VERSION);
      fprintf(stderr,"\n");
      exit(0);
      break;
    case 'w':
      output_type=1;
      output_endian=0;
      break;
    case 'W':
      paranoia_mode&=~PARANOIA_MODE_REPAIR;
      break;
    case 'x':
      get_int_arg(c, &test_flags);
      break;
    case 'X':
      /*paranoia_mode&=~(PARANOIA_MODE_SCRATCH|PARANOIA_MODE_REPAIR);*/
      abort_on_skip=1;
      break;
    case 'Y':
      paranoia_mode|=PARANOIA_MODE_OVERLAP; /* cdda2wav style overlap
                                                check only */
      paranoia_mode&=~PARANOIA_MODE_VERIFY;
      break;
    case 'Z':
      paranoia_mode=PARANOIA_MODE_DISABLE;
      break;
    case 'A':
      run_cache_test=1;
      query_only=1;
      reportfile_open=1;
      verbose=CDDA_MESSAGE_PRINTIT;
      break;
    case 'z':
      if (optarg) {
        get_int_arg(c, &max_retries);
        paranoia_mode&=~PARANOIA_MODE_NEVERSKIP;
      } else {
        paranoia_mode|=PARANOIA_MODE_NEVERSKIP;
      }
      break;
    case 'E':
      force_overread=1;
      break;
    default:
      usage(stderr);
      exit(1);
    }
  }

  if(logfile_open){
    if(logfile_name==NULL)
      logfile_name=strdup("cdparanoia.log");
    if(!strcmp(logfile_name,"-")){
      logfile=stdout;
      logfile_open=0;
    }else{
      logfile=fopen(logfile_name,"w");
      if(logfile==NULL){
	report("Cannot open log summary file %s: %s",logfile_name,
	       strerror(errno));
	exit(1);
      }
    }
  }
  if(reportfile_open){
    if(reportfile_name==NULL)
      reportfile_name=strdup("cdparanoia.log");
    if(!strcmp(reportfile_name,"-")){
      reportfile=stdout;
      reportfile_open=0;
    }else{
      if(logfile_name && !strcmp(reportfile_name,logfile_name)){
	reportfile=logfile;
	reportfile_open=0;
      }else{
	reportfile=fopen(reportfile_name,"w");
	if(reportfile==NULL){
	  report("Cannot open debug log file %s: %s",reportfile_name,
		 strerror(errno));
	  exit(1);
	}
      }
    }
  }

  if(logfile){
    /* log command line and version */
    int i;
    for (i = 0; i < argc; i++)
      fprintf(logfile,"%s ",argv[i]);
    fprintf(logfile,"\n");

    if(reportfile!=logfile){
      fprintf(logfile,VERSION);
      fprintf(logfile,"\n");
      fprintf(logfile,"Using cdda library version: %s\n",cdda_version());
      fprintf(logfile,"Using paranoia library version: %s\n",paranoia_version());
    }
    fflush(logfile);
  }

  if(reportfile && reportfile!=logfile){
    /* log command line */
    int i;
    for (i = 0; i < argc; i++)
      fprintf(reportfile,"%s ",argv[i]);
    fprintf(reportfile,"\n");
    fflush(reportfile);
  }

  if(optind>=argc && !query_only){
    if(batch)
      span=NULL;
    else{
      /* D'oh.  No span. Fetch me a brain, Igor. */
      usage(stderr);
      exit(1);
    }
  }else
    if (argv[optind]) span=strdup(argv[optind]);

  report(PARANOIA_VERSION);
  if(verbose){
    report("Using cdda library version: %s",cdda_version());
    report("Using paranoia library version: %s",paranoia_version());
  }

  /* Query the cdrom/disc; we may need to override some settings */

  if(force_cdrom_device)
    d=cdda_identify(force_cdrom_device,verbose,NULL);
  else {
    driver_id_t driver_id;
    char **ppsz_cd_drives = cdio_get_devices_with_cap_ret(NULL,
                                                          CDIO_FS_AUDIO,
                                                          false,
                                                          &driver_id);
    if (ppsz_cd_drives && *ppsz_cd_drives) {
      d=cdda_identify(*ppsz_cd_drives,verbose, NULL);
    } else {
      report("\nUnable find or access a CD-ROM drive with an audio CD"
             " in it.");
      report("\nYou might try specifying the drive, especially if it has"
             " mixed-mode (and non-audio) format tracks");
      exit(1);
    }

    cdio_free_device_list(ppsz_cd_drives);
  }

  if(!d){
    if(!verbose)
      report("\nUnable to open cdrom drive; -v might give more information.");
    exit(1);
  }

  if(verbose)
    cdda_verbose_set(d,CDDA_MESSAGE_PRINTIT,CDDA_MESSAGE_PRINTIT);
  else
    cdda_verbose_set(d,CDDA_MESSAGE_PRINTIT,CDDA_MESSAGE_FORGETIT);

  /* possibly force hand on endianness of drive, sector request size */
  if(force_cdrom_endian!=-1){
    d->bigendianp=force_cdrom_endian;
    switch(force_cdrom_endian){
    case 0:
      report("Forcing CDROM sense to little-endian; ignoring preset and autosense");
      break;
    case 1:
      report("Forcing CDROM sense to big-endian; ignoring preset and autosense");
      break;
    }
  }
  if (force_cdrom_sectors!=-1) {
    if(force_cdrom_sectors<0 || force_cdrom_sectors>100){
      report("Default sector read size must be 1<= n <= 100\n");
      cdda_close(d);
      d=NULL;
      exit(1);
    }
    report("Forcing default to read %ld sectors; "
	   "ignoring preset and autosense",force_cdrom_sectors);
    d->nsectors=force_cdrom_sectors;
  }
  if (force_cdrom_overlap!=-1) {
    if (force_cdrom_overlap<0 || force_cdrom_overlap>CDIO_CD_FRAMES_PER_SEC) {
      report("Search overlap sectors must be 0<= n <=75\n");
      cdda_close(d);
      d=NULL;
      if(logfile && logfile != stdout)
        fclose(logfile);
      exit(1);
    }
    report("Forcing search overlap to %ld sectors; "
	   "ignoring autosense",force_cdrom_overlap);
  }

  switch( cdda_open(d) ) {
  case -2:case -3:case -4:case -5:
    report("\nUnable to open disc.  Is there an audio CD in the drive?");
    exit(1);
  case -6:
    report("\nCdparanoia could not find a way to read audio from this drive.");
    exit(1);
  case 0:
    break;
  default:
    report("\nUnable to open disc.");
    exit(1);
  }

  d->i_test_flags = test_flags;

  if (force_cdrom_speed == 0) force_cdrom_speed = -1;

  if (force_cdrom_speed != -1) {
    report("\nAttempting to set speed to %ldx... ", force_cdrom_speed);
  } else {
    if (verbose)
      report("\nAttempting to set cdrom to full speed... ");
  }

  if (cdda_speed_set(d, force_cdrom_speed)) {
    if (verbose || force_cdrom_speed != -1)
      report("\tCDROM speed set FAILED. Continuing anyway...");
  } else {
    if (verbose)
      report("\tdrive returned OK.");
  }

  if(run_cache_test){
    int warn=analyze_cache(d, stderr, reportfile, force_cdrom_speed);

    if(warn==0){
      reportC("\nDrive tests OK with Paranoia.\n\n");
      return 0;
    }

    if(warn==1)
      reportC("\nWARNING! PARANOIA MAY NOT BE TRUSTWORTHY WITH THIS DRIVE!\n"
	      "\nThe Paranoia library may not model this CDROM drive's cache"
	      "\ncorrectly according to this analysis run. Analysis is not"
	      "\nalways accurate (it can be fooled by machine load or random"
	      "\nkernel latencies), but if a failed result happens more often"
	      "\nthan one time in twenty on an unloaded machine, please mail"
	      "\nthe %s file produced by this failed analysis to"
	      "\nparanoia-dev@xiph.org to assist developers in extending"
	      "\nParanoia to handle this CDROM properly.\n\n",reportfile_name);
    return 1;
  }


  /* Dump the TOC */
  if (query_only || verbose ) display_toc(d);

  if (query_only) exit(0);

  /* bias the disc.  A hack.  Of course. this is never the default. */
  /*
     Some CD-ROM/CD-R drives will add an offset to the position on
     reading audio data. This is usually around 500-700 audio samples
     (ca. 1/75 second) on reading. So when this program queries a
     specific sector, it might not receive exactly that sector, but
     shifted by some amount.

     Note that if ripping includes the end of the CD, this will this
     cause this program to attempt to read partial sectors before or
     past the known user data area of the disc, probably causing read
     errors on most drives and possibly even hard lockups on some
     buggy hardware.

     [Note to libcdio driver hackers: make sure all CD-drivers don't
     try to read outside of the stated disc boundaries.]
  */
  if(sample_offset){
    toc_offset+=sample_offset/588;
    sample_offset%=588;
    if(sample_offset<0){
      sample_offset+=588;
      toc_offset--;
    }
  }

  if (toc_bias) {
    toc_offset = -cdda_track_firstsector(d,1);
  }

  {
    int i;
    for( i=0; i < d->tracks+1; i++ )
      d->disc_toc[i].dwStartSector+=toc_offset;
  }

  if (d->nsectors==1) {
    report("WARNING: The autosensed/selected sectors per read value is\n"
           "         one sector, making it very unlikely Paranoia can \n"
           "         work.\n\n"
           "         Attempting to continue...\n\n");
  }

  /* parse the span, set up begin and end sectors */

  {
    long i_first_lsn;
    long i_last_lsn;
    long batch_first;
    long batch_last;
    int batch_track;

    if (span) {
      /* look for the hyphen */
      char *span2=strchr(span,'-');
      if(strrchr(span,'-')!=span2){
        report("Error parsing span argument");
        exit(1);
      }

      if (span2!=NULL) {
        *span2='\0';
        span2++;
      }

      i_first_lsn=parse_offset(d, span, -1);

      if(i_first_lsn==-1)
        i_last_lsn=parse_offset(d, span2, cdda_disc_firstsector(d));

      else
        i_last_lsn=parse_offset(d, span2, i_first_lsn);

      if (i_first_lsn == -1) {
        if (i_last_lsn == -1) {
          report("Error parsing span argument");
          exit(1);
        } else {
          i_first_lsn=cdda_disc_firstsector(d);
        }
      } else {
        if (i_last_lsn==-1) {
          if (span2) { /* There was a hyphen */
            i_last_lsn=cdda_disc_lastsector(d);
          } else {
            i_last_lsn=
              cdda_track_lastsector(d,cdda_sector_gettrack(d, i_first_lsn));
          }
        }
      }
    } else {
      i_first_lsn = cdda_disc_firstsector(d);
      i_last_lsn  = cdda_disc_lastsector(d);
    }

    {
      int track1 = cdda_sector_gettrack(d, i_first_lsn);

      int track2 = cdda_sector_gettrack(d, i_last_lsn);
      long off1  = i_first_lsn - cdda_track_firstsector(d, track1);
      long off2  = i_last_lsn  - cdda_track_firstsector(d, track2);
      int i;

      for( i=track1; i<=track2; i++ ) {
        if(i != 0 && !cdda_track_audiop(d,i)){
	  report("Selected span contains non audio track at track %02d.  Aborting.\n\n", i);
          exit(1);
          if (i == 0)
            i = cdio_get_first_track_num(d->p_cdio) - 1;
        }
      }

      report("Ripping from sector %7ld (track %2d [%d:%02d.%02d])\n"
	     "\t  to sector %7ld (track %2d [%d:%02d.%02d])\n",
	     i_first_lsn,
	     track1,
	     (int) (off1/(CDIO_CD_FRAMES_PER_MIN)),
	     (int) ((off1/CDIO_CD_FRAMES_PER_SEC) % CDIO_CD_SECS_PER_MIN),
	     (int)(off1 % CDIO_CD_FRAMES_PER_SEC),
	     i_last_lsn,
	     track2,
	     (int) (off2/(CDIO_CD_FRAMES_PER_MIN)),
	     (int) ((off2/CDIO_CD_FRAMES_PER_SEC) % CDIO_CD_SECS_PER_MIN),
	     (int)(off2 % CDIO_CD_FRAMES_PER_SEC));

    }

    if (toc_offset && !force_overread) {
	d->disc_toc[d->tracks].dwStartSector -= toc_offset;
	if (i_last_lsn > cdda_track_lastsector(d, d->tracks))
		i_last_lsn -= toc_offset;
    }
    {
      long cursor;
      int16_t offset_buffer[1176];
      int offset_buffer_used=0;
      int offset_skip=sample_offset*4;
      off_t sectorlen;

#if defined(HAVE_GETUID) && (defined(HAVE_SETEUID) || defined(HAVE_SETEGID))
      int dummy __attribute__((unused));
#endif
      p=paranoia_init(d);
      paranoia_modeset(p,paranoia_mode);
      if(force_cdrom_overlap!=-1)paranoia_overlapset(p,force_cdrom_overlap);

      if(verbose) {
        cdda_verbose_set(d,CDDA_MESSAGE_LOGIT,CDDA_MESSAGE_LOGIT);
        cdio_loglevel_default = CDIO_LOG_INFO;
      } else
        cdda_verbose_set(d,CDDA_MESSAGE_FORGETIT,CDDA_MESSAGE_FORGETIT);

      paranoia_seek(p,cursor=i_first_lsn,SEEK_SET);

      /* this is probably a good idea in general */
#if defined(HAVE_GETUID) && defined(HAVE_SETEUID)
      dummy = seteuid(getuid());
#endif
#if defined(HAVE_GETGID) && defined(HAVE_SETEGID)
      dummy = setegid(getgid());
#endif

      /* we'll need to be able to read one sector past user data if we
         have a sample offset in order to pick up the last bytes.  We
         need to set the disc length forward here so that the libs are
         willing to read past, assuming that works on the hardware, of
         course */
      if(sample_offset && force_overread)
        d->disc_toc[d->tracks].dwStartSector++;

      while(cursor<=i_last_lsn){
        char outfile_name[PATH_MAX];
        if ( batch ){
          batch_first = cursor;
          batch_track = cdda_sector_gettrack(d,cursor);
          batch_last  = cdda_track_lastsector(d, batch_track);
          if (batch_last>i_last_lsn) batch_last=i_last_lsn;
        } else {
          batch_first = i_first_lsn;
          batch_last  = i_last_lsn;
          batch_track = -1;
        }

        callbegin=batch_first;
        callend=batch_last;

        /* argv[optind] is the span, argv[optind+1] (if exists) is outfile */

        if (optind+1<argc) {
          if (!strcmp(argv[optind+1],"-") ){
            out = dup(fileno(stdout));
            if(out==-1){
              report("Cannot dupplicate stdout: %s",
                     strerror(errno));
              exit(1);
            }
            if(batch)
              report("Are you sure you wanted 'batch' "
                     "(-B) output with stdout?");
            report("outputting to stdout\n");
            if(logfile){
              fprintf(logfile,"outputting to stdout\n");
              fflush(logfile);
            }
            outfile_name[0]='\0';
          } else {
            char dirname[PATH_MAX];
            char *basename=split_base_dir(argv[optind+1], dirname,
					  PATH_MAX);

	    if (NULL == basename) {
	      report("Output filename too long");
	      exit(1);
	    }

            if(batch) {
	      if (strlen(argv[optind+1]) - 10 > PATH_MAX) {
		report("Output filename too long");
		exit(1);
	      }
              snprintf(outfile_name, PATH_MAX,
		       " %strack%02d.%s", dirname,
                       batch_track, basename);
            } else
              snprintf(outfile_name, PATH_MAX, "%s%s", dirname, basename);

            if(basename[0]=='\0'){
              switch (output_type) {
              case 0: /* raw */
                strncat(outfile_name, "cdda.raw", sizeof("cdda.raw"));
                break;
              case 1:
                strncat(outfile_name, "cdda.wav", sizeof("cdda.wav"));
                break;
              case 2:
                strncat(outfile_name, "cdda.aifc", sizeof("cdda.aifc"));
                break;
              case 3:
                strncat(outfile_name, "cdda.aiff", sizeof("cdda.aiff"));
                break;
              }
            }

            out=open(outfile_name,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,0666);
            if(out==-1){
              report("Cannot open specified output file %s: %s",
                      outfile_name, strerror(errno));
              exit(1);
            }
            report("outputting to %s\n", outfile_name);
            if(logfile){
              fprintf(logfile,"outputting to %s\n",outfile_name);
              fflush(logfile);
            }
          }
        } else {
          /* default */
          if (batch)
            sprintf(outfile_name,"track%02d.", batch_track);
          else
            outfile_name[0]='\0';

          switch(output_type){
          case 0: /* raw */
            strncat(outfile_name, "cdda.raw", sizeof("cdda.raw"));
            break;
          case 1:
            strncat(outfile_name, "cdda.wav", sizeof("cdda.wav"));
            break;
          case 2:
            strncat(outfile_name, "cdda.aifc", sizeof("cdda.aifc"));
            break;
          case 3:
            strncat(outfile_name, "cdda.aiff", sizeof("cdda.aiff"));
            break;
          }

          out = open(outfile_name, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, 0666);
          if(out==-1){
            report("Cannot open default output file %s: %s", outfile_name,
                    strerror(errno));
            exit(1);
          }
          report("outputting to %s\n", outfile_name);
          if(logfile){
            fprintf(logfile,"outputting to %s\n",outfile_name);
            fflush(logfile);
          }

        }

	sectorlen = batch_last - batch_first + 1;
	if (cdda_sector_gettrack(d, cursor) == d->tracks &&
		toc_offset > 0 && !force_overread){
		sectorlen += toc_offset;
	}
        switch(output_type) {
        case 0: /* raw */
          break;
        case 1: /* wav */
	  WriteWav(out, sectorlen * CD_FRAMESIZE_RAW);
          break;
        case 2: /* aifc */
	  WriteAifc(out, sectorlen * CD_FRAMESIZE_RAW);
          break;
        case 3: /* aiff */
	  WriteAiff(out, sectorlen * CD_FRAMESIZE_RAW);
          break;
        }

        /* Off we go! */

        if(offset_buffer_used){
          /* partial sector from previous batch read */
          cursor++;
          if (buffering_write(out,
                              ((char *)offset_buffer)+offset_buffer_used,
                              CDIO_CD_FRAMESIZE_RAW-offset_buffer_used)){
            report("Error writing output: %s", strerror(errno));
            exit(1);
          }
        }

        skipped_flag=0;
        while(cursor<=batch_last){
          /* read a sector */
          int16_t *readbuf=paranoia_read_limited(p, callback, max_retries);
          char *err=cdda_errors(d);
          char *mes=cdda_messages(d);

          if(mes || err)
            fprintf(stderr,"\r                               "
                    "                                           \r%s%s\n",
                    mes?mes:"",err?err:"");

          if (err) free(err);
          if (mes) free(mes);
          if( readbuf==NULL) {
	    if(errno==EBADF || errno==ENOMEDIUM){
	      report("\nparanoia_read: CDROM drive unavailable, bailing.\n");
	      exit(1);
	    }
            skipped_flag=1;
            report("\nparanoia_read: Unrecoverable error, bailing.\n");
            break;
          }
          if(skipped_flag && abort_on_skip){
            cursor=batch_last+1;
            break;
          }

          skipped_flag=0;
          cursor++;

          if (output_endian!=bigendianp()) {
            int i;
            for (i=0; i<CDIO_CD_FRAMESIZE_RAW/2; i++)
              readbuf[i]=UINT16_SWAP_LE_BE_C(readbuf[i]);
          }

          callback(cursor*(CD_FRAMEWORDS)-1, PARANOIA_CB_WROTE);

          if (buffering_write(out,((char *)readbuf)+offset_skip,
                             CDIO_CD_FRAMESIZE_RAW-offset_skip)){
            report("Error writing output: %s", strerror(errno));
            exit(1);
          }
          offset_skip=0;

          if (output_endian != bigendianp()){
            int i;
            for (i=0; i<CDIO_CD_FRAMESIZE_RAW/2; i++)
              readbuf[i] = UINT16_SWAP_LE_BE_C(readbuf[i]);
          }

          /* One last bit of silliness to deal with sample offsets */
          if(sample_offset && cursor>batch_last){
	    if (cdda_sector_gettrack(d, batch_last) < d->tracks || force_overread) {
	      int i;

	      /* Need to flush the buffer when overreading into the leadout */
	      if (cdda_sector_gettrack(d, batch_last) == d->tracks)
		paranoia_seek(p, cursor, SEEK_SET);

	      /* read a sector and output the partial offset.  Save the
		 rest for the next batch iteration */
	      readbuf=paranoia_read_limited(p,callback,max_retries);
	      err=cdda_errors(d);mes=cdda_messages(d);

	      if(mes || err)
		fprintf(stderr,"\r                               "
			"                                           \r%s%s\n",
			mes?mes:"",err?err:"");

	      if (err)
		free(err);
	      if (mes)
		free(mes);
	      if(readbuf==NULL){
		skipped_flag=1;
		report("\nparanoia_read: Unrecoverable error reading through "
		       "sample_offset shift\n\tat end of track, bailing.\n");
		break;
	      }
	      if (skipped_flag && abort_on_skip) break;
	      skipped_flag=0;
	      /* do not move the cursor */

	      if(output_endian!=bigendianp())
		for(i=0;i<CD_FRAMESIZE_RAW/2;i++)
		  offset_buffer[i]=UINT16_SWAP_LE_BE_C(readbuf[i]);
	      else
		memcpy(offset_buffer,readbuf,CD_FRAMESIZE_RAW);
	      offset_buffer_used=sample_offset*4;
	      callback(cursor* (CD_FRAMEWORDS), PARANOIA_CB_WROTE);
	    } else {
	      memset(offset_buffer, 0, sizeof(offset_buffer));
	      offset_buffer_used = sample_offset * 4;
	    }

	    if(buffering_write(out,(char *)offset_buffer,
			       offset_buffer_used)){
	      report("Error writing output: %s", strerror(errno));
	      exit(1);
	    }
	  }
        }

	/* Write sectors of silent audio to compensate for
	   missing samples that would be in the leadout */
	if (cdda_sector_gettrack(d, batch_last) == d->tracks &&
		toc_offset > 0 && !force_overread)
	{
		char *silence;
		size_t missing_sector_bytes = CD_FRAMESIZE_RAW * toc_offset;

		silence = calloc(toc_offset, CD_FRAMESIZE_RAW);
		if (!silence || buffering_write(out, silence, missing_sector_bytes)) {
		      report("Error writing output: %s", strerror(errno));
		      exit(1);
		}
		free(silence);
	}

        callback(cursor* (CDIO_CD_FRAMESIZE_RAW/2)-1,
		 PARANOIA_CB_FINISHED);
        buffering_close(out);
        if(skipped_flag){
          /* remove the file */
          report("\nRemoving aborted file: %s", outfile_name);
          unlink(outfile_name);
          /* make the cursor correct if we have another track */
          if(batch_track!=-1){
            batch_track++;
            cursor=cdda_track_firstsector(d,batch_track);
            paranoia_seek(p,cursor, SEEK_SET);
            offset_skip=sample_offset*4;
            offset_buffer_used=0;
          }
        }
        report("\n");
      }

      paranoia_free(p);
      p=NULL;
    }
  }

  report("Done.\n\n");

  return 0;
}
