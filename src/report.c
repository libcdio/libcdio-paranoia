/*
  Copyright (C) 2004, 2008, 2010, 2011 Rocky Bernstein <rocky@gnu.org>
  Copyright (C) 1998 Monty xiphmont@mit.edu
*/

/******************************************************************
 * 
 * reporting/logging routines
 *
 ******************************************************************/


/* config.h has to come first else _FILE_OFFSET_BITS are redefined in
   say opensolaris. */
#ifdef HAVE_CONFIG_H
# include "config.h"
# define __CDIO_CONFIG_H__ 1
#endif

#include <stdio.h>
#include <cdio/paranoia/cdda.h>
#include "report.h"

int quiet=0;
int verbose=CDDA_MESSAGE_FORGETIT;
FILE *reportfile=NULL;
