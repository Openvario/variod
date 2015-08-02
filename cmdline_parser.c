/*  
	varioapp -  - http://www.openvario.org/
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

#include "version.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>


extern int g_debug;
//extern int g_log;

extern int g_foreground;

extern FILE *fp_console;
extern FILE *fp_config;

void cmdline_parser(int argc, char **argv){

	// locale variables
	int c;
	char config_filename[50];
	
	const char* Usage = "\n"\
    "  -v              print version information\n"\
	"  -f              don't daemonize, stay in foreground\n"\
	"  -c [filename]   use config file [filename]\n"\
	"  -d[n]           set debug level. n can be [1..2]. default=1\n"\
	"\n";
	
	// check commandline arguments
	while ((c = getopt (argc, argv, "vc:d::f")) != -1)
	{
		switch (c) {
			case 'v':
				printf("varioapp V%c.%c RELEASE %c build: %s %s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_RELEASE,  __DATE__, __TIME__);
				printf("varioapp  Copyright (C) 2014  see AUTHORS on www.openvario.org\n");
				printf("This program comes with ABSOLUTELY NO WARRANTY;\n");
				printf("This is free software, and you are welcome to redistribute it under certain conditions;\n"); 
				break;
			
			case 'c':
				// use config file
				if (optarg == NULL)
				{
					printf("Missing option for -c\n");
					printf("Exiting ...\n");
					exit(EXIT_FAILURE);
				}
				else
				{
					strcpy(config_filename, optarg);
					printf("!! Using config file %s !!\n", config_filename);
					
					// Open the fp to config file
					fp_config = fopen(config_filename,"r");
					
					//check if config file opened ok ...
					if( fp_config == NULL)
					{
						printf("Error opening config file: %s\n", config_filename);
						printf("Exiting ...\n");
						exit(EXIT_FAILURE);
					}
				}
				break;
			case 'd':
				if (optarg == NULL)
				{
					g_debug = 1;
				}
				else
					g_debug = atoi(optarg);
				
				printf("!! DEBUG LEVEL %d !!\n",g_debug);
				break;
				
			case 'f':
				// don't daemonize
				printf("!! STAY in foreground !!\n");
				g_foreground = 1;
				break;
				
			case '?':
				printf("Unknow option %c\n", optopt);
				printf("Usage: sensord [OPTION]\n%s",Usage);
				printf("Exiting ...\n");
				exit(EXIT_FAILURE);
				break;
		}
	}
}
	