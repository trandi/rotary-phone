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
  // returns a Sender<bool> (ie. if it was answered or reached max rings)
  template <typename Predicate>
  inline auto ring(unsigned freqHz, unsigned maxCount, Predicate answered) {
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
      unifex::let_value_with(
        [maxCount]() {
          return std::make_unique<int>(maxCount);
        },
        [answered, one, this](auto& count) { 
          return unifex::then(
            unifex::repeat_effect_until(
              one,
              [answered, &count]() {
                return answered() || (--(*count) == 0);
              }
            ),
            [&count, this]() {
              std::cout << "Stopped Ringing" << std::endl;
              setBellMode(false);
              return (*count) != 0; // means it was answered rather than timed out
            }
          );
        }
      )
    );
  }


}; // Phone
