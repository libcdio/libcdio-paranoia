/*
  Copyright (C) 2004, 2005, 2008 Rocky Bernstein <rocky@gnu.org>
  Copyright (C) 1998 Monty <xiphmont@mit.edu>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define copystring(s) (s) ? s : NULL;
  
static inline char *
catstring(char *buff, const char *s)
{
  if(s){
    if(buff)
      buff=realloc(buff,strlen(buff)+strlen(s)+1);
    else
      buff=calloc(strlen(s)+1,1);
    strcat(buff,s);
  }
  return(buff);
}

