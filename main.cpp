#include "Phone.h"
#include "Linphone.h"

#include <iostream>
#include <unifex/sync_wait.hpp>
#include <unifex/for_each.hpp>
#include <unifex/sequence.hpp>
#include <unifex/just.hpp>
#include <unifex/when_all.hpp>
#include <unifex/v1/async_scope.hpp>
#include <unifex/new_thread_context.hpp>
#include <unifex/on.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/inline_scheduler.hpp>
#include <unifex/timed_single_thread_context.hpp>


// one and only thread running the whole app
unifex::timed_single_thread_context ctx_;


enum class State {
  ON_HOOK_WAITING,
  ON_HOOK_RINGING,
  OFF_HOOK_WAITING_NO,
  OFF_HOOK_WAITING_REMOTE,
  ACTIVE_CALL,
  OFF_HOOK_ERROR
};

std::ostream& operator<<(std::ostream& os, const State& state) {
  switch (state) {
    case State::ON_HOOK_WAITING:
      os << "ON_HOOK_WAITING";
      break;
    case State::ON_HOOK_RINGING:
      os << "ON_HOOK_RINGING";
      break;
    case State::OFF_HOOK_WAITING_NO:
      os << "OFF_HOOK_WAITING_NO";
      break;
    case State::OFF_HOOK_WAITING_REMOTE:
      os << "OFF_HOOK_WAITING_REMOTE";
      break;
    case State::ACTIVE_CALL:
      os << "ACTIVE_CALL";
      break;
    case State::OFF_HOOK_ERROR:
      os << "OFF_HOOK_ERROR";
      break;
  }
  return os;
}


void printIgnore(std::shared_ptr<State> state, Linphone::Event event) {
  std::cout << "Ignored | state: " << (*state) << " / Event: " << event << std::endl;
};

void printIgnore(std::shared_ptr<State> state, bool onHook) {
  std::cout << "Ignored | state: " << (*state) << " / Phone Hook status: " 
    << (onHook ? "true" : "false") << std::endl;
};

/////////////////  STATE MACHINE logic //////////////

unifex::sender auto monitorLinphoneEvents(
  std::shared_ptr<Phone> phone, 
  std::shared_ptr<Linphone> linphone, 
  std::shared_ptr<State> state
) {
  return unifex::for_each(
    linphone->eventStream(),
    [state, phone](const auto event) {
      switch (*state) {
        case State::ON_HOOK_WAITING:
          switch (event) {
            case Linphone::Event::INCOMING_CALL:
              *state = State::ON_HOOK_RINGING;
              phone->ring(true);
              break;
            default:
              printIgnore(state, event);
          }
          break;
  
        case State::ON_HOOK_RINGING:
          switch (event) {
            case Linphone::Event::REMOTE_HANGUP:
              *state = State::ON_HOOK_WAITING;
              phone->ring(false);
            default:
              printIgnore(state, event);
          }
          break;

        case State::OFF_HOOK_WAITING_REMOTE:
          switch (event) {
            case Linphone::Event::REMOTE_ANSWER:
              *state = State::ACTIVE_CALL;
              break;
            case Linphone::Event::REMOTE_HANGUP:
              *state = State::OFF_HOOK_ERROR;
              phone->playTone(Phone::Tone::INTERMITENT);
              break;
            default:
              printIgnore(state, event);
          }
          break;

        case State::ACTIVE_CALL:
          switch (event) {
            case Linphone::Event::REMOTE_HANGUP:
              *state = State::OFF_HOOK_ERROR;
              phone->playTone(Phone::Tone::INTERMITENT);
              break;
            default:
              printIgnore(state, event);
          }
          break;

        default:
          printIgnore(state, event);
      }              
    }
  );
}


unifex::sender auto monitorPhoneHookEvents(
  std::shared_ptr<Phone> phone,
  std::shared_ptr<Linphone> linphone,
  std::shared_ptr<State> state
) {
  return unifex::for_each(
    phone->hookStateStream(),
    [state, phone, linphone] (const bool onHook) {
      if(onHook) {
        switch(*state) {
          case State::OFF_HOOK_WAITING_NO:
            *state = State::ON_HOOK_WAITING;
            phone->playTone(Phone::Tone::NONE);
            break;

          case State::OFF_HOOK_WAITING_REMOTE:
            *state = State::ON_HOOK_WAITING;
            linphone->hangUp();
            break;

          case State::OFF_HOOK_ERROR:
            *state = State::ON_HOOK_WAITING;
            phone->playTone(Phone::Tone::NONE);
            break;

          default:
            printIgnore(state, onHook);
        }
      } else if(*state == State::ON_HOOK_WAITING) {
        *state = State::OFF_HOOK_WAITING_NO;
        phone->playTone(Phone::Tone::CONTINUOUS);
      } else {
        printIgnore(state, onHook);
      }
    } 
  ); 
}

/////////////////////////////////////////////////////////


template <typename Scheduler>
  requires unifex::scheduler<Scheduler>
unifex::sender auto asyncMain(
  std::shared_ptr<Phone> phone, 
  std::shared_ptr<Subprocess> proc, 
  std::shared_ptr<State> state, 
  Scheduler&& scheduler) {

  return unifex::let_value(
    Linphone::create(proc, std::forward<Scheduler>(scheduler)),
    [phone, state](auto& linphone) {
      return unifex::let_value(
        unifex::when_all( // run in parallel
          monitorLinphoneEvents(phone, linphone, state),
          monitorPhoneHookEvents(phone, linphone, state)
        ),
        [linphone] (auto& ...) {
          // need to swallow/ignore the result from when_all
          return linphone->cleanup();
        }
      );
    }
  );
}



int main() {
  auto sch = ctx_.get_scheduler();

  auto phone = Phone::create(20, sch);
  auto proc = std::make_shared<Subprocess>();
  auto state = std::make_shared<State>(State::ON_HOOK_WAITING);

  unifex::sync_wait(
    unifex::on(
      sch,
      unifex::sequence(
        asyncMain(phone, proc, state, sch),
        phone->cleanup()
      )
    )
  );

  return 0;
}
