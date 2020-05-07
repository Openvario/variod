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

#define _POSIX_SOURCE

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>


int multi_sentence_clean(char *msg,int len)
{
  char *s = msg;
  char *d = msg;
  int n = 0;
  int mulit_null = 0;

  for (int i=0; i < len; i++, s++) {
    switch (*s) {
    case '\n':
    case '\r':
    case '\0' :
      if (mulit_null == 0) {
        *d++ = '\0';
        n++;
      }
      mulit_null++;
      break;

    default:
      *d++ = *s;
      n++;
      mulit_null = 0;
      break;
    }
  }
  return n;
}

void pr_usage(char *nm)
{
  fprintf (stderr,"%s -l [-v] [-e] -p port_number\n",nm);
  exit (1);
}


int main(int argc, char *argv[])
{
  int line_cnt = 1;
  int connfd = 0;
  int listenfd = 0;
  int port = -1;
  struct sockaddr_in server;
  // struct sockaddr server;
  int c, read_size;
  char temp_buf[2001];
  int nFlags;
  bool verbose = false;
  bool for_ever = false;
  bool listen_mode = false;

  if (argc < 3) pr_usage(argv[0]);

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

      case 'l' :
        listen_mode = true;
        break;

      case 'v' :
        verbose = true;
        break;

      case 'e' :
        for_ever = true;
        break;

      default:
        fprintf (stderr,"Invalid option: %s\n",argv[i]);
        pr_usage(argv[0]);
      }
    }
  }

  if (!listen_mode) {
    fprintf (stderr,"Listen mode is currently the only supported mode. "
             "Therefore -l option is mandatory.\n");
    pr_usage(argv[0]);
  }

  if (port == -1) {
    fprintf (stderr,"Port number missing\n");
    pr_usage(argv[0]);
  }

  //ignore sigpipe (i.e. went offline)
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
  // server.sin_addr.s_addr = inet_addr("127.0.0.1");

  // Convert IPv4 and IPv6 addresses from text to binary form
  if(inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) <= 0) {
    fprintf(stderr,"Invalid address/ Address not supported \n");
    return -1;
  }

  nFlags = fcntl(listenfd, F_GETFL, 0);
  nFlags |= O_NONBLOCK;
  fcntl(listenfd, F_SETFD, nFlags);

  //Bind listening socket
  if( bind(listenfd,(struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
    //print the error message
    fprintf(stderr,"Bind failed. Error\n");
    return 1;
  }

  listen(listenfd, 10);
  fprintf(stderr,"Listening on port %d\n",port);

  do {
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
    int net_total_read = 0;
    //Receive a message and echo it on console
    while ((read_size = recv(connfd, temp_buf, sizeof(temp_buf)+1, 0 )) > 0 ) {
      // terminate received buffer
      temp_buf[read_size] = '\0';
      total_read += read_size;
      read_size = multi_sentence_clean(temp_buf,read_size+1);
      net_total_read += read_size;

      // one sentence at a time
      for (int j=0; j < read_size;) {
        if (strlen(temp_buf+j) > 0) {
          if (verbose) printf("%d: ",line_cnt);
          printf("%s\n",temp_buf+j);
          j += (strlen(temp_buf+j) + 1);
          line_cnt += 1;
        } else {
          j += 1;
        }
      }
    }
    fprintf(stderr,"%d lines %d bytes (%d bytes net)\n",
            line_cnt-1,total_read,net_total_read);
  } while (for_ever);
  close(connfd);
  return 0;
}

