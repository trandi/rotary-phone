#pragma once

#include <memory>
#include <cmath>
#include <climits>
#include <iostream>
#include <alsa/asoundlib.h>


struct Mixer {

  void setVolume(long perc);

private:
  inline static const std::string kCard{"hw:1"};
  inline static const std::string kSelemName{"Speaker"};
  snd_mixer_t* handle_;
  snd_mixer_elem_t* elem_;
  long max_;
};


struct Pcm {
  void open();
  void playTone(int freq, int durationMillis);
  void close();

private:
  inline static const std::string kPcmName{"plughw:1,0"};  // USB audio device
  inline static const int kRate{48000};
  inline static const size_t kBufferSize{8192};
  int8_t buffer_[kBufferSize];
  snd_pcm_t* handle_;
};


class Sound {
  struct NotPubliclyConstructible{};

public:
  std::unique_ptr<Mixer> mixer;
  std::unique_ptr<Pcm> pcm;

  static std::shared_ptr<Sound> create() {
    return std::make_shared<Sound>(
      NotPubliclyConstructible{},
      std::make_unique<Mixer>(),
      std::make_unique<Pcm>()
    );
  }

  explicit Sound(
    NotPubliclyConstructible, 
    std::unique_ptr<Mixer> m, 
    std::unique_ptr<Pcm> p
  ): mixer(std::move(m)), pcm(std::move(p)) {
  }
};
