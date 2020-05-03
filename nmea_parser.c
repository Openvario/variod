/*  vario_app - Audio Vario application - http://www.openvario.org/
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

#include "nmea_parser.h"
#include "def.h"
#include "nmea_checksum.h"
#define NUM_FV  10 // maximum number of values per NMEA sentence

extern int g_debug;
extern int g_foreground;
extern FILE *fp_console;

// see if we get all requested parameter(s) before we start setting them
bool robust_read_float(int n,float fv[],char *str,const char *delim)
{
	char *vp;
	char *endptr;
	if (n > NUM_FV) return false;

	for (int i=0; i < n; i++) {
		vp = strtok(str,delim);
		if (vp == NULL) return false;
		else {
			fv[i] = strtod(vp,&endptr);
			if (*endptr != '\0') return false;
		}
	}
	return true;
}

void parse_NMEA_sensor(char* message, t_sensor_context* sensors)
{
	// expect 1 NMEA sentence at a time!
	// sentence must be terninated with '\0', '\n', or '\r'
	char *val;
	float fv;
	static char buffer[100];
	const char delimiter[]=",*";
	char *ptr=NULL;
	char *endptr;

	unsigned int len = strlen(message);
	if (len >= sizeof(buffer)) return; // sentence longer than expected

	if (!verify_nmea_checksum(message)) return; // checksum error

	// now it's checksum clean we don't want checksum to be mistaken as value
	nmea_chop_checksum(message);

	// copy string and initialize strtok function
	strncpy(buffer, message, len);
	ptr = strtok(buffer, delimiter);

	if (strcmp(ptr,"$POV") == 0) ptr = strtok(NULL, delimiter);
	else ptr = NULL;

	while (ptr != NULL) {
		if (*ptr == '\n' || *ptr == '\r') return; // end of sentence

		val = strtok(NULL, delimiter);
		if (val == NULL) return; // there is no value after the qualifier

		fv = strtod(val,&endptr);
		// number must be at the end of the token
		if (*endptr != '\0') return;

		if (strcmp(ptr,"E") == 0) {
			// TE vario value
			sensors->e = fv;

		} else if (strcmp(ptr,"Q") == 0) {
			// Airspeed value
			sensors->q = fv;
		}

		// get next part of string
		ptr = strtok(NULL, delimiter);
	}
}

void parse_NMEA_command(char* message)
{
	// expect 1 NMEA sentence at a time!
	// sentence must be terninated with '\0', '\n', or '\r'
	static char buffer[100];
	const char delimiter[]=",*";
	char *ptr;
	t_polar polar;
	static float fvals[NUM_FV];

	unsigned int len = strlen(message);
	if (len >= sizeof(buffer)) return; // sentence longer than expected

	if (!verify_nmea_checksum(message)) return; // checksum error

	// now it's checksum clean we don't want checksum to be mistaken as value
	nmea_chop_checksum(message);

	// copy string and initialize strtok function
	strncpy(buffer, message, len);
	ptr = strtok(buffer, delimiter);

	if (ptr && (strcmp(ptr,"$POV") == 0)) ptr = strtok(NULL, delimiter);
	else ptr = NULL;

	if (ptr && (strcmp(ptr,"C") == 0)) ptr = strtok(NULL, delimiter);
	else ptr = NULL;

	while (ptr != NULL) {
		switch (*ptr) {
		case '\n':
		case '\r':
			return; // end of sentence

		case 'M':
			if (strcmp(ptr,"MC") == 0) {
				//Set McCready
				//printf("Set McCready\n");
				if (robust_read_float(1,fvals,NULL,delimiter)) {
					setMC(fvals[0]);
					debug_print("Get McCready Value: %f\n",fvals[0]);
				}
			}
			break;

		case 'W':
			if (strcmp(ptr,"WL") == 0) {
				//Set Wingload/Ballast
				// we are getting total_mass / dry_mass
				// we should get total_mass / reference_mass
				// TODO: fix this in XCSoar
				if (robust_read_float(1,fvals,NULL,delimiter)) {
					setBallast(fvals[0]);
					debug_print("Get Ballast Value: %f\n",fvals[0]);
				}
			}
			break;

		case 'B':
			if (strcmp(ptr,"BU") == 0) {
				//Set Bugs
				if (robust_read_float(1,fvals,NULL,delimiter)) {
					setDegradation(fvals[0]);
					debug_print("Get Bugs Value: %f\n",fvals[0]);
				}
			}
			break;

		case 'P':
			if (strcmp(ptr,"POL") == 0) {
				// UNDOCUMENTED feature
				//Set Polar
				if (robust_read_float(4,fvals,NULL,delimiter)) {
					polar.a=fvals[0];
					polar.b=fvals[1];
					polar.c=fvals[2];
					polar.w=fvals[3];
					setPolar(polar.a, polar.b, polar.c, polar.w);
					debug_print("Get Polar POL: %f, %f, %f, %f\n",
								polar.a,polar.b,polar.c,polar.w);
				}
			}
			break;

		case 'I':
			if (strcmp(ptr,"IPO") == 0) {
				//Set ideal Polar
				if (robust_read_float(3,fvals,NULL,delimiter)) {
					polar.a=fvals[0];
					polar.b=fvals[1];
					polar.c=fvals[2];
					setIdealPolar(polar.a, polar.b, polar.c);
					debug_print("Get Polar IPO: %f, %f, %f\n",
								polar.a,polar.b,polar.c);
				}
			}
			break;

		case 'R':
			if (strcmp(ptr,"RPO") == 0) {
				//Set real Polar
				if (robust_read_float(3,fvals,NULL,delimiter)) {
					polar.a=fvals[0];
					polar.b=fvals[1];
					polar.c=fvals[2];
					setRealPolar(polar.a, polar.b, polar.c);
					debug_print("Get Polar RPO: %f, %f, %f\n",
								polar.a,polar.b,polar.c);
				}
			}
			break;

		case 'N':
			if (strcmp(ptr,"NA") == 0) {
				// Requested data not avaliable
				// skip the rest of the sentence
				return;
			}
			break;

		case 'S':
			if (strcmp(ptr,"STF") == 0) {
				// Set STF Mode
				debug_print("Set STF Mode\n");
				set_vario_mode(stf);
			}
			break;

		case 'V':
			if (strcmp(ptr,"VAR") == 0) {
				// Set Vario Mode
				debug_print("Set Vario Mode\n");
				set_vario_mode(vario);

			} else if (strcmp(ptr,"VU") == 0) {
				// volume up
				debug_print("Volume up\n");
				change_volume(+10.0);

			} else if (strcmp(ptr,"VD") == 0) {
				// volume down
				debug_print("Volume down\n");
				change_volume(-10.0);

			} else if (strcmp(ptr,"VM") == 0) {
				// Toggle Mute
				debug_print("Toggle Mute\n");
				toggle_mute();
			}
			break;
		}
		// get next part of string
		ptr = strtok(NULL, delimiter);
	}
}
