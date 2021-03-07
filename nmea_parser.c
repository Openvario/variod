#include "nmea_parser.h"
#include "def.h"
#include "utils.h"
#include <sys/types.h>
#include <sys/socket.h>

extern int g_debug;
extern int g_foreground;
extern FILE *fp_console;
static unsigned char status=0x26;

// Return a 0 if checksum is bad
// Return the location of the '*' character if checksum is good.
int verify_checksum (char *ptr) {
	int i,j,k;

	for (i=j=0;((ptr[i]!=0) && (ptr[i]!='*'));++i) j^=ptr[i];
	k=j&0xf;
	if (ptr[i]!='*') return 0;
	if (k<10) {
		if (ptr[i+2] != '0'+k) return 0;
	} else { 
		if (ptr[i+2] != 'A'+k-10) return 0;
	}
	j=j>>4;
	if (j<10) {
		if (ptr[i+1] != '0'+j) return 0;
	} else {
		if (ptr[i+1] != 'A'+j-10) return 0;
	}
	return i;
}

void checksum (char *ptr) {
	int i,j,k;

	for (i=1,j=0,k=strlen(ptr);i<k;++i) j^=ptr[i];
	sprintf(ptr+k,"*%02X",j);
}

inline void tx_mesg (char *client_message, int xcsoar_sock) {

	checksum(client_message);
	strcat(client_message,"\r\n");
	if (send(xcsoar_sock, client_message, strlen(client_message), 0) < 0) 
		fprintf(stderr, "send failed\n");
}

void parse_NMEA_sensor(char* message, t_sensor_context* sensors, int xcsoar_sock)
{ 
	static char buffer[2001];
	char del1[2]="$",del2[2]=",";
	char *ptr=NULL,*ptr2,*last;
	char tick=1;
	char mesg[120];
	int i;
	static unsigned char counter=255;

	// copy string and initialize strtok function
	if (status) {
		if (strchr(buffer,'$')==NULL) counter++;
		if (counter==0) {
			strcpy(mesg,"$POV,?");
			if (status&0x01) strcat(mesg,",POL");
			if (status&0x02) strcat(mesg,",RPO");
			if (status&0x04) strcat(mesg,",MC");
			if (status&0x08) strcat(mesg,",WL");
			if (status&0x10) strcat(mesg,",BU");
			if (status&0x20) strcat(mesg,",VAR");
			tx_mesg(mesg,xcsoar_sock);
			printf ("%s\n",mesg);
			counter=1;
		} 
	}
	buffer[2000]=0;
	if ((ptr2=strchr(buffer,'$'))!=NULL) {
		for (i=0;ptr2[i]!=0;++i) buffer[i]=ptr2[i];
		buffer[i]=0;
	}
	if (strlen(buffer)+strlen(message)>1999)
		strcpy(buffer,message);
	else strcpy(buffer+strlen(buffer),message); // Append the new sentence to the last
	ptr = strtok(buffer, del1); // Tokenize about '$'
	last=buffer;
	while (ptr != NULL) {
		last=ptr;
		tick=1;
		if ((*ptr=='P') && (*(ptr+1)=='O') && (*(ptr+2)=='V') && (*(ptr+3)==',')) { // If you begin the sentence correctly  
			if (verify_checksum(ptr)) { // And checksum passes
				tick=0;
				ptr2=ptr+4; // Start looking after the $POV,
				ptr+=strlen(ptr)+1; // Move ptr to the next POV
				ptr2=strtok(ptr2,del2); // Tokenize about ','
				while (ptr2 != NULL) { // Repeat until you hit the next '$' (which has already been turned into a 0
					switch (*ptr2) {
						case 'E':
							// TE vario value
							// get next value
							ptr2 = strtok(NULL, del2); 
							sensors->e = atof(ptr2);
							break;
						case 'Q':
							// Airspeed value
							// get next value
							ptr2 = strtok(NULL, del2);
							sensors->q = atof(ptr2);
							break;
						default: break;
					}
					ptr2=strtok(NULL,del2);
				}
				ptr=strtok(ptr, del1); // Successfully processed a sentence, so get back to tokenizing about $
			} else ptr=strtok(NULL, del1); // Didn't find a valid checksum, so move on 
		} else ptr=strtok(NULL,del1); // Didn't find a valid sentence start, so move on
	}
	if (tick==1) {
		for (i=0;((i<2000)&&(last[i]!=0));++i) buffer[i]=last[i];
		if (!(last[i])) buffer[i]=0;
	} else buffer[0]=0; // Save the last sentence if not complete
}

void parse_NMEA_command(char* message, int xcsoar_sock)
{
	static char buffer[2001];
	char del1[2]="$",del2[2]=",";
	char *ptr=NULL,*ptr2,*last;
	char tick=2;
	char response[80];
	int i;

	t_polar polar;
	t_polar *ppolar;
	static float fvals[NUM_FV];
	// copy string and initialize strtok function

	buffer[2000]=0;
	if ((ptr2=strchr(buffer,'$'))!=NULL) {
		for (i=0;ptr2[i]!=0;++i) buffer[i]=ptr2[i];
		buffer[i]=0;
	}
	if (strlen(buffer)+strlen(message)>1999) 
		strcpy(buffer,message);
	else strcpy(buffer+strlen(buffer),message); // Append the new sentence to the last
	ptr = strtok(buffer, del1); // Tokenize about '$'
	last=buffer;
	while (ptr != NULL) {
		last=ptr;
		tick=1;
		if ((*ptr=='P') && (*(ptr+1)=='O') && (*(ptr+2)=='V') && (*(ptr+3)==',')) { // If you begin the sentence correctly  
			if (verify_checksum(ptr)) { // And checksum passes
				tick=0;
				ptr2=ptr+4; // Start looking after the $POV
				ptr+=strlen(ptr)+1; // Move ptr to the next POV
				ptr2=strtok(ptr2,del2); // Tokenize about ','
				while (ptr2 != NULL) { // Repeat until you hit the next '$'  
					switch (*ptr2) {
						case 'C':
							// Command sentence
							// get next value
							ptr2 = strtok(NULL, del2);
							switch (*ptr2) {
								case 'M':
									if (*(ptr2+1) == 'C') {
										//Set McCready
										status&=(0x04^0xff);
										ptr2 = strtok(NULL, del2);
										setMC(atof(ptr2));
										debug_print("Get McCready Value: %f\n",atof(ptr2));
									}
									break;
								case 'W':
									if (*(ptr2+1) == 'L') {
										//Set Wingload/Ballast
										status&=(0x08^0xff);
										ptr2 = strtok(NULL, del2);
										setBallast(atof(ptr2));
										debug_print("Get Ballast Value: %f\n",atof(ptr2));
									}
									break;
								case 'B':
									if (*(ptr2+1) == 'U') {
										//Set Bugs
										status&=(0x10^0xff);
							 			ptr2 = strtok(NULL, del2);
										setDegradation(atof(ptr2));
										debug_print("Get Bugs Value: %f\n",atof(ptr2));
									}
									break;
								case 'P':
									//Set Polar
									// depricated
									if (*(ptr2+1) == 'O' &&  *(ptr2+2) == 'L') {
										ptr2 = strtok(NULL, del2);
										polar.a=atof(ptr2);
										ptr2 = strtok(NULL, del2);
										polar.b=atof(ptr2);
										ptr2 = strtok(NULL, del2);
										polar.c=atof(ptr2);
										ptr2 = strtok(NULL, del2);
										polar.w=atof(ptr2);
										setPolar(polar.a, polar.b, polar.c, polar.w);
										status&=(0x01^0xff);
									}
									break;
								case 'R':
									//Set Real Polar
									printf ("%s\n",ptr2);
									if (*(ptr2+1) == 'P' &&  *(ptr2+2) == 'O') {
										if (read_float_from_sentence(3,fvals,ptr2+4,del2)) {
											// convert horizontal speed from m/s to km/h
											status&=(0x02^0xff);
										  	polar.a=fvals[0]/(3.6*3.6);
											polar.b=fvals[1]/3.6;
											polar.c=fvals[2];
											setRealPolar(polar.a, polar.b, polar.c);
											debug_print("Get Polar RPO: %f, %f, %f\n",polar.a,polar.b,polar.c);
										}
									}
									break;
								case 'S':
									if (*(ptr2+1) == 'T' && *(ptr2+2) == 'F') {
										// Set STF Mode
										status&=(0x20^0xff);
										debug_print("Set STF Mode\n");
										printf ("Set STF Mode\n");
										set_vario_mode(stf);
									}
									break;
								case 'V':
									switch (*(ptr2+1)) {
										case 'U' : 
											// volume up
											debug_print("Volume up\n");
											change_volume(+10.0);
											break;
										case 'D' :
											// volume down
											debug_print("Volume down\n");
											change_volume(-10.0);
											break;
										case 'M' :
											// Toggle Mute
											debug_print("Toggle Mute\n");
											toggle_mute();
											break;
										case 'A' : 
					  						if  (*(ptr2+2) == 'R') {
												// Set Vario Mode
												status&=(0x20^0xff);
												debug_print("Set Vario Mode\n");
												printf ("Vario Mode\n");
												set_vario_mode(vario);
											}
											break;
										default : break;
									}
									break;
								default : break;
							}
							//ptr2=strtok(NULL,del2);
							break;
						case '?' :
							ptr2=strtok(NULL,del2);
							while (ptr2!=NULL) {
								switch (*ptr2) {
									case 'I' :
										if ((*(ptr2+1)=='P') && (*(ptr2+2)=='O')) {
											ppolar=getIdealPolar();
											sprintf (response,"$POV,C,IPO,%f,%f,%f,%f",ppolar->a,ppolar->b,ppolar->c,ppolar->w);
											tx_mesg (response,xcsoar_sock);
										}
										break;
									case 'R' :
										if ((*(ptr2+1)=='P') && (*(ptr2+2)=='O')) {
											ppolar=getPolar();
											sprintf (response,"$POV,C,RPO,%f,%f,%f",ppolar->a,ppolar->b,ppolar->c);
											tx_mesg (response,xcsoar_sock);
										}
										break;
									case 'M' : 
										if (*(ptr2+1)=='C') {
											sprintf (response,"$POV,C,MC,%f",getMC());
											tx_mesg (response,xcsoar_sock);
										}
										break;
									case 'W' :
										if (*(ptr2+1)=='L') {
											sprintf (response,"$POV,C,WL,%f",getBallast());
											tx_mesg (response,xcsoar_sock);
										}
										break;
									case 'B' :
										if (*(ptr2+1)=='U') {
											sprintf (response,"$POV,C,BU,%f",getDegradation());
											tx_mesg (response,xcsoar_sock);
										}
										break;
									default : break;
								}
								ptr2=strtok(NULL,del2);
							}
							break;
						default : break;
					}
					ptr2=strtok(NULL,del2);
				}
				ptr=strtok(ptr, del1); // Successfully processed a sentence, so get back to tokenizing about $
			} else ptr=strtok(NULL, del1); // Didn't find a valid checksum, so move on 
		} else ptr=strtok(NULL,del1); // Didn't find a valid sentence start, so move on
	}
	if (tick==1) {
		for (i=0;((i<2000)&&(last[i]!=0));++i) buffer[i]=last[i];
		if (!(last[i])) buffer[i]=0;
	} else buffer[0]=0; // Save the last sentence if not complete
}
