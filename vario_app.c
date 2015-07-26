#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 
#include <signal.h>
#include <pthread.h>
#include "audiovario.h"

int connfd = 0;
float te = 0.0;

pthread_t tid[2];

float parse_TE(char* message)
{ 
	char *te_pair, *te_val, *te_end;

	
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
			printf("Vario value: %f\n",te);
			
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
  char *cmd, *val, *end;
  float volume;

  cmd=strchr(message, 'L' );
  cmd+=sizeof(char)*2;  
  end=strchr(cmd,',');
  val=(char*) malloc((end-cmd)*sizeof(char));  
  volume=atof(val);
  change_volume(volume);
 
   
  cmd=strchr(message, 'T' );
  //if (cmd) toggle_mute();
}

void  INThandler(int sig)
{
     signal(sig, SIG_IGN);
     printf("Exiting ...\n");
     close(connfd);
	 exit(0);
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

	// create CTRL-C handler
	signal(SIGINT, INThandler);

	// setup server
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");


	// set server address and port for listening
	server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(4353);

	//Bind listening socket
    if( bind(listenfd,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    puts("bind done");

	listen(listenfd, 10); 

	//Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);

	//accept connection from an incoming client
    connfd = accept(listenfd, (struct sockaddr *)&s_xcsoar, (socklen_t*)&c);
    if (connfd < 0)
    {
        perror("accept failed");
        return 1;
    }
    puts("Connection accepted");

	
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

	// setup and start pcm player
    start_pcm();
	
	// create alsa update thread
	err = pthread_create(&(tid[0]), NULL, &update_audio_vario, NULL);
    if (err != 0)
	{
        printf("\ncan't create thread :[%s]", strerror(err));
	}
		
    else
        printf("\n Thread created successfully\n");

    
	//Receive a message from sensord and forward to XCsoar
	while (1)
	{
		if ((read_size = recv(connfd , client_message , 2000, 0 )) > 0 )
		{
			// terminate received buffer
			client_message[read_size] = '\0';

            //get the TE value from the message 
            te=parse_TE(client_message);
			
			//Send the message back to client
			printf(client_message);

			// Send NMEA string via socket to XCSoar
			if (send(xcsoar_sock, client_message, strlen(client_message), 0) < 0)
				fprintf(stderr, "send failed\n");

		}

	/*	if ((read_size = recv(xcsoar_sock , client_message , 2000, 0 )) > 0 )
		{
			// terminate received buffer
			client_message[read_size] = '\0';
			
			// debug
			printf(client_message);
			
			// parse message from XCSoar
			control_audio(client_message);
			
		}*/
		//update audio vario
        //update_audio_vario(te);
	}
    
	close(connfd);
    stop_pcm();
	
    return 0;
}
