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
#include "stf.h"
#include "nmea_parser.h"
#include "def.h"
#include "nmea_checksum.h"


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

static void wait_for_XCSoar(int xcsoar_sock, sockaddr* s_xcsoar)
{
	while (connect(xcsoar_sock, s_xcsoar, sizeof(*s_xcsoar)) < 0) {
		fprintf(stderr, "failed to connect, trying again\n");
		fflush(stdout);
		sleep(1);
	}
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
	fprintf(fp_console,"  Base Frequency Positive:\t%d\n",vario_config[vario].base_freq_pos);
	fprintf(fp_console,"  Base Frequency Negative:\t%d\n",vario_config[vario].base_freq_neg);
	fprintf(fp_console,"Speed to fly:\n");
	fprintf(fp_console,"  Deadband Low:\t\t\t%f\n",vario_config[stf].deadband_low);
	fprintf(fp_console,"  Deadband High:\t\t%f\n",vario_config[stf].deadband_high);
	fprintf(fp_console,"  Pulse Pause Length:\t\t%d\n",vario_config[stf].pulse_length);
	fprintf(fp_console,"  Pulse Pause Length Gain:\t%f\n",vario_config[stf].pulse_length_gain);
	fprintf(fp_console,"  Base Frequency Positive:\t%d\n",vario_config[stf].base_freq_pos);
	fprintf(fp_console,"  Base Frequency Negative:\t%d\n",vario_config[stf].base_freq_neg);
	fprintf(fp_console,"=========================================================================\n");
}

int create_xcsoar_connection()
{
	// Open Socket for TCP/IP communication to XCSoar
	int xcsoar_sock;
	struct sockaddr_in s_xcsoar;
	xcsoar_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (xcsoar_sock == -1){
		fprintf(stderr, "could not create socket\n");
		return -1;
	}	  
	s_xcsoar.sin_addr.s_addr = inet_addr("127.0.0.1");
	s_xcsoar.sin_family = AF_INET;
	s_xcsoar.sin_port = htons(4352);

	// try to connect to XCSoar
	wait_for_XCSoar(xcsoar_sock, (struct sockaddr*)&s_xcsoar);
	// make socket to XCsoar non-blocking
	fcntl(xcsoar_sock, F_SETFL, O_NONBLOCK);
	return xcsoar_sock;

}

/*****************************************
 * @brief puts a chain of NMEA sentences into a clean state and saves incomplete lines
 * Inputs:
 *	*msg points to the first position of the sentences
 *	len is the total number of bytes of all sentences
 *	*incompl_sentence points to a buffer where an
 *	  incomplete sentence shall be saved
 *	  NULL pointer means: don't save
 *	max_incompl_sentence length of the incompl_sentence buffer
 * Return value:
 *	number of bytes processd total length of sentences + one
 *	seperator after each sentence.
 *	if the incomplete sentence wasn't save, n includes the length
 *	of the latter.
 *
 * Input is a concatination of sentences separated
 * by separating char(s). Separating chars are '\n' '\r' '\0'.
 * Separating char(s) are replaced by '\0'.
 * Multiple consecutive Separating chars are reduced to one '\0'.
 * len is the total number of bytes including all and the final
 * separating chars.
 * The return value is the total number of bytes after the
 * operation. That includes the uniquified seperators and the
 * terminating '\0' at the end.
 * The result is a "clean" concatination of sentences
 * separated by single '\0'.
 * It may save incomplete sentences (for later use) in case
 ******************************************/
int multi_sentence_clean(char *msg,int len, char *incompl_sentence, unsigned max_incompl_sentence)
{
	char *s = msg;
	char *d = msg;
	char *last_sentence_start = msg;
	int n = 0;
	int multi_seperators = 0;

	//initialize to 0 lenght string
	if (incompl_sentence != NULL) *incompl_sentence = '\0';

	for (int i=0; i < len; i++, s++) {
		switch (*s) {
			case '\n':
			case '\r':
			case '\0' :
				if (multi_seperators == 0) {
					// keep only one seperator
					*d++ = '\0';
					n++;
					// remember where the next sentence starts
					last_sentence_start = d;
				}
				multi_seperators++;
			break;

			default:
				*d++ = *s;
				n++;
				multi_seperators = 0;
			break;
		}
	}

	// terminate it just in case last sentence is incomplete
	*d = '\0';

	// save the incomplete line from the end of the recieve buffer
	if (multi_seperators == 0) {
		// this means the last sentence wasn't terminated!
		ddebug_print("\nIncomplete sentence received! %d of %d\n",n,len);
		if (last_sentence_start != NULL)
			ddebug_print(">%s<\n",last_sentence_start);
		if ((incompl_sentence != NULL)
			&& (strlen(last_sentence_start) < max_incompl_sentence)) {
			// we save the incomplete sentence for later
			// and adjust the number of processed bytes
			strcpy(incompl_sentence,last_sentence_start);
			n = last_sentence_start-msg;
			ddebug_print("Saved\n");
		} else {
			ddebug_print("NOT Saved\n");
		}
	}
	return n;
}

int main(int argc, char *argv[])
{
	int listenfd = 0;

	// socket communication
	int xcsoar_sock;
	struct sockaddr_in sensor_server;
	int c, read_size;
	int sendbytes;
	char client_message[2001];
	char temp_buf[200];
	int nFlags;
	t_sensor_context sensors;
	t_polar polar;
	float v_sink_net, ias, stf_diff;

	// for incomplete lines
	char save_sensor_data[200];
	char save_xcsoar_cmd[200];
	unsigned int incompl_sens_data_length = 0;
	unsigned int incompl_xcs_cmd_length = 0;

	// communication with XCSoar
	bool request_current_settings = true;

	// for debug purposes
	unsigned sensor_sentence_count = 0;

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
	if (listenfd == -1) {
		printf("Could not create socket");
	}
	printf("Socket created ...\n");

	// set server address and port for listening
	sensor_server.sin_family = AF_INET;
	sensor_server.sin_addr.s_addr = inet_addr("127.0.0.1");
	sensor_server.sin_port = htons(4353);

	nFlags = fcntl(listenfd, F_GETFL, 0);
	nFlags |= O_NONBLOCK;
	fcntl(listenfd, F_SETFD, nFlags);

	//Bind listening socket
	if( bind(listenfd,(struct sockaddr *)&sensor_server, sizeof(struct sockaddr_in)) < 0) {
		//print the error message
		fprintf(stderr,"bind failed. Error");
		return 1;
	}

	listen(listenfd, 10);

	// check if we are a daemon or stay in foreground
	if (g_foreground == 1) {
		// create CTRL-C handler
		//signal(SIGINT, INThandler);
		// open console again, but as file_pointer
		fp_console = stdout;
		stderr = stdout;

		// close the standard file descriptors
		close(STDIN_FILENO);
		//close(STDOUT_FILENO);
		close(STDERR_FILENO);
	} else {
		// implement handler for kill command
		printf("Daemonizing ...\n");
		pid = fork();

		// something went wrong when forking
		if (pid < 0) {
			exit(EXIT_FAILURE);
		}

		// we are the parent
		if (pid > 0) {
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
  
	// connect to xcsoar
	xcsoar_sock = create_xcsoar_connection();
		
	while(1) {
		//Accept and incoming connection
		fprintf(fp_console,"Waiting for incoming connections after %d sentences from sensors ...\n",
			sensor_sentence_count);
		fflush(fp_console);

		c = sizeof(struct sockaddr);
		//accept connection from an incoming client
		connfd = accept(listenfd, (struct sockaddr *)&sensor_server, (socklen_t*)&c);
		if (connfd < 0) {
			fprintf(stderr, "accept failed after %d sentences from sensors\n",sensor_sentence_count);
			return 1;
		}

		fprintf(fp_console, "Connection accepted, reset sensor sentence counter\n");
		sensor_sentence_count = 0;

		// Socket is connected

		//enable vario sound
		vario_unmute();
		// get current values for Polar, MC, ... from XCSoar
		// since we might have started after XCSoar was running for a while
		request_current_settings = true;

		// initialize the machanism to save incomplete lines from XCSoar
		incompl_xcs_cmd_length = 0;
		*save_xcsoar_cmd = '\0';

		// initialize the machanism to save incomplete lines from sensord
		incompl_sens_data_length = 0;
		*save_sensor_data = '\0';

		// receive a message from sensord and forward to XCsoar
		while ((read_size = recv(connfd, client_message + incompl_sens_data_length,
			sizeof(client_message) - incompl_sens_data_length - 1, 0 )) > 0 ) {
			read_size += incompl_sens_data_length;
			// client_message may hold several sentences separated by one or more '\0' '\n' '\r'.
			// If there is incomplete line: saved to save_sensor_data
			read_size = multi_sentence_clean(client_message,read_size,
				save_sensor_data,sizeof(save_sensor_data));

			// this will only show the first sentence
			ddebug_print("Received from sensors: %s and maybe more ...\n",client_message);

			// parse message from sensors
			// one sentence at a time
			for (int j=0; j < read_size;) {
				// take a copy because it'll be gobbled up
				strcpy(temp_buf,client_message + j);
				if (strlen(temp_buf) > 0) {
					// ddebug_print("parse from sensors %d >%s",sensor_sentence_count,temp_buf);
					// ddebug_print("%d bytes\n",sendbytes);
					// Send NMEA string via socket to XCSoar
					// XCSoar wants a '\n' at the end of the sentence
					strcat(temp_buf,"\n");
					//     not '\0' terminated!
					sendbytes=send(xcsoar_sock, temp_buf, strlen(temp_buf), 0);
					if (sendbytes < 0)
					{
						if (errno==EPIPE){
							fprintf(stderr,"XCSoar went offline, waiting\n");

							//reset socket mute vario and try reconnection
							close(xcsoar_sock);
							vario_mute();

							xcsoar_sock = create_xcsoar_connection();
							break;

						} else {
							fprintf(stderr, "send failed\n");
						}
					}
					sensor_sentence_count++;
					ddebug_print("%d: %s",sensor_sentence_count,temp_buf);
					// parseing will destroy the content, hence last in the chain
					parse_NMEA_sensor(temp_buf, &sensors);
					j += (strlen(client_message+j) + 1);
				} else {
					// if there is garbage in the queue
					j += 1; }
			}

			//get the TE value from the message
			ias = getIAS(sensors.q);
			switch(vario_mode) {
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

			// see if there is a non-zero length fraction of a sentence
			// from the previous receive
			incompl_xcs_cmd_length = strlen(save_xcsoar_cmd);
			if (incompl_xcs_cmd_length > 0) {
				if (incompl_xcs_cmd_length < sizeof(save_xcsoar_cmd)) {
					// prepend this fraction into the readbuffer
					strcpy(client_message,save_xcsoar_cmd);
				} else {
					// too long for a meaningful fraction, forget it
					incompl_xcs_cmd_length = 0;
				}
			}

			// check if there is communication from XCSoar to us
			if ((read_size = recv(xcsoar_sock, client_message + incompl_xcs_cmd_length,
				sizeof(client_message) - incompl_xcs_cmd_length - 1, 0 )) > 0 ) {
				read_size += incompl_xcs_cmd_length;
				// we got some message
				// client_message may hold several sentences separated by one or more '\0' '\n' '\r'.
				// If there is incomplete line: saved to save_xcsoar_cmd
				read_size = multi_sentence_clean(client_message,read_size,
					save_xcsoar_cmd,sizeof(save_xcsoar_cmd));

				ddebug_print("Received from XCSoar: %s and maybe more ...\n",client_message);
				// parse message from XCSoar
				// one sentence at a time
				for (int j=0; j < read_size;) {
					if (strlen(client_message+j) > 0) {
						strcpy(temp_buf,client_message+j);
						ddebug_print("parse from XCSoar %d: >%s<\n",j,temp_buf);
						parse_NMEA_command(temp_buf);
						j += (strlen(client_message+j) + 1);
					} else { j += 1; }
				}
			}

			// we might see if we need to update settings from XCSoar
			if (request_current_settings) {
				strcpy(client_message,"$POV,?,RPO,MC"); // all we need for STF
				append_nmea_checksum(client_message);
				// XCSoar wants a '\n' at the end of the sentence
				strcat(client_message,"\n");
				//     not '\0' terminated!
				if (send(xcsoar_sock, client_message, strlen(client_message), 0) > 0) {
					debug_print("Request settings: %s",client_message);
					// successful sent: cancel the request
					request_current_settings = false;
				}
			}

			// see if there is a non-zero length fraction of the sentence
			incompl_sens_data_length = strlen(save_sensor_data);
			if (incompl_sens_data_length > 0) {
				if (incompl_sens_data_length < sizeof(save_sensor_data)) {
					// prepend this fraction into the readbuffer
					strcpy(client_message,save_sensor_data);
				} else {
					// too long for a meaningful fraction, forget it
					incompl_sens_data_length = 0;
				}
			}
		}
		debug_print("recv(connfd,...) return: %d\n",read_size);
		close(connfd);
	}

	// connection dropped cleanup
	fprintf(fp_console, "Connection dropped\n");	
	fflush(fp_console);
	close(xcsoar_sock);
	return 0;
}

