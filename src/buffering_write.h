/*
  Copyright (C) 2004, 2008 Rocky Bernstein <rocky@gnu.org>
  Copyright (C) 1998 Monty <xiphmont@mit.edu>
*/

/** buffering_write() - buffers data to a specified size before writing.
 *
 * Restrictions:
 * - MUST CALL BUFFERING_CLOSE() WHEN FINISHED!!!
 *
 */
extern long buffering_write(int outf, char *buffer, long num);

/** buffering_close() - writes out remaining buffered data before
 * closing file.
 *
 */
extern int buffering_close(int fd);


