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

#define DEADBAND_LOW -0.0  
#define DEADBAND_HIGH 0.0  /*DEADBAND: Vario remains silent for DEADBAND_LOW < TE value < DEADBAND_HIGH */
#define PULSE_LENGTH  12288 /*LENGTH of PULSE (PAUSE) for positive TE values, in samples*/
#define PULSE_LENGTH_GAIN  1 /*PULSES get shorter with higher TE values*/
#define PULSE_DUTY  2.6 /*Pulse duty cycle 2*PI == 100%*/
#define PULSE_RISE 0.3 /*Timing for rising edge of pulse (Fade-In)*/
#define PULSE_FALL 0.3 /*Timing for falling edge of pulse (Fade-Out)*/
#define BASE_FREQ_POS 400   /*BASE frequency for positive TE values in Hz*/
#define BASE_FREQ_NEG 400  /*BASE frequency for negative TE values in Hz*/
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
