#include <string.h>
#include "audiovario.h" 


int16_t buffer[BUFFER_SIZE];
unsigned int sample_rate=RATE;

int synth_ptr=0;
bool mute=true;
float volume=50.0;
float phase_ptr=0.0;
float pulse_phase_ptr=0.0;

//two vario configs: one for vario one for STF
t_vario_config vario_config[2];
enum e_vario_mode vario_mode=vario;
float audio_val = 0.0;

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

void init_vario_config()
{
	// init config struct
	vario_config[vario].deadband_low = DEADBAND_LOW;
	vario_config[vario].deadband_high = DEADBAND_HIGH;
	vario_config[vario].pulse_length = PULSE_LENGTH;
	vario_config[vario].pulse_length_gain = PULSE_LENGTH_GAIN;
	vario_config[vario].pulse_duty = PULSE_DUTY;
	vario_config[vario].pulse_rise = PULSE_RISE;
	vario_config[vario].pulse_fall = PULSE_FALL;
	vario_config[vario].base_freq_pos = BASE_FREQ_POS;
	vario_config[vario].base_freq_neg = BASE_FREQ_NEG;
	vario_config[vario].freq_gain_pos = FREQ_GAIN_POS;
	vario_config[vario].freq_gain_neg = FREQ_GAIN_NEG;
	
	vario_config[stf].deadband_low = STF_DEADBAND_LOW;
	vario_config[stf].deadband_high = STF_DEADBAND_HIGH;
	vario_config[stf].pulse_length = STF_PULSE_LENGTH;
	vario_config[stf].pulse_length_gain = STF_PULSE_LENGTH_GAIN;
	vario_config[stf].pulse_duty = STF_PULSE_DUTY;
	vario_config[stf].pulse_rise = STF_PULSE_RISE;
	vario_config[stf].pulse_fall = STF_PULSE_FALL;
	vario_config[stf].base_freq_pos = STF_BASE_FREQ_POS;
	vario_config[stf].base_freq_neg = STF_BASE_FREQ_NEG;
	vario_config[stf].freq_gain_pos = STF_FREQ_GAIN_POS;
	vario_config[stf].freq_gain_neg = STF_FREQ_GAIN_NEG;
}

float change_volume(float delta){
  volume+=delta;
	vario_unmute();
  if (volume<0) volume=0;
  if (volume>100) volume=100;

  return volume;
}

inline float pulse_syn(float phase, float rise, float fall, float risei, float falli, float duty ){
  float ret;

  if (phase <rise) {
    ret=phase*risei;
    return ret>1.0?1.0:ret;
  }
  if (phase < rise+duty) return 1.0;
  if (phase < rise+duty+fall) {
    ret=1.0-((phase-rise-duty)*falli);
    return ret<0.0?0.0:ret;
  }
  return 0.0;
}

inline float triangle(float phase ){

  if (phase <m_pi) return (phase-m_pi/2)*2/m_pi;
  return 1-(phase-m_pi)*2/m_pi;
}

void synthesise_vario(float val, int16_t* pcm_buffer, size_t frames_n, t_vario_config *vario_config) {
	unsigned int j;
	float freq, pulse_freq; 
	float freq_upd, pulse_freq_upd, temp;

	if (mute || (val > vario_config->deadband_low && val < vario_config->deadband_high)) { 
		memset(pcm_buffer,0,frames_n*sizeof(int16_t));
	} else {
		if (val>0) {
			pulse_freq = (val > 0.5)? float(sample_rate)/(vario_config->pulse_length/(val*vario_config->pulse_length_gain)) : (float(sample_rate)/(float(vario_config->pulse_length*2)));
         		freq= vario_config->base_freq_pos+(val*vario_config->freq_gain_pos);
			temp=2.0*m_pi/float(sample_rate);
			pulse_freq_upd=fmodf(temp*pulse_freq,2.0*m_pi);
			freq_upd=fmodf(temp*freq,2.0*m_pi);
			for (j=0;j<frames_n;++j) {
				pcm_buffer[j]=(int)(round(pulse_syn(pulse_phase_ptr, vario_config->pulse_rise,vario_config->pulse_fall,vario_config->pulse_risei,vario_config->pulse_falli,vario_config->pulse_duty) * 327.67*volume*triangle(phase_ptr)));
				pulse_phase_ptr+=pulse_freq_upd;
				phase_ptr+=freq_upd;
				if (pulse_phase_ptr>=2.0*m_pi) pulse_phase_ptr-=2.0*m_pi;
				if (phase_ptr>=2.0*m_pi) phase_ptr-=2.0*m_pi;
			}
		} else {
			freq= vario_config->base_freq_neg / (1.0-val*vario_config->freq_gain_neg);
			freq_upd=fmodf(2.0*m_pi/float(sample_rate)*freq,2.0*m_pi);
			for (j=0;j<frames_n;++j) {
				pcm_buffer[j]=327.67*volume*triangle(phase_ptr);
				phase_ptr+=freq_upd;
				if (phase_ptr>=2.0*m_pi) phase_ptr-=2.0*m_pi;

			}
		}
	}
	synth_ptr+=frames_n;
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



void context_state_cb(pa_context* context, void* mainloop) {
    pa_threaded_mainloop_signal(mainloop, 0);
}

void stream_state_cb(pa_stream *s, void *mainloop) {
    pa_threaded_mainloop_signal(mainloop, 0);
}

void stream_write_cb(pa_stream *stream, size_t requested_bytes, void *userdata) {
    
    size_t bytes_to_fill = BUFFER_SIZE*2;
    int bytes_remaining = requested_bytes;
    
    while (bytes_remaining > 0) {

        if (bytes_to_fill > bytes_remaining) bytes_to_fill = bytes_remaining;

	synthesise_vario(audio_val, buffer, (size_t)bytes_to_fill/2, &(vario_config[vario_mode]));

        pa_stream_write(stream, buffer, bytes_to_fill, NULL, 0LL, PA_SEEK_RELATIVE);

        bytes_remaining -= bytes_to_fill;
    }
}

void stream_success_cb(pa_stream *stream, int success, void *userdata) {
    return;
}
