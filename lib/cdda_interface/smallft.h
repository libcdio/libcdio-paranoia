/*
  $Id: smallft.h,v 1.2 2008/04/16 17:00:40 karl Exp $

  Copyright (C) 2008 Rocky Bernstein <rocky@gnu.org>
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

/******************************************************************
 * FFT implementation from OggSquish, minus cosine transforms.
 * Only convenience functions exposed
 ******************************************************************/

extern void fft_forward(int n, float *buf, float *trigcache, int *sp);
extern void fft_backward(int n, float *buf, float *trigcache, int *sp);
extern void fft_i(int n, float **trigcache, int **splitcache);
