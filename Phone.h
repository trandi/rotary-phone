#pragma once

#include <string>
#include <chrono>
#include <memory>
#include <iostream>
#include <chrono>

#include <unifex/type_erased_stream.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/sequence.hpp>
#include <unifex/just_from.hpp>
#include <unifex/repeat_effect_until.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/let_done.hpp>
#include <unifex/let_value.hpp>
#include <unifex/let_value_with.hpp>
#include <unifex/timed_single_thread_context.hpp>
#include <unifex/on.hpp>
#include <unifex/defer.hpp>
#include <unifex/v1/async_scope.hpp>

#include "Sounds.h"

class Phone {
  struct NotPubliclyConstructible{};

  static void hardwareSetup();
  static void hardwareTerminate();
  static void setBellMode(bool enable);
  static void hitBell(bool left);
 
  // HAS to be run an a timed_single_thread_context (or similar) provided scheduler
  unifex::sender auto handleRinging() {
    if(freqHz_ < 10 || freqHz_ > 50) throw std::runtime_error("Use a valid ring frequency !!!");
    auto delayMs = std::chrono::milliseconds(500 / freqHz_); 

    auto one = unifex::sequence(  
      unifex::just_from([&] {
        hitBell(true);
      }),
      unifex::schedule_after(delayMs),
      unifex::just_from([&] {
        hitBell(false);
      }),
      unifex::schedule_after(delayMs) 
    );

    auto waitAndRingUntilStopped = unifex::sequence(
      // wait until asked to ring
      unifex::repeat_effect_until(
        unifex::schedule_after(std::chrono::milliseconds(10)),
        [&]() {
          return ringing_;
        }
      ),
      
      unifex::just_from([] {
        std::cout << "Started Ringing..." << std::endl;
        setBellMode(true);
      }),

      // ring until asked to stop ringing
      unifex::repeat_effect_until(
        one,
        [&]() {
          return !ringing_;
        }
      ),

      unifex::just_from([] {
        std::cout << "Stopped Ringing..." << std::endl;
        setBellMode(false);
      })
    );

    // infinite loop
    return unifex::repeat_effect(
      waitAndRingUntilStopped
    );
 } 



  // HAS to be run an a timed_single_thread_context (or similar) provided scheduler
  unifex::sender auto handleTonePlaying() {
    auto continuousToneSender = unifex::sequence(
      unifex::just_from([&] {
        sound_->pcm->playTone(440, 50);
      }),
      // add a short delay as above uses 100%CPU and we need to give time to other tasks 
      unifex::schedule_after(std::chrono::milliseconds(5))
    );

    auto intermitentToneSender = unifex::sequence(
      unifex::just_from([&] {
        sound_->pcm->playTone(500, 400);
      }),
      unifex::schedule_after(std::chrono::milliseconds(400))
    );


    auto waitAndPlayUntilStopped = unifex::sequence(
      // wait until asked to play a tone
      unifex::repeat_effect_until(
        unifex::schedule_after(std::chrono::milliseconds(10)),
        [&] {
          return tone_ != Tone::NONE;
        }
      ),

      unifex::just_from([&] {
        sound_->mixer->setVolume(3);
        sound_->pcm->open();
      }),

      unifex::repeat_effect_until(
        unifex::defer([this, continuousToneSender, intermitentToneSender] () ->
           unifex::variant_sender<decltype(continuousToneSender), decltype(intermitentToneSender)> {
          if (tone_ == Tone::CONTINUOUS) {
            return continuousToneSender;
          } else {
            return  intermitentToneSender;
          }
        }),
        [&] {
          return tone_ == Tone::NONE;
        }
      ),
  
      unifex::just_from([&] {
        sound_->pcm->close();
      })
    );

    // infinite loop
    return unifex::repeat_effect(
      waitAndPlayUntilStopped
    ); 
  }



public:
  // use factory method to ensure singleton 
  template <typename Scheduler>
    requires unifex::scheduler<Scheduler> // has to be TIMED scheduler
  static std::shared_ptr<Phone> create(
    unsigned freqHz,
    std::shared_ptr<Sound> sound, 
    Scheduler&& scheduler
  ) {
    static auto instance_ = std::make_shared<Phone>(
      NotPubliclyConstructible(), 
      freqHz,
      std::move(sound), 
      std::forward<Scheduler>(scheduler)
    );

    return instance_;
  }
  
  // can't move async_scope around
  Phone(Phone&&) = delete;
  Phone& operator=(Phone&) = delete;
  // given that it controls GPIOs we can't have more than 1 single instance-> forbid copy
  Phone(const Phone&) = delete;
  Phone& operator=(const Phone&) = delete;

  // can't make constructor private due to std::make_shared<> yet we want to force usage of static factory method
  template <typename Scheduler>
    requires unifex::scheduler<Scheduler> // has to be TIMED scheduler
  explicit Phone(
    NotPubliclyConstructible, 
    unsigned freqHz, 
    std::shared_ptr<Sound> sound, 
    Scheduler&& scheduler
  ): freqHz_(freqHz), sound_(std::move(sound)) {
    hardwareSetup();

    // BACKGROUND tasks
    backgroundScope_.detached_spawn_on(
      std::forward<Scheduler>(scheduler),
      handleRinging()
    );
  
    backgroundScope_.detached_spawn_on(
      std::forward<Scheduler>(scheduler),
      handleTonePlaying()
    );
  }

  ~Phone() {
    hardwareTerminate();
  }

  // async cleanup
  unifex::sender auto cleanup() {
    // scope won't acccept anymore work & requests cancellation of all outstanding work
    return backgroundScope_.cleanup();
  }

  enum class Tone {
    NONE,
    CONTINUOUS,
    INTERMITENT
  };


  /////////// API //////////////////////

  // ACTIONS
  void ring(bool on);
  
  void playTone(Tone tone);
  
  
  // EVENTS
  static unifex::type_erased_stream<bool> hookStateStream(); 

  static unifex::type_erased_stream<unsigned> digitStream(); 

  static unifex::type_erased_stream<std::string> numberStream(); 

/////////////////////////////////////


private:

  unsigned freqHz_;
  bool ringing_{false};
  Tone tone_{Tone::NONE};

  unifex::async_scope backgroundScope_;

  std::shared_ptr<Sound> sound_;

}; // Phone


std::ostream& operator<<(std::ostream& os, Phone::Tone tone);
