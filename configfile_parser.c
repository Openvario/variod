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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "configfile_parser.h"
#include "audiovario.h"

extern int g_debug;
extern FILE *fp_console;

int cfgfile_parser(FILE *fp, t_vario_config *vario_config)
{
	char line[70];
	char tmp[20];
		
	// is config file used ??
	if (fp)
	{
		// read whole config file
		while (! feof(fp))
		{
			// get line from config file
			fgets(line, 70, fp);
			//printf("getting line: '%s'\n", line);
			
			// check if line is comment
			if((!(line[0] == '#')) && (!(line[0] == '\n')))
			{
			
				// get first config tag
				sscanf(line, "%s", tmp);
				
				// check for deadband_low
				if (strcmp(tmp,"deadband_low") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &vario_config->deadband_low);	
				}
				
				// check for deadband_high
				if (strcmp(tmp,"deadband_high") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &vario_config->deadband_high);	
				}
				
				// check for pulse_length
				if (strcmp(tmp,"pulse_length") == 0)
				{
					// get config data
					sscanf(line, "%s %d", tmp, &vario_config->pulse_length);	
				}
				
				// check for pulse gain
				if (strcmp(tmp,"pulse_length_gain") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &vario_config->pulse_length_gain);	
				}
				
				// check for base frequency positive
				if (strcmp(tmp,"base_freq_pos") == 0)
				{
					// get config data
					sscanf(line, "%s %d", tmp, &vario_config->base_freq_pos);	
				}
				
				// check for base frequency negative
				if (strcmp(tmp,"base_freq_neg") == 0)
				{
					// get config data
					sscanf(line, "%s %d", tmp, &vario_config->base_freq_neg);	
				}				
			}
		}
		return(1);
	}
	else
	{
		// no config file used
		return (0);
	}
}