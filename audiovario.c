#include "audiovario.h" 


snd_pcm_t *pcm_handle;
int16_t pcm_buffer[BUFFER_SIZE];
unsigned int sample_rate=44100;
snd_pcm_uframes_t period_size;
snd_pcm_uframes_t buffer_size;
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
  if (volume<0) volume=0;
  if (volume>100) volume=100;

  printf("New Volume: %f\n", volume);
  return volume;
}

float pulse_syn(float phase, float rise, float fall, float duty ){
  float ret;

  phase= fmodf(phase,2*m_pi);

  if (phase <rise) {
    ret=phase/rise;
    return ret>1.0?1.0:ret;
  }
  else if (phase < rise+duty) return 1.0;
  else if (phase < rise+duty+fall) {
    ret=1.0-((phase-rise-duty)/fall);
    return ret<0.0?0.0:ret;
  }
  else return 0.0;
}

float triangle(float phase ){

  phase= fmodf(phase,2*m_pi);
  if (phase <m_pi) return (phase-m_pi/2)*2/m_pi;
  else return 1-(phase-m_pi)*2/m_pi;
}

void synthesise_vario(float val, int16_t* pcm_buffer, size_t frames_n, t_vario_config *vario_config){
	unsigned int j;
	float freq, pulse_freq; 

   for(j=0;j<frames_n;j++) {
     if (mute || (val > vario_config->deadband_low && val < vario_config->deadband_high)) pcm_buffer[j]=0;
     else {
       if (val > 0)
	   {
		 pulse_freq = (val > 0.5)? float(sample_rate)/(vario_config->pulse_length/(val*vario_config->pulse_length_gain)) : (float(sample_rate)/(float(vario_config->pulse_length*2)));
         freq= vario_config->base_freq_pos+(val*vario_config->freq_gain_pos);
         pcm_buffer[j]=pulse_syn( float(j)*m_pi*2.0/float(sample_rate)*pulse_freq+pulse_phase_ptr, vario_config->pulse_rise,vario_config->pulse_fall,vario_config->pulse_duty) * 327.67*volume*triangle(float(j)*m_pi*2.0/sample_rate*freq+phase_ptr);
       }
	   else
	   {
         freq= vario_config->base_freq_neg / (1.0-val*vario_config->freq_gain_neg);
         pcm_buffer[j]=327.67*volume*triangle(float(synth_ptr)*m_pi*2.0/sample_rate*freq);
         pcm_buffer[j]=327.67*volume*triangle(float(j)*m_pi*2.0/float(sample_rate)*freq+phase_ptr);
       }

     }
     synth_ptr++;
   }
   phase_ptr=float(j)*m_pi*2.0/float(sample_rate)*freq+phase_ptr;
   pulse_phase_ptr=float(j)*m_pi*2.0/float(sample_rate)*pulse_freq+pulse_phase_ptr;
   
}

void* update_audio_vario(void *arg)
{
  snd_pcm_sframes_t avail;

	while(1)  
	{
		if ((avail = snd_pcm_avail_update(pcm_handle)) < 0) {
			if (avail == -EPIPE) {
			/*underrun occured, reset the sound device*/ 					
			snd_pcm_prepare(pcm_handle);
			}
		}

		if ((size_t)avail > BUFFER_SIZE) avail= (snd_pcm_sframes_t) BUFFER_SIZE;
		synthesise_vario(audio_val, pcm_buffer, (size_t)avail, &vario_config[vario_mode]);

		snd_pcm_writei(pcm_handle, &pcm_buffer, avail);
		snd_pcm_wait(pcm_handle, 100 );
	}
}
	      
int start_pcm() {

  snd_pcm_hw_params_t *hw_params;
  snd_pcm_sw_params_t *sw_params;
  //snd_pcm_sframes_t avail;

  if (snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0 ||
      snd_pcm_hw_params_malloc(&hw_params) < 0 ||   		
      snd_pcm_hw_params_any(pcm_handle, hw_params) < 0 ||
      snd_pcm_hw_params_set_access(pcm_handle, hw_params, 
                                   SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
      snd_pcm_hw_params_set_format(pcm_handle, hw_params, 
                                   SND_PCM_FORMAT_S16_LE) < 0 ||
      snd_pcm_hw_params_set_rate_near(pcm_handle, 
                                      hw_params, &sample_rate, 0) < 0 ||
      snd_pcm_hw_params_set_channels(pcm_handle, hw_params, 1) < 0 ||
      snd_pcm_hw_params_set_period_size_near (pcm_handle, hw_params, 
                                              &period_size, NULL) <0 ||
      snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, 
                                              &buffer_size) <0 ||
      snd_pcm_hw_params (pcm_handle, hw_params) < 0) {
    return false;
  }
  snd_pcm_hw_params_free(hw_params);
  if (snd_pcm_sw_params_malloc(&sw_params) < 0 ||
      snd_pcm_sw_params_current(pcm_handle, sw_params) < 0 ||
      snd_pcm_sw_params_set_avail_min(pcm_handle, 
                                       sw_params, period_size) < 0 ||
      snd_pcm_sw_params_set_start_threshold(pcm_handle, 
                                            sw_params, 0U) < 0 ||
      snd_pcm_sw_params(pcm_handle, sw_params)  < 0 ||
      snd_pcm_prepare(pcm_handle) < 0) {
    return false;
  }
  snd_pcm_sw_params_free(sw_params);
  
  snd_pcm_start(pcm_handle);
  snd_pcm_pause(pcm_handle,0);
  return true;
 }


int stop_pcm() {
  snd_pcm_drop(pcm_handle);
  snd_pcm_close (pcm_handle);
  return (0);
}

/*main (int argc, char *argv[])
{ 

  start_pcm();
  while(1) {
  snd_pcm_wait(pcm_handle, 100 );
  update_audio_vario(1.5);}
  stop_pcm();
}
*/
