/*
	  sensord - Sensor Interface for XCSoar Glide Computer - http://www.openvario.org/
    Copyright (C) 2014  The openvario project
    A detailed list of copyright holders can be found in the file "AUTHORS"

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 3
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

/**
 * access class for various utilities and types
 */

#include <stdbool.h>

#define NUM_FV  5 // maximum number of values per NMEA sentence

/*****************************************
 * @brief check if all requested parameter(s) are available from the
 * current position in the NMEA sentence before we use the values.
 * Applies all reasonable checks before accepting the parameres' values.
 * n  number of expected parameter(s).
 * fv pointer to an array of floats with minimum n elements to
 *    write the values to.
 * str pointer to the position of the first parameter in the sentence.
 * delim string of chars representing the limiters between values in the sentence.
 * return true on success or false if any of the checks fail
 ******************************************/
bool read_float_from_sentence(int n,float fv[],char *str);
