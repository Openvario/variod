#include "nmea_parser.h"
#include "def.h"
#include "utils.h"
#include "nmea_checksum.h"

extern int g_debug;
extern int g_foreground;
extern FILE *fp_console;

void parse_NMEA_sensor(char* message, t_sensor_context* sensors)
{ 
	// Expect 1 NMEA sentence at a time!
	// For information on NMEA sentence structure refer to this:
	// https://en.wikipedia.org/wiki/NMEA_0183#Message_structure
	// Exception to this: sentence must be terminated with '\0', no '\n', or '\r' allowed.

	static char buffer[100]; // NMEA sentence must not be longer than 82 chars
	const char delimiter[]=",*";
	char *ptr=NULL;

	ddebug_print("sensor >%s<\n",message);

	unsigned int len = strlen(message);
	if (len >= sizeof(buffer)) return; // sentence longer than expected

	if (!verify_nmea_checksum(message)) return; // checksum error

	// copy string so we can modify it
	strncpy(buffer, message, len);

	// it's checksum clean we don't want checksum to be mistaken as value
	nmea_chop_checksum(buffer);

	ptr = strtok(buffer, delimiter);

	if (strcmp(ptr,"$POV") == 0) ptr = strtok(NULL, delimiter);
	else ptr = NULL;

	while (ptr != NULL) {
		char *val = strtok(NULL, delimiter);
		if (val == NULL) return; // there is no value after the qualifier

		char *endptr;
		float fv = strtof(val,&endptr);
		if (endptr == val || *endptr != 0) return; // not a clean number

		if (strcmp(ptr,"E") == 0) {
			// TE vario value in m/s
			// positive is up, negative is down
			ddebug_print("E %f\n",fv);
			sensors->e = fv;

		} else if (strcmp(ptr,"Q") == 0) {
			// Dynamic pressure in Pa
			ddebug_print("Q %f\n",fv);
			sensors->q = fv;

		} else if (strcmp(ptr,"P") == 0) {
			// Static pressure in hPa
			ddebug_print("P %f\n",fv);
			sensors->p = fv;
		}

		// get next part of string
		ptr = strtok(NULL, delimiter);
	}
	return;
}

void parse_NMEA_command(char* message){
	
	// Expect 1 NMEA sentence at a time!
	// For information on NMEA sentence structure refer to this:
	// https://en.wikipedia.org/wiki/NMEA_0183#Message_structure
	// Exception to this: sentence must be terminated with '\0', no '\n', or '\r' allowed.

	char buffer[100]; // NMEA sentence has 82 charactes at most
	char delimiter[]=",*";
	char *ptr;
	static float fvals[NUM_FV];
	
	ddebug_print("command >%s<\n",message);

	unsigned int len = strlen(message);
	if (len >= sizeof(buffer)) return; // sentence longer than expected

	if (!verify_nmea_checksum(message)) return; // checksum error

	// copy string so we can modify the message
	strncpy(buffer, message, len);

	// now it's checksum clean we don't want checksum to be mistaken as value
	nmea_chop_checksum(buffer);

	ptr = strtok(buffer, delimiter);

	if (ptr && (strcmp(ptr,"$POV") == 0)) ptr = strtok(NULL, delimiter);
	else ptr = NULL;

	if (ptr && (strcmp(ptr,"C") == 0)) ptr = strtok(NULL, delimiter);
	else ptr = NULL;

	if (ptr != NULL)
	{	
		switch (*ptr)
		{
			case 'M':
				//Set McCready
				if (strcmp(ptr,"MC") == 0) {
					if (read_float_from_sentence(1,fvals,NULL,delimiter)) {
						setMC(fvals[0]);
						debug_print("Get McCready Value: %f\n",fvals[0]);
					}
				}
			break;
			
			case 'W':
				//Set Overload
				// the value here is Total_Mass/Reference_Mass
				// Total_Mass = Dry_Mass + Ballast
				// Reference_Mass is the mass of the glider for which the polar is valid
				if (strcmp(ptr,"WL") == 0) {
					if (read_float_from_sentence(1,fvals,NULL,delimiter)) {
						setBallast(fvals[0]);
						debug_print("Get Ballast Value: %f\n",fvals[0]);
					}
				}
			break;

			case 'B':
				//Set Bugs, aka polar degradation
				// 5% degradation is represented here as a value of 0.95
				if (strcmp(ptr,"BU") == 0) {
					if (read_float_from_sentence(1,fvals,NULL,delimiter)) {
						setDegradation(fvals[0]);
						debug_print("Get Bugs Value: %f\n",fvals[0]);
					}
				}
			break;

			case 'P':
				//Set Polar
				// depreciated unless sombody is using it and understands which
				// parmeters and units are presented or expectd
				if (strcmp(ptr,"POL") == 0) {
					if (read_float_from_sentence(4,fvals,NULL,delimiter)) {
						debug_print("Get Polar POL: %f, %f, %f, %f\n",
							fvals[0],fvals[1],fvals[2],fvals[3]);
						setPolar(fvals[0],fvals[1],fvals[2],fvals[3]);
					}
				}
			break;

			case 'R':
				//Set Real Polar
				if (strcmp(ptr,"RPO") == 0) {
					if (read_float_from_sentence(3,fvals,NULL,delimiter)) {
						// convert horizontal speed from m/s to km/h
						float a=fvals[0]/(3.6*3.6);
						float b=fvals[1]/3.6;
						float c=fvals[2];
						setRealPolar(a, b, c);
						debug_print("Get Polar RPO: %e, %e, %e\n",
									a,b,c);
					}
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
				}
				else if (strcmp(ptr,"VU") == 0) {
					// volume up
					debug_print("Volume up\n");
					change_volume(+10.0);
				}
				else if (strcmp(ptr,"VD") == 0) {
					// volume down
					debug_print("Volume down\n");
					change_volume(-10.0);
				}
				else if (strcmp(ptr,"VM") == 0) {
					// Toggle Mute
					debug_print("Toggle Mute\n");
					toggle_mute();
				}
			break;
		}
	}
	// there shouldn't be anything left on that sentence
	if ((ptr = strtok(NULL, delimiter)) != NULL)
		debug_print("Not the end of the sentece: >%s< is left\n",ptr);
	return;
}
