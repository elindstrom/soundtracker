
/*
 * The Real SoundTracker - DOS Charset recoder
 *
 * Copyright (C) 1998-2001 Michael Krause
 *
 * The tables have been taken from recode-3.4.1/ibmpc.c:
 * Copyright (C) 1990, 1993, 1994 Free Software Foundation, Inc.
 * Francois Pinard <pinard@iro.umontreal.ca>, 1988.
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

#include <glib.h>

#include "recode.h"

static unsigned char ibmpc_convert_rulers[48] =
  {
    '#',			/* 176 */
    '#',			/* 177 */
    '#',			/* 178 */
    '|',			/* 179 */
    '+',			/* 180 */
    '|',			/* 181 */
    '+',			/* 182 */
    '.',			/* 183 */
    '.',			/* 184 */
    '|',			/* 185 */
    '|',			/* 186 */
    '.',			/* 187 */
    '\'',			/* 188 */
    '\'',			/* 189 */
    '\'',			/* 190 */
    '.',			/* 191 */
    '`',			/* 192 */
    '+',			/* 193 */
    '+',			/* 194 */
    '+',			/* 195 */
    '-',			/* 196 */
    '+',			/* 197 */
    '|',			/* 198 */
    '+',			/* 199 */
    '`',			/* 200 */
    '.',			/* 201 */
    '=',			/* 202 */
    '=',			/* 203 */
    '|',			/* 204 */
    '=',			/* 205 */
    '=',			/* 206 */
    '=',			/* 207 */
    '+',			/* 208 */
    '=',			/* 209 */
    '+',			/* 210 */
    '`',			/* 211 */
    '`',			/* 212 */
    '.',			/* 213 */
    '.',			/* 214 */
    '+',			/* 215 */
    '=',			/* 216 */
    '\'',			/* 217 */
    '.',			/* 218 */
    '#',			/* 219 */
    '#',			/* 220 */
    '#',			/* 221 */
    '#',			/* 222 */
    '#',			/* 223 */
  };

typedef struct known_pair KNOWN_PAIR;

struct known_pair
  {
    unsigned char left;		/* first character in pair */
    unsigned char right;	/* second character in pair */
  };

/* Data for IBM PC to ISO Latin-1 code conversions.  */

static KNOWN_PAIR ibmpc_known_pairs[] =
  {
    { 20, 182},			/* pilcrow sign */
    { 21, 167},			/* section sign */

    {128, 199},			/* capital letter C with cedilla */
    {129, 252},			/* small letter u with diaeresis */
    {130, 233},			/* small letter e with acute accent */
    {131, 226},			/* small letter a with circumflex accent */
    {132, 228},			/* small letter a with diaeresis */
    {133, 224},			/* small letter a with grave accent */
    {134, 229},			/* small letter a with ring above */
    {135, 231},			/* small letter c with cedilla */
    {136, 234},			/* small letter e with circumflex accent */
    {137, 235},			/* small letter e with diaeresis */
    {138, 232},			/* small letter e with grave accent */
    {139, 239},			/* small letter i with diaeresis */
    {140, 238},			/* small letter i with circumflex accent */
    {141, 236},			/* small letter i with grave accent */
    {142, 196},			/* capital letter A with diaeresis */
    {143, 197},			/* capital letter A with ring above */
    {144, 201},			/* capital letter E with acute accent */
    {145, 230},			/* small ligature a with e */
    {146, 198},			/* capital ligature A with E */
    {147, 244},			/* small letter o with circumblex accent */
    {148, 246},			/* small letter o with diaeresis */
    {149, 242},			/* small letter o with grave accent */
    {150, 251},			/* small letter u with circumflex accent */
    {151, 249},			/* small letter u with grave accent */
    {152, 255},			/* small letter y with diaeresis */
    {153, 214},			/* capital letter O with diaeresis */
    {154, 220},			/* capital letter U with diaeresis */
    {155, 162},			/* cent sign */
    {156, 163},			/* pound sign */
    {157, 165},			/* yen sign */

    {160, 225},			/* small letter a with acute accent */
    {161, 237},			/* small letter i with acute accent */
    {162, 243},			/* small letter o with acute accent */
    {163, 250},			/* small letter u with acute accent */
    {164, 241},			/* small letter n with tilde */
    {165, 209},			/* capital letter N with tilde */
    {166, 170},			/* feminine ordinal indicator */
    {167, 186},			/* masculine ordinal indicator */
    {168, 191},			/* inverted question mark */

    {170, 172},			/* not sign */
    {171, 189},			/* vulgar fraction one half */
    {172, 188},			/* vulgar fraction one quarter */
    {173, 161},			/* inverted exclamation mark */
    {174, 171},			/* left angle quotation mark */
    {175, 187},			/* right angle quotation mark */

    {225, 223},			/* small german letter sharp s */

    {230, 181},			/* small Greek letter mu micro sign */

    {241, 177},			/* plus-minus sign */

    {246, 247},			/* division sign */

    {248, 176},			/* degree sign */

    {250, 183},			/* middle dot */

    {253, 178},			/* superscript two */

    {255, 160},			/* no-break space */
  };
#define NUMBER_OF_PAIRS (sizeof (ibmpc_known_pairs) / sizeof (KNOWN_PAIR))

void
recode_ibmpc_to_latin1 (char *string, int len)
{
    guint8 c;
    int i;

    while((c = *string) && len) {
	if(c >= 176 && c <= 223) {
	    c = ibmpc_convert_rulers[c - 176];
	} else {
	    for(i = 0; i < NUMBER_OF_PAIRS; i++) {
		if(ibmpc_known_pairs[i].left == c) {
		    c = ibmpc_known_pairs[i].right;
		    break;
		}
	    }
	}
	*string++ = c;
	len--;
    }
}

void
recode_latin1_to_ibmpc (char *string, int len)
{
    guint8 c;
    int i;

    while((c = *string) && len) {
	for(i = 0; i < NUMBER_OF_PAIRS; i++) {
	    if(ibmpc_known_pairs[i].right == c) {
		c = ibmpc_known_pairs[i].left;
		break;
	    }
	}
	*string++ = c;
	len--;
    }
}
