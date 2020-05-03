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


int main(int argc, char *argv[])
{
  int loop_cnt = 0;
  int connfd = 0;
  int listenfd = 0;
  int port = 4353;
  struct sockaddr_in server;
  // struct sockaddr server;
  int c, read_size;
  char temp_buf[2001];
  int nFlags;
  bool verbose = false;



  for (int i = 0; i < argc; i++) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {

      case 'p' : // port number
        i += 1;
        if (i < argc) {
          port = strtol(argv[i],NULL,10);
        } else {
          fprintf (stderr,"port number extected after -p\n");
          return 1;
        }
        break;

      case 'v' :
        verbose = true;
        break;

      default:
        fprintf (stderr,"Invalid option: %s\n",argv[i]);
        return 2;
      }
    }
  }


  //ignore sigpipe (i.e. XCSoar went offline)
  signal(SIGPIPE, SIG_IGN);

  // setup server
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    fprintf(stderr,"Could not create socket");
    return -1;
  }
  fprintf(stderr,"Socket created ...\n");

  // set server address and port for listening
  memset(&server,0,sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = inet_addr("127.0.0.1");

  // Convert IPv4 and IPv6 addresses from text to binary form
  /**
  if(inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) <= 0) {
    fprintf(stderr,"Invalid address/ Address not supported \n");
    return -1;
  }
**/

  nFlags = fcntl(listenfd, F_GETFL, 0);
  nFlags |= O_NONBLOCK;
  fcntl(listenfd, F_SETFD, nFlags);

  //Bind listening socket
  // if( bind(listenfd,&server, sizeof(struct sockaddr_in)) < 0) {
  if( bind(listenfd,(struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
    //print the error message
    fprintf(stderr,"Bind failed. Error\n");
    return 1;
  }

  listen(listenfd, 10);
  fprintf(stderr,"Listening on port %d\n",port);

  while(1) {
    //Accept and incoming connection
    fprintf(stderr,"Waiting for incoming connections on port %d ...\n",port);
    fflush(stderr);
    c = sizeof(struct sockaddr_in);

    //accept connection from an incoming client
    connfd = accept(listenfd, (struct sockaddr *)&server, (socklen_t*)&c);
    if (connfd < 0) {
      fprintf(stderr, "accept failed on port %d\n",port);
      return 1;
    }

    fprintf(stderr, "Connection accepted\n");

    // Socket is connected

    int total_read = 0;
    //Receive a message and echo it on console
    while ((read_size = recv(connfd, temp_buf, sizeof(temp_buf), 0 )) > 0 ) {
      // terminate received buffer
      temp_buf[read_size] = '\0';
      total_read += read_size;
      for (int j=0; j < read_size;) {
	printf("%d: %s",loop_cnt++,&temp_buf[j]);
	j += (strlen(temp_buf + j) + 1);
      }
    }
    fprintf(stderr,"%d loops %d bytes\n",loop_cnt,total_read);
  }
  close(connfd);
  return 0;
}

