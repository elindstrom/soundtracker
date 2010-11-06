
/*
 * The Real SoundTracker - General support routines (header)
 *
 * Copyright (C) 1998-2001 Michael Krause
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _ST_SUBS_H
#define _ST_SUBS_H

#include "xm.h"

/* --- Module functions --- */
void          st_free_all_pattern_channels             (XM *xm);
int           st_init_pattern_channels                 (XMPattern *p, unsigned length, int num_channels);
int           st_instrument_num_save_samples           (STInstrument *instr);
int           st_num_save_instruments                  (XM *xm);
int           st_num_save_patterns                     (XM *xm);
void          st_clean_song                            (XM *);
void          st_set_num_channels                      (XM *, int);

/* --- Pattern functions --- */
XMPattern*    st_dup_pattern                           (XMPattern*);
void          st_free_pattern_channels                 (XMPattern *pat);
void          st_clear_pattern                         (XMPattern*);
int           st_copy_pattern                          (XMPattern *dst,
							XMPattern *src);
void          st_pattern_delete_track                  (XMPattern*, int);
void          st_pattern_insert_track                  (XMPattern*, int);
gboolean      st_is_empty_pattern                      (XMPattern *p);
int           st_find_first_unused_and_empty_pattern   (XM *xm);
gboolean      st_is_pattern_used_in_song               (XM *xm,
							int patnum);
void          st_set_pattern_length                    (XMPattern *, int);
void          st_exchange_patterns                     (XM *xm,
							int p1,
							int p2);
gboolean      st_check_if_odd_are_not_empty	       (XMPattern *p);
void	      st_shrink_pattern			       (XMPattern *p);
void	      st_expand_pattern			       (XMPattern *p);

/* --- Track functions --- */
XMNote*       st_dup_track                             (XMNote*, int length);
XMNote*       st_dup_track_wrap                        (XMNote*, int tracklength, int copystart, int copylength);
void          st_clear_track                           (XMNote*, int length);
void          st_clear_track_wrap                      (XMNote*, int tracklength, int clearstart, int clearlength);
void          st_paste_track_into_track_wrap           (XMNote *from, XMNote *to, int tolength, int insertstart, int fromlength);
gboolean      st_is_empty_track                        (XMNote *notes,
							int length);

/* --- Instrument functions --- */
int           st_instrument_num_samples                (STInstrument *i);
void          st_clean_instrument                      (STInstrument *i, const char *name);
void          st_copy_instrument		       (STInstrument *src, STInstrument *dest);
gboolean      st_instrument_used_in_song               (XM *xm, int instr);

/* --- Sample functions --- */
void          st_clean_sample                          (STSample *s, const char *name);
void          st_sample_fix_loop                       (STSample *s);
void          st_convert_sample                        (void *src,
							void *dst,
							int srcformat,
							int dstformat,
							int count);
void          st_sample_cutoff_lowest_8_bits           (gint16 *data,
							int count);
void          st_sample_8bit_signed_unsigned           (gint8 *data,
							int count);
void          st_sample_16bit_signed_unsigned          (gint16 *data,
							int count);

#endif /* _ST_SUBS_H */
