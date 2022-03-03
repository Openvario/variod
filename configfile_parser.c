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
#include "stf.h"

int cfgfile_parser(FILE *fp, t_vario_config *vario_config, t_polar *polar)
{
	char line[128];
	char tmp[128];

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
					sscanf(line, "%s %f", tmp, &(vario_config[vario].deadband_low));
				}

				// check for deadband_high
				if (strcmp(tmp,"deadband_high") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[vario].deadband_high));
				}

				// check for pulse_length
				if (strcmp(tmp,"pulse_length") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[vario].loval));
				}

				// check for pulse gain
				if (strcmp(tmp,"pulse_length_gain") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[vario].hival));
				}

				// check for pulse duty
				if (strcmp(tmp,"pulse_duty") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[vario].pulse_riseduty));
				}
				// check for pulse rise
				if (strcmp(tmp,"pulse_rise") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[vario].pulse_rise));
				}
				// check for pulse fall
				if (strcmp(tmp,"pulse_fall") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[vario].pulse_falli));
				}
				// check for base frequency positive
				if (strcmp(tmp,"base_freq_pos") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[vario].base_freq_pos));
				}

				// check for base frequency negative
				if (strcmp(tmp,"base_freq_neg") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[vario].base_freq_neg));
				}
				// check for frequency gain positive
				if (strcmp(tmp,"freq_gain_pos") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[vario].freq_gain_pos));
				}
				// check for frequency gain negative
				if (strcmp(tmp,"freq_gain_neg") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[vario].freq_gain_neg));
				}
				// check for stf_deadband_low
				if (strcmp(tmp,"stf_deadband_low") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[stf].deadband_low));
				}

				// check for stf_deadband_high
				if (strcmp(tmp,"stf_deadband_high") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[stf].deadband_high));
				}

				// check for stf_pulse_length
				if (strcmp(tmp,"stf_pulse_length") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[stf].loval));
				}

				// check for stf_pulse gain
				if (strcmp(tmp,"stf_pulse_length_gain") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[stf].hival));
				}

				// check for pulse duty
				if (strcmp(tmp,"stf_pulse_duty") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[stf].pulse_riseduty));
				}
				// check for pulse rise
				if (strcmp(tmp,"stf_pulse_rise") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[stf].pulse_rise));
				}
				// check for pulse fall
				if (strcmp(tmp,"stf_pulse_fall") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[stf].pulse_falli));
				}
				// check for base frequency positive
				if (strcmp(tmp,"stf_base_freq_pos") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[stf].base_freq_pos));
				}

				// check for base frequency negative
				if (strcmp(tmp,"stf_base_freq_neg") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[stf].base_freq_neg));
				}
				// check for frequency gain positive
				if (strcmp(tmp,"stf_freq_gain_pos") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[stf].freq_gain_pos));
				}
				// check for frequency gain negative
				if (strcmp(tmp,"stf_freq_gain_neg") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(vario_config[stf].freq_gain_neg));
				}
				// check for polar values
				if (strcmp(tmp,"polar_a") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(polar->a));
				}
				if (strcmp(tmp,"polar_b") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(polar->b));
				}
				if (strcmp(tmp,"polar_c") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(polar->c));
				}
				if (strcmp(tmp,"polar_w") == 0)
				{
					// get config data
					sscanf(line, "%s %f", tmp, &(polar->w));
				}
			}
		}
		for (int i=0;i<2;++i) {
			vario_config[i].loval=m_pi/vario_config[i].loval;
			vario_config[i].hival*=vario_config[i].loval*2.0;
			vario_config[i].pulse_riseduty+=vario_config[i].pulse_rise;
			vario_config[i].pulse_risedutyfall=vario_config[i].pulse_riseduty+vario_config[i].pulse_falli;
			vario_config[i].freq_gain_pos*=4/(float) RATE;
			vario_config[i].base_freq_pos*=4/(float) RATE;
			vario_config[i].base_freq_neg*=4/(float) RATE;
			vario_config[i].pulse_falli=1.0/vario_config[i].pulse_falli;
			vario_config[i].pulse_falliv=vario_config[i].pulse_falli*16383.5;
			vario_config[i].pulse_risei=1.0/vario_config[i].pulse_rise;
			vario_config[i].pulse_riseiv=vario_config[i].pulse_risei*16383.5;
		}
		return(1);
	}
	else
	{
		// no config file used
		return (0);
	}
}
