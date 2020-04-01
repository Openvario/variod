#ifndef AUDIOVARIO
#define AUDIOVARIO

#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <pulse/pulseaudio.h>
	      
#define FORMAT PA_SAMPLE_S16LE
#define RATE 44100
#define BUFFER_SIZE 4096 
#define m_pi 3.14159265 //M_PI doesn't work, need to check why....

#define DEADBAND_LOW -0.0  
#define DEADBAND_HIGH 0.0  /*DEADBAND: Vario remains silent for DEADBAND_LOW < TE value < DEADBAND_HIGH */
#define PULSE_LENGTH  48000 /*LENGTH of PULSE (PAUSE) for positive TE values, in samples*/
#define PULSE_LENGTH_GAIN  1 /*PULSES get shorter with higher TE values*/
#define PULSE_DUTY  2.6 /*Pulse duty cycle 2*PI == 100%*/
#define PULSE_RISE 0.3 /*Timing for rising edge of pulse (Fade-In)*/
#define PULSE_FALL 0.3 /*Timing for falling edge of pulse (Fade-Out)*/
#define BASE_FREQ_POS 350   /*BASE frequency for positive TE values in Hz*/
#define BASE_FREQ_NEG 350  /*BASE frequency for negative TE values in Hz*/
#define FREQ_GAIN_POS 120
#define FREQ_GAIN_NEG 0.85

#define STF_DEADBAND_LOW -2.5  
#define STF_DEADBAND_HIGH 2.5  /*DEADBAND: Vario remains silent for DEADBAND_LOW < TE value < DEADBAND_HIGH */
#define STF_PULSE_LENGTH  48000 /*LENGTH of PULSE (PAUSE) for positive values, in samples*/
#define STF_PULSE_LENGTH_GAIN  0.2 /*PULSES get shorter with higher values*/
#define STF_PULSE_DUTY  2.6 /*Pulse duty cycle 2*PI == 100%*/
#define STF_PULSE_RISE 0.1 /*Timing for rising edge of pulse (Fade-In)*/
#define STF_PULSE_FALL 0.1 /*Timing for falling edge of pulse (Fade-Out)*/
#define STF_BASE_FREQ_POS 350   /*BASE frequency for positive TE values in Hz*/
#define STF_BASE_FREQ_NEG 350  /*BASE frequency for negative TE values in Hz*/
#define STF_FREQ_GAIN_POS 30
#define STF_FREQ_GAIN_NEG 0.1

enum e_vario_mode{
  vario = 0,
  stf   = 1
};

typedef struct{
	float deadband_low;
	float deadband_high;
	int pulse_length;
	float pulse_length_gain;
	float pulse_duty;
	float pulse_rise;
	float pulse_fall;
	int base_freq_pos;
	int base_freq_neg;
  float freq_gain_pos;
  float freq_gain_neg;
} t_vario_config;



void context_state_cb(pa_context* context, void* mainloop);
void stream_state_cb(pa_stream *s, void *mainloop);
void stream_success_cb(pa_stream *stream, int success, void *userdata);
void stream_write_cb(pa_stream *stream, size_t requested_bytes, void *userdata);


void toggle_mute();
void vario_mute();
void vario_unmute();
float change_volume(float delta);
float triangle(float phase );
void synthesise_vario(float val, int16_t* pcm_buffer, size_t frames_n, t_vario_config *vario_config);
void* update_audio_vario(void *);
void start_pcm();
void stop_pcm();
void init_vario_config();

t_vario_config* get_vario_config(enum e_vario_mode mode);
void set_audio_val(float val);
void set_vario_mode(enum e_vario_mode mode);
#endif
