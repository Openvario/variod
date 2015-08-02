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

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h> 
#include <signal.h>
#include <pthread.h>
#include <syslog.h>
#include "audiovario.h"
#include "cmdline_parser.h"
#include "configfile_parser.h"

int connfd = 0;
float te = 0.0;
int g_debug=0;

int g_foreground=0;

FILE *fp_console=NULL;
FILE *fp_config=NULL;

extern t_vario_config vario_config;

pthread_t tid_audio_update;
pthread_t tid_volume_control;

float parse_TE(char* message)
{ 
	char *te_val;
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
			te_val = (char *) malloc(strlen(ptr));
			strncpy(te_val,ptr,strlen(ptr));
			te = atof(te_val);
			break;
			
			default:
			break;
		}
		// get next part of string
		ptr = strtok(NULL, delimiter);
	}
  return te;
}

void control_audio(char* message){
	
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
			
			case 'D':
			// Volume down by 10%
			// get next value
			change_volume(-10.0);
			break;
			
			case 'U':
			// Volume up by 10%
			// get next value
			change_volume(+10.0);
			break;
			
			case 'M':
			// Toggle Mute
			toggle_mute();
			break;
			
			default:
			break;
		}
		// get next part of string
		ptr = strtok(NULL, delimiter);
	}
}

/**
* @brief Signal handler if varioapp will be interrupted
* @param sig_num
* @return 
* 
* Signal handler for catching STRG-C singal from command line
* Closes all open files handles like log files
* @date 17.04.2014 born
*
*/ 
void INThandler(int sig)
{
     signal(sig, SIG_IGN);
     printf("Exiting ...\n");
	 fclose(fp_console);
     close(connfd);
	 exit(0);
}
     
void print_runtime_config(t_vario_config *vario_config)
{
	// print actual used config
	fprintf(fp_console,"=========================================================================\n");
	fprintf(fp_console,"Runtime Configuration:\n");
	fprintf(fp_console,"----------------------\n");
	fprintf(fp_console,"Vario:\n");
	fprintf(fp_console,"  Deadband Low:\t\t\t%f\n",vario_config->deadband_low);
	fprintf(fp_console,"  Deadband High:\t\t%f\n",vario_config->deadband_high);
	fprintf(fp_console,"  Pulse Pause Length:\t\t%d\n",vario_config->pulse_length);
	fprintf(fp_console,"  Pulse Pause Length Gain:\t%f\n",vario_config->pulse_length_gain);
	fprintf(fp_console,"  Base Frequency Positive:\t%d\n",vario_config->base_freq_pos);
	fprintf(fp_console,"  Base Frequency Negative:\t%d\n",vario_config->base_freq_neg);
	fprintf(fp_console,"=========================================================================\n");
	
}

int main(int argc, char *argv[])
{
	int listenfd = 0;
	// socket communication
	int xcsoar_sock;
    float te = 2.5;
	struct sockaddr_in server, s_xcsoar;
	int c , read_size;
	char client_message[2000];
	int err;
	int nFlags;

	// for daemonizing
	pid_t pid;
	pid_t sid;

	//parse command line arguments
	cmdline_parser(argc, argv);
	
	// init vario config structure
	init_vario_config(&vario_config);

	// read config file
	// get config file options
	if (fp_config != NULL)
		cfgfile_parser(fp_config, &vario_config);
	
	// setup server
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd == -1)
    {
        printf("Could not create socket");
    }
	printf("Socket created ...\n");	
	
	// set server address and port for listening
	server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(4353);
	
	nFlags = fcntl(listenfd, F_GETFL, 0);
	nFlags |= O_NONBLOCK;
	fcntl(listenfd, F_SETFD, nFlags);

	//Bind listening socket
    if( bind(listenfd,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        printf("bind failed. Error");
        return 1;
    }
	
	listen(listenfd, 10); 
	
	// check if we are a daemon or stay in foreground
	if (g_foreground == 1)
	{
		// create CTRL-C handler
		//signal(SIGINT, INThandler);
		
		// open console again, but as file_pointer
		fp_console = stdout;
		stderr = stdout;
		
		// close the standard file descriptors
		close(STDIN_FILENO);
		//close(STDOUT_FILENO);
		close(STDERR_FILENO);	
	}
	else
	{
		// implement handler for kill command
		printf("Daemonizing ...\n");
		pid = fork();
		
		// something went wrong when forking
		if (pid < 0) 
		{
			exit(EXIT_FAILURE);
		}
		
		// we are the parent
		if (pid > 0)
		{	
			exit(EXIT_SUCCESS);
		}
		
		// set umask to zero
		umask(0);
				
		/* Try to create our own process group */
		sid = setsid();
		if (sid < 0) {
			syslog(LOG_ERR, "Could not create process group\n");
			exit(EXIT_FAILURE);
		}
		
		// close the standard file descriptors
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		
		//open file for log output
		fp_console = fopen("varioapp.log","w+");
		setbuf(fp_console, NULL);
		stderr = fp_console;
	}
	
	// all filepointers setup -> print config
	print_runtime_config(&vario_config);
	
	// setup and start pcm player
		start_pcm();
		
	// create alsa update thread
	err = pthread_create(&tid_audio_update, NULL, &update_audio_vario, NULL);
	if (err != 0)
	{
		fprintf(stderr, "\ncan't create thread :[%s]", strerror(err));
	}
	else
	{
		fprintf(fp_console, "\n Thread 'update_audio_vario' created successfully\n");
	}
		

	
	while(1)
	{
		//Accept and incoming connection
		fprintf(fp_console,"Waiting for incoming connections...\n");
		fflush(fp_console);
		c = sizeof(struct sockaddr_in);

		//accept connection from an incoming client
		connfd = accept(listenfd, (struct sockaddr *)&s_xcsoar, (socklen_t*)&c);
		if (connfd < 0)
		{
			fprintf(stderr, "accept failed");
			return 1;
		}
	
		fprintf(fp_console, "Connection accepted\n");	
		
		// Socket is connected
		// Open Socket for TCP/IP communication to XCSoar
		xcsoar_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (xcsoar_sock == -1)
			fprintf(stderr, "could not create socket\n");
	  
		s_xcsoar.sin_addr.s_addr = inet_addr("127.0.0.1");
		s_xcsoar.sin_family = AF_INET;
		s_xcsoar.sin_port = htons(4352);

		// try to connect to XCSoar
		while (connect(xcsoar_sock, (struct sockaddr *)&s_xcsoar, sizeof(s_xcsoar)) < 0) {
			fprintf(stderr, "failed to connect, trying again\n");
			fflush(stdout);
			sleep(1);
		}
			
		// make socket to XCsoar non-blocking
		fcntl(xcsoar_sock, F_SETFL, O_NONBLOCK);
		
		//Receive a message from sensord and forward to XCsoar
		while ((read_size = recv(connfd , client_message , 2000, 0 )) > 0 )
		{
			// terminate received buffer
			client_message[read_size] = '\0';

			//get the TE value from the message 
			te=parse_TE(client_message);
			
			//Send the message back to client
			//printf(client_message);

			// Send NMEA string via socket to XCSoar
			if (send(xcsoar_sock, client_message, strlen(client_message), 0) < 0)
				fprintf(stderr, "send failed\n");
			
			// check if there is communication from XCSoar to us
			if ((read_size = recv(xcsoar_sock , client_message , 2000, 0 )) > 0 )
			{
				// we got some message
				// terminate received buffer
				client_message[read_size] = '\0';
				
				fprintf(fp_console, "from xcsoar: %s",client_message);

				// parse message from XCSoar
				control_audio(client_message);				
			}
			
		}
		
		// connection dropped cleanup
		fprintf(fp_console, "Connection dropped\n");	
		fflush(fp_console);
		
		close(xcsoar_sock);
		close(connfd);
		te=0.0;		
	}
    return 0;
}



