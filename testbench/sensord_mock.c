#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include "nmea_checksum.h"

#define PORT 4353
#define NMEA_SENTENCE_LENGTH 200


int reconnect_sock(int sock, int port)
{
  if (sock) close(sock);

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr,"\n Socket creation error \n");
    return sock;
  }

  struct sockaddr_in serv_addr;

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  // Convert IPv4 and IPv6 addresses from text to binary form
  if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    fprintf(stderr,"\nInvalid address/ Address not supported \n");
    return -1;
  }

  while (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr,"Connection Failed, trying again\n");
    sleep(1);
  }
  fprintf(stderr,"Connected\n");
  fflush(stderr);
  return sock;
}

/* returns number of strings replaced.
*/
int repl_str(char *line, const char *search, const char *replace)
{
  char *sp;
  int search_len = strlen(search);
  int replace_len = strlen(replace);
  int tail_len;
  int count = 0;

  while ((sp = strstr(line, search)) != NULL) {
    tail_len = strlen(sp+search_len);
    memmove(sp+replace_len,sp+search_len,tail_len+1);
    memcpy(sp, replace, replace_len);
    count += 1;
  }
  return count;
}

/*
 * calculates the milliseconds from a GPRMS time stamp
 */
int timestring2msec(char *time_string, char *date_string)
{
  static char first_date_string[12] = {'\0'};
  char ts[12], *p;
  unsigned int len;
  int msec = -1;
  int sec, min, h;
  float sec_frac;

  if (strcmp(time_string,"uninitialized") == 0) return -2;
  if ((len = strlen(time_string)) < 6) return -2;
  if (len+1 > sizeof(time_string));

  strcpy(ts,time_string);

  if ((p = strchr(ts,'.')) != NULL) {
    sscanf (p,"%f",&sec_frac);
    *p = '\0';
  } else {
    sec_frac = 0;
  }

  len = strlen(ts);

  if (len == 6) {
    sec = atoi(ts+4);
    *(ts+4) = '\n';
    min = atoi(ts+2);
    *(ts+2) = '\n';
    h = atoi(ts);
    msec = ((((((h * 60) + min) * 60) + sec) * 1000) + ((int)sec_frac * 1000));

    // deal with the case when we cross the 24h boundary
    if (strlen(date_string) == 6) { // received a valid date string
      int dsf = strlen(first_date_string);
      if (dsf == 0) { // no valid date string was seen before
        strcpy(first_date_string,date_string); // remember it
      } else if (dsf == 6) { // we already have a valid date string
        if (strcmp(date_string,first_date_string) == 0) {
          // date has not changed, we are happy
        } else { //date has changed
          // TODO: check if it really changed to the next day
          // Nobody flies for more than 24h ;-)
          msec += 24 * 60 * 60 * 1000;
        }
      }
    }

  }

  return msec;
}

void cutCRLF(char *sp)
{
  char c;

  while((c = *sp) != '\0') {
    if (c == '\n' || c == '\r') {
      *sp = '\0';
      return;
    }
    sp += 1;
  }
}

void pr_usage (const char* nm)
{
  fprintf (stderr,"Usage: %s [-x time_scale_factor] [-d] [-v] [-i nmea_stimulus_file_name]\n",nm);
  exit (1);
}

struct configuration {
  // the name of the executable
  const char *nm = NULL;
  int port = PORT;
  int sock = 0;
  bool verbose = false;
  const char *stimulus_file_name = NULL;
  // lets NMEA sentences through even if they aren't clean, e.g. checksum failur
  bool allow_dirty = false;
  // time_scale: 200 means 5x, 1000 means 1x, 100 means 10x speed, 0 means speed of light
  int time_scale = 1000;
  int msec_incr = 500;
};

int stimulus_generated(configuration *cfg)
{
  float te_var = 0.0f, te_incr;
  char buffer[1024] = {0};
  int sock_err = 0;

  srand (123);

  float statp=952.5;
  float dynp=567.7;
  te_incr = 0.013;
  int line_counter = 0;

  while (1) {
    te_var += te_incr;
    if (te_var > 2.5) te_incr = -0.11;
    if (te_var < -3.1) te_incr = 0.18;

    sprintf(buffer,"$POV,E,%1.2f,P,%1.2f,Q,%1.2f",te_var,
            statp + 0.033 * (float)(rand()/(RAND_MAX/100)),
            dynp + 2.1 * (float)(rand()/(RAND_MAX/100)));
    append_nmea_checksum(buffer);
    strcat(buffer,"\n");

    do {
      line_counter += 1;
      sock_err = send(cfg->sock, buffer, strlen(buffer)+1, 0);
      if (sock_err >= 0) {
        if (cfg->verbose) printf ("%d: %s",line_counter,buffer);
      } else {
        fprintf(stderr,"send failed, try reconnect\n");
        sleep(1);
        if ((cfg->sock = reconnect_sock(cfg->sock,cfg->port)) < 0)
          perror("Error creating socket\n");
        sock_err = 0;
      }
    } while (sock_err < 0);
    usleep(cfg->time_scale * cfg->msec_incr);
  }
// never gets here
  return -1;
}

int stimulus_from_file(configuration *cfg)
{
  char line_buf[NMEA_SENTENCE_LENGTH+2];
  char sentence[NMEA_SENTENCE_LENGTH+2];
  int sock_err = 0;
  char time_string[] = "uninitialized";
  char date_string[] = "uninitialized";
  char *pch;
  int t;
  int msec_now = 0;
  int msec_last = 0;
  int sleep_time = 0;
  bool time_sync = false;

  if (cfg->stimulus_file_name == NULL) {
    fprintf (stderr,"Input file name missing\n");
    pr_usage(cfg->nm);
  }

  FILE * fh = fopen (cfg->stimulus_file_name, "r");
  int Error = 1;
  int line_counter = -1;

  if (fh == NULL) {
    perror ("Error opening NMEA file for read");
    Error = 1;
  } else {
    Error = 0;
    line_counter = 0;
  }

  if (Error) exit(Error);

  if (cfg->verbose) {
    printf ("Using Stimulus: %s\n",cfg->stimulus_file_name);
  }

  while ( fgets (line_buf, NMEA_SENTENCE_LENGTH, fh) != NULL ) {
    line_buf[NMEA_SENTENCE_LENGTH+1] = '\0';
    line_counter += 1;
    cutCRLF(line_buf);
    strcpy(sentence,line_buf);

    if (cfg->allow_dirty || verify_nmea_checksum(sentence)) {

      if (strncmp(line_buf,"$GPRMC,",7) == 0) {
        repl_str(line_buf,",,",", ,");
        pch = strtok(line_buf,",*");
        // $GPRMC,110501.00,A,4827.26735,N,01156.29056,E,0.515,,180519,,,A*76

        int i = 0;
        while (pch != NULL) {
          // printf ("%d %d: %s\n",i,strlen(pch),pch);
          if (i == 1) strcpy(time_string,pch);
          if (i == 9) strcpy(date_string,pch);
          pch = strtok(NULL,",*");
          i += 1;
        }
        if (i-2 != 12) {
          fprintf(stderr,"GPRMC at line %d:  %s\n"
                  "Incorrect number of fields: should be 12 found %d\n",
                  line_counter,sentence,i-2);
        } else {
          // advance time
          t = timestring2msec(time_string,date_string);
          if (time_sync) {
            if (t > 0) {
              msec_now = t;
              sleep_time = msec_now -  msec_last;
            } else {
              msec_now += cfg->msec_incr;
              sleep_time = cfg->msec_incr;
              time_sync = false;
            }
          } else {
            sleep_time = cfg->msec_incr;
            if (t > 0) {
              msec_now = t;
              time_sync = true;
            } else {
              msec_now += cfg->msec_incr;
            }
          }
          usleep(cfg->time_scale * sleep_time);
          msec_last = msec_now;
        }
      }

      strcat(sentence,"\n");

      do {
        sock_err = send(cfg->sock, sentence, strlen(sentence)+1, 0);
        if (sock_err >= 0) {
          if (cfg->verbose) printf ("%d: %s",line_counter,sentence);
        } else {
          fprintf(stderr,"send failed, try reconnect\n");
          sleep(1);
          if ((cfg->sock = reconnect_sock(cfg->sock,cfg->port)) < 0) perror("Error creating socket\n");
          sock_err = 0;
        }
      } while (sock_err < 0);
    } else {
      fprintf(stderr,"NMEA Checksum Error at line %d: >%s<\n",line_counter,sentence);
    }

  }
  fclose(fh);
  return line_counter;
}

int main(int argc, char const *argv[])
{
  struct configuration cfg;

// Read through command-line arguments for options.
  cfg.nm = argv[0];
  for (int i = 0; i < argc; i++) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'i' : // standard NMEA log of FLARM and e-Vario written by XCSoar
        i += 1;
        if (i < argc) {
          cfg.stimulus_file_name = argv[i];
        } else {
          fprintf (stderr,"Input file name missing\n");
          return 1;
        }
        break;

      case 'x' : // speed up of replay
        i += 1;
        if (i < argc) {
          float x = atof(argv[i]);
          if ((x > 0.1) && (x <= 1002.0)) {
            cfg.time_scale = (int)((float)1000 / x);
          } else {
            fprintf (stderr,"time scale factor 0.1 ... 10.0\n");
            return 1;
          }
        } else {
          fprintf (stderr,"time scale missing\n");
          return 1;
        }
        break;

      case 'd' :
        cfg.allow_dirty = true;
        break;

      case 'v' :
        cfg.verbose = true;
        break;

      default:
        fprintf (stderr,"Invalid option: %s\n",argv[i]);
        pr_usage(cfg.nm);
        return 2;
      }
    }
  }

  // ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  if (cfg.verbose) printf("Client on Socket %d\n",cfg.port);
  if ((cfg.sock = reconnect_sock(cfg.sock,cfg.port)) < 0) perror("Error creating socket\n");

  int line_counter = 0;
  if (cfg.stimulus_file_name != NULL) line_counter = stimulus_from_file(&cfg);
  else line_counter = stimulus_generated(&cfg);


  close(cfg.sock);
  printf("Done with %d lines\n",line_counter);
}
