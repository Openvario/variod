#include "nmea_parser.h"
#include "log.h"
#include "utils.h"
#include <sys/types.h>
#include <sys/socket.h>

// Status bit shows what has been set:
// 0x01 = Polar (deprecated), please do not use
// 0x02 = Real Polar
// 0x04 = MacCready
// 0x08 = Wing Loading
// 0x10 = Bugs
// 0x20 = Vario / STF -- This is not supported in the OV protocol yet, please do not use
// 0x40 = Volume / Mute -- This is not supported in the OV protocol yet, please do not use

static unsigned char status=0x06; // Request Real Polar and MacCready on start up

// Return a 0 if checksum is bad
// Return the location of the '*' character if checksum is good.
static int verify_checksum (char *ptr) {
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

static void checksum (char *ptr) {
	int i,j,k;

	for (i=1,j=0,k=strlen(ptr);i<k;++i) j^=ptr[i];
	sprintf(ptr+k,"*%02X",j);
}

static void tx_mesg (char *client_message, int xcsoar_sock) {

	checksum(client_message);
	strcat(client_message,"\r\n");
	if (send(xcsoar_sock, client_message, strlen(client_message), 0) < 0)
		fprintf(stderr, "send failed\n");
}

void parse_NMEA_sensor(char* message, t_sensor_context* sensors, int xcsoar_sock)
{
	static char buffer[2001]; // A long buffer to hold multiple messages;
	char del1[2]="$",del2[2]=",";
	char *ptr,*ptr2,*last; // Pointers into the buffer
	char invalid_sentence=1;
	char status_mesg[40]; // Periodically transmitted after startup until status bits are cleared
	int i;
	static unsigned char counter=255; // Rolling counter to keep track of when to ask for parameters
	static unsigned char freshstart=1; // Indicates no previous good messages or partial messages

	if (status) { // Check if status bits are all clear
		if (strchr(buffer,'$')==NULL) { // Check if a sentence has been received
			if (++counter==0) { // Increment counter and if counter is at 0, then send out a request for parameters
				strcpy(status_mesg,"$POV,?");
				if (status&0x01) strcat(status_mesg,",POL");
				if (status&0x02) strcat(status_mesg,",RPO");
				if (status&0x04) strcat(status_mesg,",MC");
				if (status&0x08) strcat(status_mesg,",WL");
				if (status&0x10) strcat(status_mesg,",BU");
				tx_mesg(status_mesg,xcsoar_sock);
			}
		}
	}

	// Add new message to pre-existing buffer
	if ((strlen(buffer)+strlen(message)>1998) || freshstart) {
		if (strlen(message)<2000) {
			strcpy(buffer,message); // If the new message doesn't fit or it's a fresh start, replace the old message
			ptr=strchr(buffer,'$'); // Look for a '$'
			if (ptr!=NULL) { freshstart=0; ptr++; } else freshstart=1; // If not found, ignore the message, if it is start after it
		} else freshstart=1; // If the incoming message is too long, ignore it
	} else {
		strcpy(buffer+strlen(buffer),message); // else append the new message to the last
		ptr=buffer; // Start at the beginning of the buffer, a very good place to start
	}
	buffer[strlen(buffer)+1]=0; // strtok can overrun the 0 termination without a second 0

	if (!freshstart) { // If we have a string that contans a '$' in it...
		ptr = strtok(ptr, del1); // Tokenize about '$'
		last=ptr;
		while (ptr != NULL) {
			last=ptr; // Store the pointer, needed if it is a valid sentence
			invalid_sentence=1; // Let's assume it's not a valid sentence
			if ((*ptr=='P') && (*(ptr+1)=='O') && (*(ptr+2)=='V') && (*(ptr+3)==',')) { // If you begin the sentence correctly
				if (verify_checksum(ptr)) { // And checksum passes
					invalid_sentence=0; // Looks like it is a valid sentence
					ptr2=ptr+4; // Start looking after the $POV
					ptr+=strlen(ptr)+1; // Move ptr to the next POV
					ptr2=strtok(ptr2,del2); // Tokenize about ','
					while (ptr2 != NULL) { // Repeat until you hit the next '$' -- which has already been turned into a 0
						switch (*ptr2) {
							case 'E':
								// TE vario value
								// get next value
								ptr2 = strtok(NULL, del2); // Skip to the value
								if (ptr2!=NULL) sensors->e = atof(ptr2);
								break;
							case 'Q':
								// Airspeed value
								// get next value
								ptr2 = strtok(NULL, del2); // Skip to the value
								if (ptr2!=NULL) sensors->q = atof(ptr2);
								break;
							default: break;
						}
						ptr2=strtok(NULL,del2); // Skip to the next comma after the value, if present -- it likely won't be
					}
					ptr=strtok(ptr, del1); // Successfully processed a sentence, so get back to tokenizing about $
				} else ptr=strtok(NULL, del1); // Didn't find a valid checksum, so move on
			} else ptr=strtok(NULL,del1); // Didn't find a valid sentence start, so move on
		}
		if ((invalid_sentence) && (last!=NULL)) { // If the last sentence wasn't valid retain it for next time
			for (i=0;((i<(buffer+2000-last))&&(last[i]!=0));++i) buffer[i]=last[i]; // Copy it to the beginning of the buffer, can't use strcpy, results can be bad
			buffer[i]=0; // terminate the end of the message.
		} else buffer[0]=0; // Last sentence was valid, so start with an empty buffer for next time

	}
}

void parse_NMEA_command(char* message, int xcsoar_sock)
{
	static char buffer[2001]; // A long buffer to hold multiple messages;
	char del1[2]="$",del2[2]=",";
	char *ptr,*ptr2,*last; // Pointers into the buffer
	char invalid_sentence=1;
	char response[80]; // Response message to "$POV,?"
	int i;
	static char freshstart=1;
	t_polar polar;
	t_polar *ppolar;
	static float fvals[NUM_FV];

	// Add new message to pre-existing buffer
	if ((strlen(buffer)+strlen(message)>1998) || freshstart) {
		if (strlen(message)<2000) {
			strcpy(buffer,message); // If the new message doesn't fit or it's a fresh start, replace the old message
			ptr=strchr(buffer,'$'); // Look for a '$'
			if (ptr!=NULL) { freshstart=0; ptr++; } else freshstart=1; // If not found, ignore the message, if it is start after it
		} else freshstart=1; // If the incoming message is too long, ignore it
	} else {
		strcpy(buffer+strlen(buffer),message); // else append the new message to the last
		ptr=buffer; // Start at the beginning of the buffer, a very good place to start
	}
	buffer[strlen(buffer)+1]=0; // strtok can overrun the 0 termination without a second 0

	if (!freshstart) { // If we have a string that contans a '$' in it...
		ptr = strtok(ptr, del1); // Tokenize about '$'
		last=ptr;
		while (ptr != NULL) {
			last=ptr; // Store the pointer, needed if it is a valid sentence
			invalid_sentence=1; // Let's assume it's not a valid sentence
			if ((*ptr=='P') && (*(ptr+1)=='O') && (*(ptr+2)=='V') && (*(ptr+3)==',')) { // If you begin the sentence correctly
				if (verify_checksum(ptr)) { // And checksum passes
					invalid_sentence=0; // Looks like it is a valid sentence
					ptr2=ptr+4; // Start looking after the $POV
					ptr+=strlen(ptr)+1; // Move ptr to the next POV
					ptr2=strtok(ptr2,del2); // Tokenize about ','
					while (ptr2 != NULL) { // Repeat until you hit the next '$'
						switch (*ptr2) {
							case 'C':
								// Command sentence
								// get next value
								ptr2 = strtok(NULL, del2);
								if (ptr2!=NULL) switch (*ptr2) {
									case 'M':
										if (*(ptr2+1) == 'C') {
											//Set McCready
											if ((ptr2 = strtok(NULL, del2))!=NULL) {
												setMC(atof(ptr2));
												status&=(0x04^0xff);
												debug_print("Get McCready Value: %f\n",atof(ptr2));
											}
										}
										break;
									case 'W':
										if (*(ptr2+1) == 'L') {
											//Set Wingload/Ballast
											if ((ptr2 = strtok(NULL, del2))!=NULL) {
												setBallast(atof(ptr2));
												status &=(0x08^0xff);
												debug_print("Get Ballast Value: %f\n",atof(ptr2));
											}
										}
										break;
									case 'B':
										if (*(ptr2+1) == 'U') {
											//Set Bugs
								 			if ((ptr2 = strtok(NULL, del2))!=NULL) {
												setDegradation(atof(ptr2));
												status&=(0x10^0xff);
												debug_print("Get Bugs Value: %f\n",atof(ptr2));
											}
										}
										break;
									case 'N' :
									  if (*(ptr2+1)=='A') {
											// Not Available received
											if ((ptr2 = strtok(NULL,del2))!=NULL) {
												if ((*ptr2=='W') && (*(ptr2+1)=='L')) // Wing loading
													status&=(0x08^0xff);
												else
													if ((*ptr2=='B') && (*(ptr2+1)=='U')) // Bugs
														status&=(0x10^0xff);
													else
														if ((*ptr2=='M') && (*(ptr2+1)=='C')) // MacCready
															status&=(0x04^0xff);
														else
															if ((*ptr2=='R') && (*(ptr2+1)=='P') && (*(ptr2+2)=='O')) // Real Polar
																status&=(0x02^0xff);
											}
										}
										break;
									case 'P':
										//Set Polar
										// depricated
										if (*(ptr2+1) == 'O' &&  *(ptr2+2) == 'L') {
											if ((ptr2 = strtok(NULL, del2))!=NULL) {
												polar.a=atof(ptr2);
												if ((ptr2 = strtok(NULL, del2))!=NULL) {
													polar.b=atof(ptr2);
													if ((ptr2 = strtok(NULL, del2))!=NULL) {
														polar.c=atof(ptr2);
														if ((ptr2 = strtok(NULL, del2))!=NULL) {
															polar.w=atof(ptr2);
															setPolar(polar.a, polar.b, polar.c, polar.w);
															status&=(0x01^0xff);
														}
													}
												}
											}
										}
										break;
									case 'R':
										//Set Real Polar
										if (*(ptr2+1) == 'P' &&  *(ptr2+2) == 'O') {
											if (read_float_from_sentence(3,fvals,ptr2+4)) {
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
					ptr=strtok(ptr, del1); // Successfully processed a sentence, so get back to tokenizing about $, note ptr has been incremented
				} else ptr=strtok(NULL, del1); // Didn't find a valid checksum, so move on
			} else ptr=strtok(NULL,del1); // Didn't find a valid sentence start, so move on
		}
		if ((invalid_sentence) && (last!=NULL)) { // If the last sentence wasn't valid retain it for next time
			for (i=0;((i<(buffer+2000-last))&&(last[i]!=0));++i) buffer[i]=last[i]; // Copy it to the beginning of the buffer, can't use strcpy, results can be bad
			buffer[i]=0; // terminate the end of the message.
		} else buffer[0]=0; // Last sentence was valid, so start with an empty buffer for next time
	}
}
