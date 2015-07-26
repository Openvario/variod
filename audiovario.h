#ifndef AUDIOVARIO
#define AUDIOVARIO

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <alsa/asoundlib.h>
	      
#define BUFFER_SIZE 4096
#define m_pi 3.1415 //M_PI doesn't work, need to check why....

#define DEADBAND_LOW -0.5  
#define DEADBAND_HIGH 0.25  /*DEADBAND: Vario remains silent for DEADBAND_LOW < TE value < DEADBAND_HIGH */
#define PULSE_LENGTH  30000 /*LENGTH of PULSE (PAUSE) for positive TE values, in samples*/
#define PULSE_LENGTH_GAIN  1.5 /*PULSES get shorter with higher TE values*/
#define BASE_FREQ_POS 200   /*BASE frequency for positive TE values in Hz*/
#define BASE_FREQ_NEG 200  /*BASE frequency for negative TE values in Hz*/
#define FREQ_GAIN_POS 180
#define FREQ_GAIN_NEG 0.75


void toggle_mute();
float change_volume(float delta);
float triangle(float phase );
void  Synthesise(int16_t* pcm_buffer, size_t frames_n);
void synthesise_vario(float te, int16_t* pcm_buffer, size_t frames_n);
void* update_audio_vario(void *);
int start_pcm();
int stop_pcm();

#endif
