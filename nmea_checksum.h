/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2016 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#ifndef VARIOD_NMEA_CHECKSUM_H
#define VARIOD_NMEA_CHECKSUM_H

#include <cstdint>
#include <cstring>

/**
 * Calculates the checksum for the specified line (without the
 * asterisk and the newline character).
 *
 * @param p a NULL terminated string
 */
static inline uint8_t
nmea_checksum(const char *p)
{
	uint8_t checksum = 0;

	/* skip the dollar sign at the beginning (the exclamation mark is
	   used by CAI302 */
	if (*p == '$' || *p == '!')
		++p;

	while (*p != 0)
		checksum ^= *p++;

	return checksum;
}

/**
 * Chops off the checksum part of the sentence
 * That is the part including and after the asterisk
 *
 * @param p a NULL terminated string
 */
static inline void
nmea_chop_checksum(char *p)
{
	char *asterisk;

	if ((asterisk = strchr(p,'*')) != NULL) *asterisk = '\0';
}

/**
 * Calculates the checksum for the specified line (without the
 * asterisk and the newline character).
 *
 * @param p a string
 * @param length the number of characters in the string
 */
static inline uint8_t
nmea_checksum(const char *p, unsigned length)
{
	uint8_t checksum = 0;

	unsigned i = 0;

	/* skip the dollar sign at the beginning (the exclamation mark is
	   used by CAI302 */
	if (length > 0 && (*p == '$' || *p == '!')) {
		++i;
		++p;
	}

	for (; i < length; ++i)
		checksum ^= *p++;

	return checksum;
}

/**
 * Verify the NMEA checksum at the end of the specified string,
 * separated with an asterisk ('*').
 */
bool
verify_nmea_checksum(const char *p);

/**
 * Caclulates the checksum of the specified string, and appends it at
 * the end, preceded by an asterisk ('*').
 */
void
append_nmea_checksum(char *p);

#endif // VARIOD_NMEA_CHECKSUM_H
