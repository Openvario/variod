#include "nmea_parser.h"


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
			
			case 'C':
			// Command sentence
			// get next value
			ptr = strtok(NULL, delimiter);
			
			switch (*ptr)
			{
				case 'V':
				if (*(ptr+1) == 'U') {
					// volume up
					//printf("Volume up\n");
					change_volume(+10.0);
				}
				if (*(ptr+1) == 'D') {
					// volume down
					//printf("Volume down\n");
					change_volume(-10.0);
				}
				if (*(ptr+1) == 'M') {
					// Toggle Mute
					//printf("Toggle Mute\n");
					toggle_mute();
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
