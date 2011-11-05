/*
  $Id: utils.h,v 1.8 2008/04/16 17:00:41 karl Exp $

  Copyright (C) 2004, 2005, 2008 Rocky Bernstein <rocky@gnu.org>
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

#include <cdio/bytesex.h>
#include <stdio.h>

/* I wonder how many alignment issues this is gonna trip in the
   future...  it shouldn't trip any...  I guess we'll find out :) */

static inline int 
bigendianp(void)
{
  int test=1;
  char *hack=(char *)(&test);
  if(hack[0])return(0);
  return(1);
}

extern char *catstring(char *buff, const char *s);


/*#if BYTE_ORDER == LITTLE_ENDIAN*/

#ifndef WORDS_BIGENDIAN

static inline int16_t be16_to_cpu(int16_t x){
  return(UINT16_SWAP_LE_BE_C(x));
}

static inline int16_t le16_to_cpu(int16_t x){
  return(x);
}

#else

static inline int16_t be16_to_cpu(int16_t x){
  return(x);
}

static inline int16_t le16_to_cpu(int16_t x){
  return(UINT16_SWAP_LE_BE_C(x));
}


#endif

static inline int16_t cpu_to_be16(int16_t x){
  return(be16_to_cpu(x));
}

static inline int16_t cpu_to_le16(int16_t x){
  return(le16_to_cpu(x));
}

void cderror(cdrom_drive_t *d, const char *s);

void cdmessage(cdrom_drive_t *d,const char *s);

void idperror(int messagedest, char **messages, const char *f, const char *s);

void idmessage(int messagedest, char **messages, const char *f, const char *s);

