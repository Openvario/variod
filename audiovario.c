#include "audiovario.h"

#include <pulse/pulseaudio.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#define DEFAULT_VOLUME 50.0

static int16_t buffer[BUFFER_SIZE];
static float m4sr_ = 4.0/(float) RATE; // Pre-calculate 4 / sample_rate used for some future calculations
//static int synth_ptr=0; // Not sure what this does, don't need
static bool mute=true;
static float volume=DEFAULT_VOLUME;
static float int_volume=DEFAULT_VOLUME*327.67; // Pre-calculate volume
static float phase_ptr=0.0;
static float pulse_phase_ptr=0.0;

//two vario configs: one for vario one for STF
t_vario_config vario_config[2];
enum e_vario_mode vario_mode=vario;

//this is the value to be synthesised: In vario mode it's the TE compensated
//climb/sink value, in STF it is a value correlating to the difference of STF,
//and current airspeed
static float audio_val = 0.0;

t_vario_config* get_vario_config(enum e_vario_mode mode){
	return &vario_config[mode];
}

void set_vario_mode(enum e_vario_mode mode){
	vario_mode=mode;
}

void set_audio_val(float val){
	audio_val=val;
}

void toggle_mute(){
	mute=!mute;
}

void vario_mute(){
	mute=true;
}

void vario_unmute(){
	mute=false;
}

void init_vario_config() {

	// init config struct
	vario_config[vario].deadband_low = DEADBAND_LOW;
	vario_config[vario].deadband_high = DEADBAND_HIGH;
	vario_config[vario].pulse_length = PULSE_LENGTH; // This isn't really needed (see loval), but without it the initial display will be wrong if nothing in conf file
	vario_config[vario].pulse_length_gain = PULSE_LENGTH_GAIN; // This isn't really needed (see hival), but without it, the initial display will be wrong if nothing in conf file
	vario_config[vario].pulse_rise = PULSE_RISE;
	vario_config[vario].pulse_risei = 1.0/(float) PULSE_RISE;  // reciprocal of rise to avoid a divide
	vario_config[vario].pulse_fall = PULSE_FALL;
	vario_config[vario].pulse_falli = 1.0/(float) PULSE_FALL;   // reciprocal of fall to avoid a divide
	vario_config[vario].pulse_riseduty = PULSE_RISE + PULSE_DUTY; // pre-calculate rise+duty
	vario_config[vario].pulse_risedutyfall = PULSE_RISE + PULSE_DUTY + PULSE_FALL; // pre-calculate rise+duty+fall
	vario_config[vario].base_freq_pos = (float) BASE_FREQ_POS*m4sr_; // in units of: 100 grads per sample
	vario_config[vario].base_freq_neg = (float) BASE_FREQ_NEG*m4sr_; // in units of: 100 grads per sample
	vario_config[vario].freq_gain_pos = (float) FREQ_GAIN_POS*m4sr_; // in units of: 100 grads per sample per value
	vario_config[vario].freq_gain_neg = (float) FREQ_GAIN_NEG;
	vario_config[vario].loval = m_pi/(float) PULSE_LENGTH;
	vario_config[vario].hival = vario_config[vario].loval * 2.0 * PULSE_LENGTH_GAIN;

	vario_config[stf].deadband_low = STF_DEADBAND_LOW;
	vario_config[stf].deadband_high = STF_DEADBAND_HIGH;
	vario_config[stf].pulse_length = STF_PULSE_LENGTH; // See above
	vario_config[stf].pulse_length_gain = STF_PULSE_LENGTH_GAIN; // See above
	vario_config[stf].pulse_rise = STF_PULSE_RISE;
	vario_config[stf].pulse_risei = 1.0/(float)STF_PULSE_RISE; // reciprocal of rise to avoid a divide
	vario_config[stf].pulse_fall = STF_PULSE_FALL;
	vario_config[stf].pulse_falli = 1.0/(float) STF_PULSE_FALL;  // reciprocal of fall to avoid a divide
	vario_config[stf].pulse_riseduty = STF_PULSE_RISE + STF_PULSE_DUTY; // Pre-calculate rise+duty
	vario_config[stf].pulse_risedutyfall = STF_PULSE_RISE + STF_PULSE_DUTY + STF_PULSE_FALL; // Pre-calculate rise+duty+fall
	vario_config[stf].base_freq_pos = (float) STF_BASE_FREQ_POS*m4sr_; // in units of: 100 grads per sample
	vario_config[stf].base_freq_neg = (float) STF_BASE_FREQ_NEG*m4sr_; // in units of: 100 grads per sample
	vario_config[stf].freq_gain_pos = (float) STF_FREQ_GAIN_POS*m4sr_; // in units of: 100 grads per sample per value
	vario_config[stf].freq_gain_neg = STF_FREQ_GAIN_NEG;
	vario_config[stf].loval = m_pi/(float) STF_PULSE_LENGTH;
	vario_config[stf].hival = vario_config[stf].loval * 2.0 * STF_PULSE_LENGTH_GAIN;
}

float change_volume(float delta) {
	volume+=delta;
	vario_unmute();
	if (volume<0) volume=0;
	if (volume>100) volume=100;
	int_volume=volume*327.67; // Pre-calculate actual multiplier
	vario_config[vario].pulse_riseiv=vario_config[vario].pulse_risei*int_volume; // Pre-calculate multiplier for 'rise' including volume
	vario_config[vario].pulse_falliv=vario_config[vario].pulse_falli*int_volume; // Pre-calculate multiplier for 'fall' including volume
	vario_config[stf].pulse_riseiv=vario_config[stf].pulse_risei*int_volume; // Pre-calculate multiplier for 'rise' including volume
	vario_config[stf].pulse_falliv=vario_config[stf].pulse_falli*int_volume; // Pre-calculate multiplier for 'fall' including volume
	return volume;
}

// Synthesize the "sink" (triangle) waveform.  Amplitude is 1, and for calculation simplicity, phase is in 100s of grads (0 to 4), making it a simple
// map (0-2) translates to (-1 to 1) and (2-4) translates to (1 to -1), both done by subtraction.

inline float triangle(float phase) {

	if (phase>2) return (3-phase); else return (phase-1);
}

// Synthesize_vario fills the buffer based on operating mode (muted/deadband, climb, sink):
// This is a substantial re-write from previous versions and introduces the concept of a taper, and removes the previous pulse_syn function.  The taper is a coefficient
// between 0 and 1.  It is always being incremented (by uprate) or decremented (by downrate) based on taperd(irection), but is limited to the range of 0 to 1.
// Whenever switcheing operating mode, taperd and the rates are changed abruptly but taper is continuous, therefore there will always be a smooth volume transition.
//
// Climb mode is more challenging since the taperd changes to rising at pulse_phase_ptr=0, and to falling at pulse_phase_ptr = pulse_riseduty.  When switching into climb mode,
// pulse_phase_ptr is reset to the current taper location and direction is set based on the value of taper.  If taper is greater than .5, taperd is set to falling, else it's set to rising.
// The hope is this will give the fastest possible audio response to switching modes.
//
// This function incorporates a "safeguard" which ensures only an integer number of triangle waveforms, ending at either 0 or 180 degrees are included in a buffer.  This is
// probably unnecessary because of the taper, but it may help in the case of a buffer underrun.
//
// To improve performance, use of floating point divides is minimized versus legacy versions.

static size_t synthesise_vario(float val, int16_t* pcm_buffer, size_t frames_n, t_vario_config *vario_config) {
	static int mode=0;
	static bool taperd=false;
	static bool initialized = false;
	static float deltaphase, deltapulse=0; // phase accumulators
	static float uprate, downrate;
	static float taper=0;

	if (!initialized) {
		deltaphase=vario_config->base_freq_pos;
		initialized = true;
	}

	if (mute || (val > vario_config->deadband_low && val < vario_config->deadband_high)) { // Mute/deadband mode
		mode=0;
		taperd=false;
		uprate=downrate=0.001; // This might benefit from tweaking
	} else {
		if (val<=0) { // Sink mode
			mode=1;
			taperd=true;
			uprate=downrate=0.001; // This might benefit from tweaking
			deltaphase = (vario_config->base_freq_neg / (1.0-val*vario_config->freq_gain_neg));
		} else { // Climb mode
			if (mode!=2) { // If swtiching into climb mode, reset pulse_phase_ptr and taperd
				if (taper<.5) { // If the taper is below .5, set taper to rise
					pulse_phase_ptr=taper*vario_config->pulse_rise;
					taperd=true;
				} else { // If above .5, set taper to fall
					pulse_phase_ptr=vario_config->pulse_riseduty+(1-taper)*vario_config->pulse_fall;
					taperd=false;
				}
			}
			mode=2;
			deltaphase = (vario_config->base_freq_pos+(val*vario_config->freq_gain_pos)); // 100 grad/sample for triangle wave
			deltapulse = (val>0.5)?vario_config->hival*val : vario_config->loval; // Rad/sample for square wave
			uprate=deltapulse*vario_config->pulse_risei; // Calculate the rising edge rate
			downrate=deltapulse*vario_config->pulse_falli; // Calculate the falling edge rate
		}
	}
	const size_t safeguard  = (size_t)round((floor((frames_n*deltaphase+phase_ptr-1)*0.5)*2.0-phase_ptr+1)/deltaphase); // Number of samples for integer number of cycles

	for (size_t j=0;j<safeguard;++j) {

		//  It's possible this could be sped up by checking if taper = 0 in which case output is 0, or 1 in which case it's int_volume*triangle.
		if (taper==0) pcm_buffer[j]=0;
		else { if (taper==1) pcm_buffer[j]=(int) round(int_volume*triangle(phase_ptr));
			else pcm_buffer[j]=(int) round(taper*int_volume*triangle(phase_ptr));
		}
		phase_ptr+=deltaphase; // Accumulate triangle phase
		if (phase_ptr>4) phase_ptr-=4; // Perform modulo

		if (taperd) { // Adjust taper based on taperd
			taper+=uprate;  // Increment Taper
			if (taper>1) taper=1; // Limit taper at 1
		} else {
			taper-=downrate; // Decrement taper
			if (taper<0) taper=0; // Limit taper at 0.
		}

		if (mode==2) { // Implement climb mode
			pulse_phase_ptr+=deltapulse; // Increment pulse_phase_ptr
			if (pulse_phase_ptr>2*m_pi) { // If it rolls over...
				pulse_phase_ptr-=(2*m_pi); // Perform modulo
				taperd=true; // Set taperd to rising edge
			} else if (pulse_phase_ptr>=vario_config->pulse_riseduty) taperd=false; // Set taperd to falling edge if appropriate
		}
	}
	return safeguard;
}

static void context_state_cb(pa_context* context, void* mainloop) {
	(void)context;

	pa_threaded_mainloop_signal(mainloop, 0);
}

static void stream_state_cb(pa_stream *s, void *mainloop) {
	(void)s;

	pa_threaded_mainloop_signal(mainloop, 0);
}

static void stream_write_cb(pa_stream *stream, size_t requested_bytes, void *userdata) {
	(void)userdata;

	size_t bytes_to_fill = BUFFER_SIZE*2;
	size_t bytes_remaining = requested_bytes;
	bool repeat = true;

	do {
		if (bytes_to_fill > bytes_remaining) {
			bytes_to_fill = bytes_remaining;
			repeat=false;
		}
		size_t bytes_filled=2*(synthesise_vario(audio_val, buffer, (size_t)bytes_to_fill/2, &(vario_config[vario_mode])));
		pa_stream_write(stream, buffer, bytes_filled, NULL, 0LL, PA_SEEK_RELATIVE);
		bytes_remaining -= bytes_filled;
	} while (repeat);
}

static void stream_success_cb(pa_stream *stream, int success, void *userdata) {
	(void)stream;
	(void)success;
	(void)userdata;

	return;
}

void start_pcm() {

	pa_threaded_mainloop *mainloop;
	pa_mainloop_api *mainloop_api;
	pa_context *context;
	pa_stream *stream;

	mainloop = pa_threaded_mainloop_new();
	assert(mainloop);
	mainloop_api = pa_threaded_mainloop_get_api(mainloop);
	context = pa_context_new(mainloop_api, "audio_vario");
	assert(context);

	pa_context_set_state_callback(context, &context_state_cb, mainloop);

	pa_threaded_mainloop_lock(mainloop);
	assert(pa_threaded_mainloop_start(mainloop) == 0);

	printf("Connecting to pulseaudio...\n");
	assert(pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN | PA_CONTEXT_NOFAIL, NULL) == 0);
	for(;;) {
		pa_context_state_t context_state = pa_context_get_state(context);
		assert(PA_CONTEXT_IS_GOOD(context_state));
		if (context_state == PA_CONTEXT_READY) break;
		pa_threaded_mainloop_wait(mainloop);
	}
	printf("Connection to pulseaudio established.\n");

	pa_sample_spec sample_specifications;
	sample_specifications.format = FORMAT;
	sample_specifications.rate = RATE;
	sample_specifications.channels = 1;

	pa_channel_map map;
	pa_channel_map_init_mono(&map);

	stream = pa_stream_new(context, "variod", &sample_specifications, &map);
	pa_stream_set_state_callback(stream, stream_state_cb, mainloop);
	pa_stream_set_write_callback(stream, stream_write_cb, mainloop);

	pa_buffer_attr buffer_attr;
	buffer_attr.maxlength = (uint32_t) -1;
	buffer_attr.tlength = (uint32_t) BUFFER_SIZE; //We need low latency!
	buffer_attr.prebuf = (uint32_t) -1;
	buffer_attr.minreq = (uint32_t) -1;

	pa_stream_flags_t stream_flags;
	stream_flags = PA_STREAM_START_CORKED | PA_STREAM_INTERPOLATE_TIMING |
		PA_STREAM_NOT_MONOTONIC | PA_STREAM_AUTO_TIMING_UPDATE |
		PA_STREAM_ADJUST_LATENCY;

	assert(pa_stream_connect_playback(stream, NULL, &buffer_attr, stream_flags, NULL, NULL) == 0);

	for(;;) {
		pa_stream_state_t stream_state = pa_stream_get_state(stream);
		assert(PA_STREAM_IS_GOOD(stream_state));
		if (stream_state == PA_STREAM_READY) break;
		pa_threaded_mainloop_wait(mainloop);
	}

	pa_threaded_mainloop_unlock(mainloop);
	pa_stream_cork(stream, 0, stream_success_cb, mainloop);

}
