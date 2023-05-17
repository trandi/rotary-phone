#pragma once

#include <string>
#include <chrono>

#include <unifex/task.hpp>
#include <unifex/type_erased_stream.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/sequence.hpp>
#include <unifex/just_from.hpp>
#include <unifex/repeat_effect_until.hpp>
#include <unifex/scheduler_concepts.hpp>

void initPhone();
void stopPhone();

unifex::task<void> ring(unsigned count, unsigned freq);
unifex::type_erased_stream<std::string> numberStream();


void setBellMode(bool enable);
void hitBell(bool left);


// HAS to be run an a timed_single_thread_context (or similar) provided scheduler
inline unifex::typed_sender auto ringS(unsigned count, unsigned freq) {
  auto delayMs = std::chrono::milliseconds(1000 / freq / 2);

  auto one = unifex::sequence(  
    unifex::just_from([] {
      hitBell(true);
    }),
    unifex::schedule_after(unifex::current_scheduler, delayMs),
    unifex::just_from([] {
      hitBell(false);
    }),
    unifex::schedule_after(unifex::current_scheduler, delayMs) 
  );

  return unifex::sequence(
    unifex::just_from([] {
      setBellMode(true);
    }),
    unifex::repeat_effect_until(
      one,
      [&]() {
        return count-- < 0;
      }
    ),
    unifex::just_from([] {
      setBellMode(false);
    })
  );
}
