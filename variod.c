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
#include <math.h>
#include "audiovario.h"
#include "cmdline_parser.h"
#include "configfile_parser.h"
#include "stf.h"
#include "nmea_parser.h"
#include "def.h"


int connfd = 0;

int g_debug=1;

int g_foreground=0;

FILE *fp_console=NULL;
FILE *fp_config=NULL;

extern t_vario_config vario_config[2];
extern enum e_vario_mode vario_mode;

//this is the value to be synthesised: In vario mode it's the TE compensated
//climb/sink value, in STF it is a value correlating to the difference of STF,
//and current airspeed
extern float audio_val;

pthread_t tid_audio_update;
pthread_t tid_volume_control;

/**
* @brief Signal handler if variod will be interrupted
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

static void wait_for_XCSoar(int xcsoar_sock, sockaddr* s_xcsoar){
	while (connect(xcsoar_sock, s_xcsoar, sizeof(*s_xcsoar)) < 0) {
		fprintf(stderr, "failed to connect, trying again\n");
		fflush(stdout);
		sleep(1);
	}
}

void add_checksum(char* msg){
	int i=1;
	int cs=0;
	int len;

	len= strlen(msg);

	while (i<len && msg[i]!='*'){
		cs ^= msg[i];
		i++;
	}

	sprintf((char*) (msg+len), "%x",cs);

}

void print_runtime_config(t_vario_config *vario_config)
{
	// print actual used config
	fprintf(fp_console,"=========================================================================\n");
	fprintf(fp_console,"Runtime Configuration:\n");
	fprintf(fp_console,"----------------------\n");
	fprintf(fp_console,"Vario:\n");
	fprintf(fp_console,"  Deadband Low:\t\t\t%f\n",vario_config[vario].deadband_low);
	fprintf(fp_console,"  Deadband High:\t\t%f\n",vario_config[vario].deadband_high);
	fprintf(fp_console,"  Pulse Pause Length:\t\t%d\n",vario_config[vario].pulse_length);
	fprintf(fp_console,"  Pulse Pause Length Gain:\t%f\n",vario_config[vario].pulse_length_gain);
	fprintf(fp_console,"  Base Frequency Positive:\t%f\n",vario_config[vario].base_freq_pos);
	fprintf(fp_console,"  Base Frequency Negative:\t%f\n",vario_config[vario].base_freq_neg);
	fprintf(fp_console,"Speed to fly:\n");
	fprintf(fp_console,"  Deadband Low:\t\t\t%f\n",vario_config[stf].deadband_low);
	fprintf(fp_console,"  Deadband High:\t\t%f\n",vario_config[stf].deadband_high);
	fprintf(fp_console,"  Pulse Pause Length:\t\t%d\n",vario_config[stf].pulse_length);
	fprintf(fp_console,"  Pulse Pause Length Gain:\t%f\n",vario_config[stf].pulse_length_gain);
	fprintf(fp_console,"  Base Frequency Positive:\t%f\n",vario_config[stf].base_freq_pos);
	fprintf(fp_console,"  Base Frequency Negative:\t%f\n",vario_config[stf].base_freq_neg);
	fprintf(fp_console,"=========================================================================\n");
}

int main(int argc, char *argv[])
{
	int listenfd = 0;

	// socket communication
	int xcsoar_sock;
	struct sockaddr_in server, s_xcsoar;
	int c , read_size;
	char client_message[2001];
	int nFlags;
	t_sensor_context sensors;
	t_polar polar;
	float v_sink_net, ias, stf_diff;

	// for daemonizing
	pid_t pid;
	pid_t sid;

	//parse command line arguments
	cmdline_parser(argc, argv);

	//ignore sigpipe (i.e. XCSoar went offline)
	signal(SIGPIPE, SIG_IGN);

	// init vario config structure
	init_vario_config();
	initSTF();

	//set Polar to default values (ASW24)
	polar.a = POL_A;
	polar.b = POL_B;
	polar.c = POL_C;
	polar.w = POL_W;

	setMC(1.0);

	// read config file
	// get config file options
	if (fp_config != NULL)
		cfgfile_parser(fp_config, (t_vario_config*) &vario_config,&polar);

	setPolar(polar.a,polar.b,polar.c,polar.w);
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
		fp_console = fopen("variod.log","w+");
		setbuf(fp_console, NULL);
		stderr = fp_console;
	}

	// all filepointers setup -> print config
	print_runtime_config(&vario_config[vario_mode]);

	// setup and start pcm player
	start_pcm();

	while(1) {
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
		wait_for_XCSoar(xcsoar_sock, (struct sockaddr*)&s_xcsoar);
		// make socket to XCsoar non-blocking
		fcntl(xcsoar_sock, F_SETFL, O_NONBLOCK);

		//enable vario sound
		vario_unmute();

		//Receive a message from sensord and forward to XCsoar
		while ((read_size = recv(connfd , client_message , 2000, 0 )) > 0 )
		{
			// terminate received buffer
			client_message[read_size] = '\0';

			parse_NMEA_sensor(client_message, &sensors,xcsoar_sock);

			//get the TE value from the message
			ias = getIAS(sensors.q);
			switch(vario_mode){
				case vario:
					set_audio_val(sensors.e);
				break;
				case stf:
					//sensors.s=100;

					v_sink_net=getNet( -sensors.e, ias);
					stf_diff=ias-getSTF(v_sink_net);

					if (stf_diff >=0)  set_audio_val(sqrt(stf_diff));
					else  set_audio_val(-sqrt(-stf_diff));


				break;
			}
			//Send the message back to client
			//printf("SendNMEA: %s",client_message);
			// Send NMEA string via socket to XCSoar
			if (send(xcsoar_sock, client_message, strlen(client_message), 0) < 0)
			{
				if (errno==EPIPE){
					fprintf(stderr,"XCSoar went offline, waiting\n");

					//reset socket
					close(xcsoar_sock);
					vario_mute();
					xcsoar_sock = socket(AF_INET, SOCK_STREAM, 0);
					if (xcsoar_sock == -1)
						fprintf(stderr, "could not create socket\n");

					wait_for_XCSoar(xcsoar_sock,(struct sockaddr*)&s_xcsoar);

					// make socket to XCsoar non-blocking
					fcntl(xcsoar_sock, F_SETFL, O_NONBLOCK);
					break;

				} else {
					fprintf(stderr, "send failed\n");
				}
			}
			// check if there is communication from XCSoar to us
			if ((read_size = recv(xcsoar_sock , client_message , 2000, 0 )) > 0 )
			{
				// we got some message
				// terminate received buffer
				client_message[read_size] = '\0';

				ddebug_print("Message from XCSoar: %s\n",client_message);

				// parse message from XCSoar
				parse_NMEA_command(client_message,xcsoar_sock);

			}

		}

		// connection dropped cleanup
		fprintf(fp_console, "Connection dropped\n");
		fflush(fp_console);

		close(xcsoar_sock);
		close(connfd);
	}
	return 0;
}



