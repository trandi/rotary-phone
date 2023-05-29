#pragma once

#include <string>
#include <chrono>
#include <memory>
#include <iostream>

#include <unifex/task.hpp>
#include <unifex/type_erased_stream.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/sequence.hpp>
#include <unifex/just_from.hpp>
#include <unifex/repeat_effect_until.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/let_value.hpp>
#include <unifex/timed_single_thread_context.hpp>
#include <unifex/on.hpp>



class  Phone {
  struct NotPubliclyConstructible{};

  void setBellMode(bool enable);
  void hitBell(bool left);

  // TODO unused
  unifex::timed_single_thread_context ctx_;

public:
  // use factory method to ensure singleton 
  // TODO use unique_ptr ??
  static std::shared_ptr<Phone> create();

  Phone(Phone&&) = default;
  Phone& operator=(Phone&) = default;

  // can't make constructor private as we want it to work with std::make_shared<> yet we want to force usage of static factory method
  explicit Phone(NotPubliclyConstructible);
  ~Phone();

  // given that it controls GPIOs we can't have more than 1 single instance
  Phone(const Phone&) = delete;
  Phone& operator=(const Phone&) = delete;

  //unifex::task<void> ring(unsigned count, unsigned freq);
  //unifex::type_erased_stream<std::string> numberStream();


  bool hookStatus();

  // HAS to be run an a timed_single_thread_context (or similar) provided scheduler
  template <typename Predicate>
  inline unifex::typed_sender auto ring(unsigned freq, Predicate until) {
    auto delayMs = std::chrono::milliseconds(1000 / freq / 2);

    auto one = unifex::sequence(  
      unifex::just_from([&] {
        std::cout << "HitBell TRUE" << std::endl;
        hitBell(true);
      }),
      unifex::schedule_after(unifex::current_scheduler, delayMs),
      unifex::just_from([&] {
        std::cout<< "HitBell FALSE" << std::endl;
        hitBell(false);
      }),
      unifex::schedule_after(unifex::current_scheduler, delayMs) 
    );

    return unifex::sequence(
      unifex::just_from([&] {
        std::cout << "Started Ringing..." << std::endl;
        setBellMode(true);
      }),
      unifex::repeat_effect_until(
        unifex::sequence(one, one, one, one, one, one),
        until
      ),
      unifex::just_from([&] {
        std::cout << "Stopped Ringing" << std::endl;
        setBellMode(false);
      })
    );
  }


}; // Phone
