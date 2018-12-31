#include <cdio/version.h>

#include <stdlib.h>
#include <stdio.h>
int
main(int argc, const char *argv[])
{
  if (LIBCDIO_VERSION_NUM > 20000) {
    return system("./check_paranoia.sh");
  } else {
    fprintf(stderr,
	    "Please upgrade libcdio to a version after 2.0.0 to handle CD-DAs that don't start at track 1\n");
    return 77;
  }
}
