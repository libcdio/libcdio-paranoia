/*
  Copyright (C) 2005, 2008, 2013, 2019 Rocky Bernstein <rocky@gnu.org>
  Copyright (C) 1998-2008 Monty xiphmont@mit.edu
  derived from code (C) 1994-1996 Heiko Eissfeldt

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
 * Table of contents convenience functions
 ******************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
# define __CDIO_CONFIG_H__ 1
#endif

#include "low_interface.h"
#include "utils.h"
#include <cdio/paranoia/toc.h>

/*! Return the lsn for the start of track i_track or CDIO_LEADOUT_TRACK */
lsn_t
cdda_track_firstsector(cdrom_drive_t *d, track_t i_track)
{
  if(!d->opened){
    cderror(d,"400: Device not open\n");
    return(-400);
  } else {
    const track_t i_first_track = cdio_get_first_track_num(d->p_cdio);
    const track_t i_last_track  =
	cdio_get_last_track_num(d->p_cdio) + 1; /* include leadout */

    if (i_track == CDIO_CDROM_LEADOUT_TRACK) i_track = i_last_track;
    if (i_track == 0) {
      if (d->disc_toc[0].dwStartSector == 0) {
	/* first track starts at lba 0 -> no pre-gap */
	cderror(d,"402: No initial pregap\n");
	return(-402);
      }
      else {
	return 0; /* pre-gap of first track always starts at lba 0 */
      }
    } else if (i_track < i_first_track || i_track > i_last_track) {
      char buf[100];
      snprintf(buf, sizeof(buf), "401: Invalid track number %02d\n", i_track);
      cderror(d, buf);
      return(-401);
    }
    return(d->disc_toc[i_track-i_first_track].dwStartSector);
  }
}

/*! Get first lsn of the first audio track. -1 is returned on error. */
lsn_t
cdda_disc_firstsector(cdrom_drive_t *d)
{
  int i;
  int first_track = cdio_get_first_track_num(d->p_cdio);

  if(!d->opened){
    cderror(d,"400: Device not open\n");
    return(-400);
  }

  /* look for an audio track */
  for (i = first_track - 1; i < first_track - 1 + d->tracks; i++)
    if( cdda_track_audiop(d, i+1)==1 ) {
      if (i == first_track - 1) /* disc starts at lba 0 if first track is an audio track */
       return 0;
      else
       return cdda_track_firstsector(d, i+1);
    }

  cderror(d,"403: No audio tracks on disc\n");
  return(-403);
}

/*! Get last lsn of the track. The lsn is generally one less than the
  start of the next track. -1 is returned on error. */
lsn_t
cdda_track_lastsector(cdrom_drive_t *d, track_t i_track)
{
  if(!d->opened){
    cderror(d,"400: Device not open\n");
    return(-400);
  } else {
    const track_t i_first_track = cdio_get_first_track_num(d->p_cdio);
    const track_t i_last_track  = cdio_get_last_track_num(d->p_cdio);

    if (i_track == 0) {
      if (d->disc_toc[0].dwStartSector == 0) {
	/* first track starts at lba 0 -> no pre-gap */
	cderror(d,"402: No initial pregap\n");
	return(-402);
      }
      else {
	return d->disc_toc[0].dwStartSector-1;
      }
    } else if (i_track < i_first_track || i_track > i_last_track) {
      char buf[100];
      snprintf(buf, sizeof(buf), "401: Invalid track number %02d\n", i_track);
      cderror(d, buf);
      return(-401);
    }

    /* CD Extra have their first session ending at the last audio track */
    if (d->cd_extra > 0 && i_track-i_first_track+2 <= d->tracks) {
      if (d->audio_last_sector >= d->disc_toc[i_track-i_first_track].dwStartSector &&
          d->audio_last_sector < d->disc_toc[i_track-i_first_track+1].dwStartSector) {
        return d->audio_last_sector;
      }
    }

    /* Index safe because we always have the leadout at
     * disc_toc[tracks] */
    return(d->disc_toc[i_track-i_first_track+1].dwStartSector-1);
  }
}

/*! Get last lsn of the last audio track. The last lssn generally one
  less than the start of the next track after the audio track. -1 is
  returned on error. */
lsn_t
cdda_disc_lastsector(cdrom_drive_t *d)
{
  if (!d->opened) {
    cderror(d,"400: Device not open\n");
    return -400;
  } else {
    /* look for an audio track */
    const track_t i_first_track = cdio_get_first_track_num(d->p_cdio);
    track_t i = cdio_get_last_track_num(d->p_cdio);
    for ( ; i >= i_first_track; i-- )
      if ( cdda_track_audiop(d,i) )
	return (cdda_track_lastsector(d,i));
  }
  cderror(d,"403: No audio tracks on disc\n");
  return -403;
}

/*! Return the number of tracks on the or 300 if error. */
track_t
cdda_tracks(cdrom_drive_t *d)
{
  if (!d->opened){
    cderror(d,"400: Device not open\n");
    return CDIO_INVALID_TRACK;
  }
  return(d->tracks);
}

/*! Return the track containing the given LSN. If the LSN is before
    the first track (in the pregap), 0 is returned. If there was an
    error or the LSN after the LEADOUT (beyond the end of the CD), then
    CDIO_INVALID_TRACK is returned.
 */
int
cdio_cddap_sector_gettrack(cdrom_drive_t *d, lsn_t lsn)
{
  if (!d->opened) {
    cderror(d,"400: Device not open\n");
    return CDIO_INVALID_TRACK;
  } else {
    if (lsn < d->disc_toc[0].dwStartSector)
      return 0; /* We're in the pre-gap of first track */

    return cdio_get_track(d->p_cdio, lsn);
  }
}

/*! Return the number of channels in track: 2 or 4; -2 if not
  implemented or -1 for error.
  Not meaningful if track is not an audio track.
*/
extern int
cdio_cddap_track_channels(cdrom_drive_t *d, track_t i_track)
{
  return(cdio_get_track_channels(d->p_cdio, i_track));
}

/*! Return 1 is track is an audio track, 0 otherwise. */
extern int
cdio_cddap_track_audiop(cdrom_drive_t *d, track_t i_track)
{
  track_format_t track_format = cdio_get_track_format(d->p_cdio, i_track);
  return TRACK_FORMAT_AUDIO == track_format ? 1 : 0;
}

/*! Return 1 is track is an audio track, 0 otherwise. */
extern int
cdio_cddap_track_copyp(cdrom_drive_t *d, track_t i_track)
{
  track_flag_t track_flag = cdio_get_track_copy_permit(d->p_cdio, i_track);
  return CDIO_TRACK_FLAG_TRUE == track_flag ? 1 : 0;
}

/*! Return 1 is audio track has linear preemphasis set, 0 otherwise.
    Only makes sense for audio tracks.
 */
extern int
cdio_cddap_track_preemp(cdrom_drive_t *d, track_t i_track)
{
  track_flag_t track_flag = cdio_get_track_preemphasis(d->p_cdio, i_track);
  return CDIO_TRACK_FLAG_TRUE == track_flag ? 1 : 0;
}
