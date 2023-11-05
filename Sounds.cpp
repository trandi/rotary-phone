#include "Sounds.h"

namespace {

  void logErr(int result, const std::string& step) {
    if(result < 0) {
      std::cout << "Alsa error on " << step << ": " << snd_strerror(result) << std::endl;
    }
  }

}


void Mixer::setVolume(long perc) {
  long min;
  logErr(snd_mixer_open(&handle_, 0), "mixer_open");
  logErr(snd_mixer_attach(handle_, kCard.c_str()), "mixer_attach");
  logErr(snd_mixer_selem_register(handle_, NULL, NULL), "mixer_selem_register");
  logErr(snd_mixer_load(handle_), "mixer_load");
 
  snd_mixer_selem_id_t* sid;
  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, kSelemName.c_str());
  elem_ = snd_mixer_find_selem(handle_, sid);
  logErr(snd_mixer_selem_get_playback_volume_range(elem_, &min, &max_), "mixer_volume_range"); 

  std::cout << "Set Volume: " << perc << std::endl;
  logErr(snd_mixer_selem_set_playback_volume_all(elem_, perc * max_ / 100), "mixer_set_volume");

  logErr(snd_mixer_close(handle_), "mixer_close");
}



void Pcm::open() {
  logErr(snd_pcm_open(&handle_, kPcmName.c_str(), SND_PCM_STREAM_PLAYBACK, 0), "pcm_open");

  logErr(snd_pcm_set_params(
    handle_,
    SND_PCM_FORMAT_U8,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    1, // channels
    kRate,
    0, // soft resample 
    500000 // latency
  ), "pcm_set_params");
}


void Pcm::playTone(int freq, int durationMillis) {
  const size_t len = durationMillis * (kRate / 1000);
  const float arg = 2 * 3.141592 * freq / kRate;
  
  int i = 0;
  while(i < len) {
    int j = 0;
    for(; (j<kBufferSize) && (i<len); j++, i++) {
      buffer_[j] = CHAR_MAX * cos(arg * i);
    }

    logErr(snd_pcm_writei(handle_, buffer_, j), "pcm_writei");
  }
}

void Pcm::close() {
  logErr(snd_pcm_close(handle_), "pcm_close");
}
