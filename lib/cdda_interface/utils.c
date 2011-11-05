/*
  Copyright (C) 2004, 2008, 2010, 2011 Rocky Bernstein <rocky@gnu.org>
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

#include "common_interface.h"
#include "utils.h"
void 
cderror(cdrom_drive_t *d,const char *s)
{
  ssize_t bytes_ret;
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
    default: ;
    }
  }
}

void 
cdmessage(cdrom_drive_t *d, const char *s)
{
  ssize_t bytes_ret;
  if(s && d){
    switch(d->messagedest){
    case CDDA_MESSAGE_PRINTIT:
      bytes_ret = write(STDERR_FILENO, s, strlen(s));
      break;
    case CDDA_MESSAGE_LOGIT:
      d->messagebuf=catstring(d->messagebuf,s);
      break;
    case CDDA_MESSAGE_FORGETIT:
    default: ;
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
    ssize_t bytes_ret;
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
    default: ;
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
  if(!f)
    buffer=(char *)s;
  else
    if(!s)
      buffer=(char *)f;
    else{
      const unsigned int i_buffer=strlen(f)+strlen(s)+10;
      buffer=malloc(i_buffer);
      sprintf(buffer,f,s);
      strncat(buffer,"\n", i_buffer);
      malloced=1;
    }

  if(buffer) {
    ssize_t bytes_ret;
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
    default: ;
    }
  }
  if(malloced)free(buffer);
}

char *
catstring(char *buff, const char *s) {
  if (s) {
    const unsigned int add_len = strlen(s) + 9;
    if(buff) {
      buff = realloc(buff, strlen(buff) + add_len);
    } else {
      buff=calloc(add_len, 1);
    }
    strncat(buff, s, add_len);
  }
  return(buff);
}
