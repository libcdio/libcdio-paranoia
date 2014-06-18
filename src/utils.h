/*
  Copyright (C) 2004, 2005, 2008, 2014 Rocky Bernstein <rocky@gnu.org>
  Copyright (C) 1998 Monty <xiphmont@mit.edu>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

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

/** Returns basename(fullname) and sets path to the dirname.
    rename includes a trailing slash execpt when dirname is empty.
*/
static inline char *
split_base_dir(char *fullpath, char *path, unsigned int max)
{
  char *post         = strrchr(fullpath, '/');
  int   pos          = (post ? post-fullpath+1 : 0);
  path[0]='\0';
  if (pos>max) return NULL;
  if (fullpath[pos] == '/') pos++;
  if (pos) strncat(path, fullpath, pos);
  return fullpath + pos;
}

/* By renaming this to utils.c and compiling like this:
   gcc -DSTANDALONE -o utils utils.h
you can demo this code.
*/
#ifdef STANDALONE
int main(int argc, char **argv)
{
  int i;
  const char *paths[] = {"/abc/def", "hij/klm"};
  char path[10];
  for (i=0; i<2; i++) {
      char *fullpath = strdup(paths[i]);
      char *base = split_base_dir(fullpath, path, sizeof(path));
      printf("dirname of %s is %s; basename is %s\n", fullpath, path, base);
      if (fullpath) free(fullpath);
  }
}
#endif
