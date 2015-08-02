#include "audiovario.h" 


snd_pcm_t *pcm_handle;
int16_t pcm_buffer[BUFFER_SIZE];
unsigned int sample_rate=44100;
snd_pcm_uframes_t period_size;
snd_pcm_uframes_t buffer_size;
int synth_ptr=0;
bool mute=false;
float volume=50.0;
float phase_ptr=0.0;
float pulse_phase_ptr=0.0;

t_vario_config vario_config;

extern float te;

void toggle_mute(){
  mute=!mute;
}

void init_vario_config(t_vario_config *vario_config)
{
	// init config struct
	vario_config->deadband_low = DEADBAND_LOW;
	vario_config->deadband_high = DEADBAND_HIGH;
	vario_config->pulse_length = PULSE_LENGTH;
	vario_config->pulse_length_gain = PULSE_LENGTH_GAIN;
	vario_config->base_freq_pos = BASE_FREQ_POS;
	vario_config->base_freq_neg = BASE_FREQ_NEG;
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


void  Synthesise(int16_t* pcm_buffer, size_t frames_n){
  int j;
  for(j=0;j<frames_n;j++) {
    pcm_buffer[j]=32767*triangle(float(synth_ptr)*m_pi*2.0/sample_rate*(100+(1+sin(float(synth_ptr)/sample_rate/10.0)*400.0)));
    synth_ptr++;
  }
}

void synthesise_vario(float te, int16_t* pcm_buffer, size_t frames_n, t_vario_config *vario_config){
   int j;
   float freq, pulse_freq; 

   for(j=0;j<frames_n;j++) {
     if (mute || (te > vario_config->deadband_low && te < vario_config->deadband_high)) pcm_buffer[j]=0;
     else {
       if (te > 0)
	   {
		 pulse_freq = (te > 0.5)? float(sample_rate)/(vario_config->pulse_length/(te*vario_config->pulse_length_gain)) : (float(sample_rate)/(float(vario_config->pulse_length*2)));
         freq= vario_config->base_freq_pos+(te*FREQ_GAIN_POS);
         pcm_buffer[j]=pulse_syn( float(j)*m_pi*2.0/float(sample_rate)*pulse_freq+pulse_phase_ptr  , PULSE_RISE,PULSE_FALL,PULSE_DUTY) * 32.767*volume*triangle(float(j)*m_pi*2.0/sample_rate*freq+phase_ptr);
       }
	   else
	   {
         freq= vario_config->base_freq_neg / (1.0-te*FREQ_GAIN_NEG);
         pcm_buffer[j]=32.767*volume*triangle(float(synth_ptr)*m_pi*2.0/sample_rate*freq);
         pcm_buffer[j]=32.767*volume*triangle(float(j)*m_pi*2.0/float(sample_rate)*freq+phase_ptr);
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
			printf("called prepare\n");
			}
		}

		if ((size_t)avail > BUFFER_SIZE) avail= (snd_pcm_sframes_t) BUFFER_SIZE;
		synthesise_vario(te, pcm_buffer, (size_t)avail, &vario_config);

		int written=snd_pcm_writei(pcm_handle, &pcm_buffer, avail);
		//printf("###sndpcm_write avail:%d,%d,%d\n", (int) avail, written, (int) period_size);
		//printf("###called ALSA callback \n");
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
    printf("returned false\n");
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
