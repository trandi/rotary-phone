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
#include <unifex/timed_single_thread_context.hpp>
#include <unifex/on.hpp>



class  Phone {
  struct NotPubliclyConstructible{};

  void setBellMode(bool enable);
  void hitBell(bool left);

public:
  // use factory method to ensure singleton 
  static std::shared_ptr<Phone> create();
  // allow move
  Phone(Phone&&) = default;
  Phone& operator=(Phone&) = default;

  // given that it controls GPIOs we can't have more than 1 single instance-> forbid copy
  Phone(const Phone&) = delete;
  Phone& operator=(const Phone&) = delete;

  // can't make constructor private due to std::make_shared<> yet we want to force usage of static factory method
  explicit Phone(NotPubliclyConstructible);
  ~Phone();

  bool hookStatus();

  unifex::type_erased_stream<std::string> numberStream();

  // HAS to be run an a timed_single_thread_context (or similar) provided scheduler
  template <typename Predicate>
  inline auto ring(unsigned freqHz, Predicate until) {
    auto delayMs = std::chrono::milliseconds(500 / freqHz); 


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

    return unifex::sequence(
      unifex::just_from([&] {
        std::cout << "Started Ringing..." << std::endl;
        setBellMode(true);
      }),
      unifex::repeat_effect_until(
        one,
        until
      ),
      unifex::just_from([&] {
        std::cout << "Stopped Ringing" << std::endl;
        setBellMode(false);
      })
    );
  }


}; // Phone
