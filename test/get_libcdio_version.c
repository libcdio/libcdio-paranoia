/*
 * Prints the version of libcdio.
 *
 * Test scripts can call this to skip tests under certain version conditions.
 */

#include <cdio/version.h>
#include <stdlib.h>
#include <stdio.h>

int
main(void)
{
    printf("%d\n", LIBCDIO_VERSION_NUM);
    return EXIT_SUCCESS;
}
