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
#include <sys/un.h>
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
#include "log.h"

extern t_vario_config vario_config[2];
extern enum e_vario_mode vario_mode;

static void wait_for_XCSoar(int xcsoar_sock, struct sockaddr* s_xcsoar){
	while (connect(xcsoar_sock, s_xcsoar, sizeof(*s_xcsoar)) < 0) {
		fprintf(stderr, "failed to connect, trying again\n");
		fflush(stdout);
		sleep(1);
	}
}

static void print_runtime_config(t_vario_config *vario_config)
{
	// print actual used config
	fprintf(stderr,"=========================================================================\n");
	fprintf(stderr,"Runtime Configuration:\n");
	fprintf(stderr,"----------------------\n");
	fprintf(stderr,"Vario:\n");
	fprintf(stderr,"  Deadband Low:\t\t\t%f\n",vario_config[vario].deadband_low);
	fprintf(stderr,"  Deadband High:\t\t%f\n",vario_config[vario].deadband_high);
	fprintf(stderr,"  Pulse Pause Length:\t\t%d\n",vario_config[vario].pulse_length);
	fprintf(stderr,"  Pulse Pause Length Gain:\t%f\n",vario_config[vario].pulse_length_gain);
	fprintf(stderr,"  Base Frequency Positive:\t%f\n",vario_config[vario].base_freq_pos);
	fprintf(stderr,"  Base Frequency Negative:\t%f\n",vario_config[vario].base_freq_neg);
	fprintf(stderr,"Speed to fly:\n");
	fprintf(stderr,"  Deadband Low:\t\t\t%f\n",vario_config[stf].deadband_low);
	fprintf(stderr,"  Deadband High:\t\t%f\n",vario_config[stf].deadband_high);
	fprintf(stderr,"  Pulse Pause Length:\t\t%d\n",vario_config[stf].pulse_length);
	fprintf(stderr,"  Pulse Pause Length Gain:\t%f\n",vario_config[stf].pulse_length_gain);
	fprintf(stderr,"  Base Frequency Positive:\t%f\n",vario_config[stf].base_freq_pos);
	fprintf(stderr,"  Base Frequency Negative:\t%f\n",vario_config[stf].base_freq_neg);
	fprintf(stderr,"=========================================================================\n");
}

int main(int argc, char *argv[])
{
	// socket communication
	int xcsoar_sock;
	struct sockaddr_in s_xcsoar;
	int read_size;
	char client_message[2001];
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
	if (fp_config != NULL) {
		cfgfile_parser(fp_config, (t_vario_config*) &vario_config,&polar);
		fclose(fp_config);
	}

	setPolar(polar.a,polar.b,polar.c,polar.w);

	// set server address and port for listening
	struct sockaddr_un sensord_address;
	sensord_address.sun_family = AF_LOCAL;
	strcpy(sensord_address.sun_path, "/run/sensord.socket");
	const size_t sensord_address_size = sizeof(sensord_address) - sizeof(sensord_address.sun_path) + strlen(sensord_address.sun_path);

	// check if we are a daemon or stay in foreground
	if (!g_foreground)
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
		int null_fd = open("/dev/null", O_RDONLY);
		if (null_fd >= 0) {
			dup2(null_fd, STDIN_FILENO);
			close(null_fd);
		}

		//open file for log output
		int log_fd = open("variod.log", O_CREAT|O_WRONLY|O_TRUNC,
				  0666);
		if (log_fd >= 0) {
			dup2(log_fd, STDOUT_FILENO);
			dup2(log_fd, STDERR_FILENO);
			close(log_fd);
		}
	}

	// all filepointers setup -> print config
	print_runtime_config(&vario_config[vario_mode]);

	// setup and start pcm player
	start_pcm();

	while(1) {
		// connect to sensord
		fprintf(stderr,"Connecting to sensord...\n");
		fflush(stderr);

		const int sensord_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
		if (sensord_fd == -1) {
			printf("Could not create socket");
			return EXIT_FAILURE;
		}

		if (connect(sensord_fd, (struct sockaddr *)&sensord_address, sensord_address_size) < 0)
		{
			fprintf(stderr, "connect to sensord failed");
			close(sensord_fd);
			sleep(1);
			continue;
		}

		fprintf(stderr, "Connection to sensord established\n");

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
		while ((read_size = recv(sensord_fd , client_message , 2000, 0 )) > 0 )
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
		fprintf(stderr, "Connection dropped\n");
		fflush(stderr);

		close(xcsoar_sock);
		close(sensord_fd);
	}
	return 0;
}



