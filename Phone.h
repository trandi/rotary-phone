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
#include <unifex/v1/async_scope.hpp>


class  Phone {
  struct NotPubliclyConstructible{};

  unsigned freqHz_;
  bool ringing_{false};

  unifex::async_scope backgroundScope_;


  static void hardwareSetup();
  static void hardwareTerminate();
  static void setBellMode(bool enable);
  static void hitBell(bool left);
 
  // HAS to be run an a timed_single_thread_context (or similar) provided scheduler
  // returns a Sender<bool> (ie. if it was answered or reached max rings)
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
        unifex::schedule_after(std::chrono::milliseconds(100)),
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

public:
  // use factory method to ensure singleton 
  template <typename Scheduler>
    requires unifex::scheduler<Scheduler> // has to be TIMED scheduler
  static std::shared_ptr<Phone> create(unsigned freqHz, Scheduler&& scheduler) {
    static auto instance_ = std::make_shared<Phone>(
      NotPubliclyConstructible(), 
      freqHz, 
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
  explicit Phone(NotPubliclyConstructible, unsigned freqHz, Scheduler&& scheduler): freqHz_(freqHz) {
    hardwareSetup();

    // BACKGROUND tasks
    backgroundScope_.detached_spawn_on(
      std::forward<Scheduler>(scheduler),
      handleRinging()
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
  void ring(bool on) {
    ringing_ = on;
  }

  void playTone(Tone tone) {
  }
  
  
  // EVENTS
  static unifex::type_erased_stream<bool> hookStateStream(); 

  static unifex::type_erased_stream<unsigned> digitStream(); 

  static unifex::type_erased_stream<std::string> numberStream(); 

/////////////////////////////////////


}; // Phone


