/*
  Copyright (C) 2014 Rocky Bernstein <rocky@gnu.org>

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

/* test of routines in src/utils.h. For more verbse output, pass
   an argument. For example:
     make testutils
     ./testutils debug

*/

#ifdef HAVE_CONFIG_H
# include "config.h"
# define __CDIO_CONFIG_H__ 1
#endif

#include "../src/utils.h"

typedef struct {
    const char *fullname;
    const char *dirname;
    const char *basename;
} testdata;

int
main(int argc, const char *argv[])
{
  int i_rc=0;
  int i;
  const testdata paths[] = {
    {"/abc/def", "/abc/", "def"},
    {"hij/klm", "hij/",   "klm"},
    {"1234567890/klm", NULL,   NULL}
  };
  char path[10];
  for (i=0; i<3; i++) {
    char *fullpath = strdup(paths[i].fullname);
    char *base = split_base_dir(fullpath, path, sizeof(path));
    if (base == NULL) {
      if (paths[i].basename != NULL) {
	fprintf(stderr, "Got an unexpected null. Expected %s\n",
		paths[i].basename);
      } else {
	if (argc > 1) printf("%s is too large as expected\n", fullpath);
      }
    } else {
      if (argc > 1) {
	printf("dirname of %s is %s; basename is %s\n",
	       fullpath, path, base);
      }
      if (0 != strcmp(paths[i].basename, base)) {
	fprintf(stderr,
		"Expecting basename %s; got %s for %s\n",
		paths[i].basename, base, fullpath);
	i_rc = 1;
      }
      if (0 != strcmp(paths[i].dirname, path)) {
	fprintf(stderr,
		"Expecting dirname %s; got %s for %s\n",
		paths[i].dirname, path, fullpath);
	i_rc = 1;
      }
    }
    if (fullpath) free(fullpath);
  }
  exit(i_rc);
}
