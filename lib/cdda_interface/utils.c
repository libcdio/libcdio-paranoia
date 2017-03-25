/*
  Copyright (C) 2004, 2008, 2010-2011, 2017 Rocky Bernstein <rocky@gnu.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
# define __CDIO_CONFIG_H__ 1
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include "common_interface.h"
#include "utils.h"
void
cderror(cdrom_drive_t *d,const char *s)
{
  ssize_t bytes_ret __attribute__((unused));
  if(s && d){
    switch(d->errordest){
    case CDDA_MESSAGE_PRINTIT:
      bytes_ret = write(STDERR_FILENO, s, strlen(s));
      if (strlen(s) != bytes_ret)

      break;
    case CDDA_MESSAGE_LOGIT:
      d->errorbuf=catstring(d->errorbuf,s);
      break;
    case CDDA_MESSAGE_FORGETIT:
    default:
      break;
    }
  }
}

void
cdmessage(cdrom_drive_t *d, const char *s)
{
  ssize_t bytes_ret __attribute__((unused));
  if(s && d){
    switch(d->messagedest){
    case CDDA_MESSAGE_PRINTIT:
      bytes_ret = write(STDERR_FILENO, s, strlen(s));
      break;
    case CDDA_MESSAGE_LOGIT:
      d->messagebuf=catstring(d->messagebuf,s);
      break;
    case CDDA_MESSAGE_FORGETIT:
    default:
      break;
    }
  }
}

void
idperror(int messagedest,char **messages,const char *f,
	 const char *s)
{

  char *buffer;
  int malloced=0;
  if(!f)
    buffer=(char *)s;
  else
    if(!s)
      buffer=(char *)f;
    else{
      buffer=malloc(strlen(f)+strlen(s)+9);
      sprintf(buffer,f,s);
      malloced=1;
    }

  if(buffer){
    ssize_t bytes_ret __attribute__((unused));
    switch(messagedest){
    case CDDA_MESSAGE_PRINTIT:
      bytes_ret = write(STDERR_FILENO,buffer,strlen(buffer));
      if(errno){
	bytes_ret = write(STDERR_FILENO,": ",2);
	bytes_ret = write(STDERR_FILENO,strerror(errno),strlen(strerror(errno)));
	bytes_ret = write(STDERR_FILENO,"\n",1);
      }
      break;
    case CDDA_MESSAGE_LOGIT:
      if(messages){
	*messages=catstring(*messages,buffer);
	if(errno){
	  *messages=catstring(*messages,": ");
	  *messages=catstring(*messages,strerror(errno));
	  *messages=catstring(*messages,"\n");
	}
      }
      break;
    case CDDA_MESSAGE_FORGETIT:
    default:
      break;
    }
  }
  if(malloced)free(buffer);
}

void
idmessage(int messagedest,char **messages,const char *f,
	  const char *s)
{
  char *buffer;
  int malloced=0;
  ssize_t bytes_ret __attribute__((unused));
  if(!f)
    buffer=(char *)s;
  else
    if(!s)
      buffer=(char *)f;
    else{
      const unsigned int i_buffer=strlen(f)+strlen(s)+2;
      buffer=malloc(i_buffer);
      sprintf(buffer,f,s);
      strncat(buffer,"\n",1);
      malloced=1;
    }

  if(buffer) {
    switch(messagedest){
    case CDDA_MESSAGE_PRINTIT:
      bytes_ret = write(STDERR_FILENO,buffer,strlen(buffer));
      if(!malloced)
	 bytes_ret = write(STDERR_FILENO,"\n",1);
      break;
    case CDDA_MESSAGE_LOGIT:
      if(messages){
	*messages=catstring(*messages,buffer);
	if(!malloced)*messages=catstring(*messages,"\n");
	}
      break;
    case CDDA_MESSAGE_FORGETIT:
    default:
      break;
    }
  }
  if(malloced)free(buffer);
}

char *
catstring(char *buff, const char *s) {
  if (s) {
    const unsigned int add_len = strlen(s) + 1;
    if(buff) {
      buff = realloc(buff, strlen(buff) + add_len);
    } else {
      buff=calloc(add_len, 1);
    }
    strncat(buff, s, add_len - 1);
  }
  return(buff);
}

int
gettime(struct timespec *ts) {
  int ret = -1;
  if (!ts) return ret;

#if defined(HAVE_CLOCK_GETTIME)
  /* Use clock_gettime if available, preferably using the monotonic clock.
   */
  static clockid_t clock = (clockid_t)-1;
  if ((int)clock == -1) clock = (clock_gettime(CLOCK_MONOTONIC, ts) < 0 ? CLOCK_REALTIME : CLOCK_MONOTONIC);
  ret = clock_gettime(clock, ts);
#elif defined(WIN32)
  /* clock() returns wall time (not CPU time) on Windows, so we can use it here.
   */
  clock_t time = clock();
  if ((int)time != -1) {
    ts->tv_sec  = time/CLOCKS_PER_SEC;
    ts->tv_nsec = time%CLOCKS_PER_SEC*(1000000000/CLOCKS_PER_SEC);
    ret = 0;
  }
#else
  /* In other cases use gettimeofday.
   */
  struct timeval tv;
  ret = gettimeofday(&tv, NULL);
  if (ret == 0) {
    ts->tv_sec  = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec*1000;
  }
#endif

  return ret;
}
