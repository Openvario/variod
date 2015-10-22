#include "nmea_parser.h"
#include "def.h"

extern int g_debug;
extern int g_foreground;
extern FILE *fp_console;

void parse_NMEA_sensor(char* message, t_sensor_context* sensors)
{ 
	char *val;
	char buffer[100];
	char delimiter[]=",*";
	char *ptr;
	
	// copy string and initialize strtok function
	strncpy(buffer, message, strlen(message));
	ptr = strtok(buffer, delimiter);	
	
	while (ptr != NULL)
	{
		
		switch (*ptr)
		{
			case '$':
			// skip start of NMEA sentence
			break;
			
			case 'E':
			// TE vario value
			// get next value
			ptr = strtok(NULL, delimiter);
			val = (char *) malloc(strlen(ptr));
			strncpy(val,ptr,strlen(ptr));
			sensors->e = atof(val);
			break;
			
			default:
			break;
		}
		// get next part of string
		ptr = strtok(NULL, delimiter);
	}
}

void parse_NMEA_command(char* message){
	
	char *val;
	char buffer[100];
	char delimiter[]=",*";
	char *ptr;
  t_polar polar;
	
	// copy string and initialize strtok function
	strncpy(buffer, message, strlen(message));
	ptr = strtok(buffer, delimiter);

	while (ptr != NULL)
	{	
		switch (*ptr)
		{
			case '$':
			// skip start of NMEA sentence
			break;
			
			case 'C':
			// Command sentence
			// get next value
			ptr = strtok(NULL, delimiter);
			
			switch (*ptr)
			{
				case 'M':
					if (*(ptr+1) == 'C') {
						//Set McCready 
						//printf("Set McCready\n");
						ptr = strtok(NULL, delimiter);
						val = (char *) malloc(strlen(ptr));
						strncpy(val,ptr,strlen(ptr));
						setMC(atof(val));
						debug_print("Get McCready Value: %f\n",atof(val));
					}
				break;
				
				case 'W':
					if (*(ptr+1) == 'L') {
						//Set Wingload/Ballast 
						ptr = strtok(NULL, delimiter);
						val = (char *) malloc(strlen(ptr));
						strncpy(val,ptr,strlen(ptr));
						setBallast(atof(val));
						debug_print("Get Ballast Value: %f\n",atof(val));
					}
				break;
				
				case 'B':
					if (*(ptr+1) == 'U') {
						//Set Bugs 
						ptr = strtok(NULL, delimiter);
						val = (char *) malloc(strlen(ptr));
						strncpy(val,ptr,strlen(ptr));
						setDegradation(atof(val));
						debug_print("Get Bugs Value: %f\n",atof(val));
					}
				break;
				
				case 'P':
					//Set Polar
					if (*(ptr+1) == 'O' &&  *(ptr+2) == 'L') {
						ptr = strtok(NULL, delimiter);
						val = (char *) malloc(strlen(ptr));
						strncpy(val,ptr,strlen(ptr));
						polar.a=atof(val);
						ptr = strtok(NULL, delimiter);
						strncpy(val,ptr,strlen(ptr));
						polar.b=atof(val);
						ptr = strtok(NULL, delimiter);
						strncpy(val,ptr,strlen(ptr));
						polar.c=atof(val);
						ptr = strtok(NULL, delimiter);
						strncpy(val,ptr,strlen(ptr));
						polar.w=atof(val);
						setPolar(polar.a, polar.b, polar.c, polar.w);
					}
				break;

				case 'S':
					if (*(ptr+1) == 'T' && *(ptr+2) == 'F') {
						// Set STF Mode
						debug_print("Set STF Mode\n");
						set_vario_mode(stf);
					}
				break;
				case 'V':
					if (*(ptr+1) == 'U') {
						// volume up
						debug_print("Volume up\n");
						change_volume(+10.0);
					}
					if (*(ptr+1) == 'D') {
						// volume down
						debug_print("Volume down\n");
						change_volume(-10.0);
					}
					if (*(ptr+1) == 'M') {
						// Toggle Mute
						debug_print("Toggle Mute\n");
						toggle_mute();
					}
					if (*(ptr+1) == 'A' &&  *(ptr+2) == 'R') {
						// Set Vario Mode
						debug_print("Set Vario Mode\n");
						set_vario_mode(vario);
					}
				break;
				
			}
			break;
			
			default:
			break;
		}
		// get next part of string
		ptr = strtok(NULL, delimiter);
	}
}
