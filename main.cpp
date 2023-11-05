#include "Phone.h"
#include "Linphone.h"
#include "util.hpp"

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <cstring>
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


/////////////////// Utils ///////////////////

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


///////////////// Numbers mapping from file //////////////////
constexpr auto kNumberMappingsFileName = "number-mappings.txt";
constexpr auto kSeparator = "-->";

std::unordered_map<std::string, std::string> numberMappings() {
  std::ifstream file(kNumberMappingsFileName, std::ios::in);
  std::unordered_map<std::string, std::string> res;
  
  std::string line;
  while(std::getline(file, line)) {
    if(line.starts_with("#")) {
      std::cout << "Ignore comment in Numbers Mapping: " << line << std::endl;
      continue;
    }

    auto pos = line.find(kSeparator);
    if(pos == std::string::npos) {
      std::cout << "Invalid Numbers Mapping line: " << line << std::endl;
      continue;
    }

    auto key = line.substr(0, pos);
    auto value = line.substr(pos + strlen(kSeparator));
    res[key] = value;
  }

  return res;
}

std::string mapNumber(const std::string& in) {
  auto mappings = numberMappings();
  if(!mappings.contains(in)) {
    std::cout << "No mapping for number: " << in << std::endl;
    return in;
  }

  auto res = mappings[in];
  std::cout << "Mapped number: " << in << " to: " << res << std::endl;
  return res;
}


/////////////////  STATE MACHINE logic //////////////

unifex::sender auto monitorLinphoneEvents(
  std::shared_ptr<Phone> phone, 
  std::shared_ptr<Linphone> linphone, 
  std::shared_ptr<State> state
) {
  return unifex::for_each(
    linphone->eventStream(),
    [state, phone, linphone](const auto event) {
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

        case State::OFF_HOOK_WAITING_NO:
          switch (event) {
            case Linphone::Event::INCOMING_CALL:
              linphone->hangUp();
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
        // irrespective of where we were, hanging up should bring us to a safe, known state
        *state = State::ON_HOOK_WAITING;
        phone->ring(false);
        phone->playTone(Phone::Tone::NONE);
        linphone->hangUp();
      } else {
        switch(*state) {
          case State::ON_HOOK_WAITING:
            *state = State::OFF_HOOK_WAITING_NO;
            phone->playTone(Phone::Tone::CONTINUOUS);
            break;

          case State::ON_HOOK_RINGING:
            *state = State::ACTIVE_CALL;
            phone->ring(false);
            linphone->answer();
            break;

          default:
            printIgnore(state, onHook);
        }
      }
    } 
  ); 
}


// all it does is stop playing a tone when user starts dialling
unifex::sender auto monitorDigitEvents(
  std::shared_ptr<Phone> phone,
  std::shared_ptr<State> state
) {
  return unifex::for_each(
    phone->digitStream(),
    [state, phone] (const unsigned /*digit*/) {
      if( *state == State::OFF_HOOK_WAITING_NO) {
        phone->playTone(Phone::Tone::NONE);
      }
    } 
  );
}


unifex::sender auto monitorNumberEvents(
  std::shared_ptr<Phone> phone,
  std::shared_ptr<Linphone> linphone,
  std::shared_ptr<State> state
) {
  return unifex::for_each(
    phone->numberStream(),
    [state, linphone] (const std::string& number) {
      if( *state == State::OFF_HOOK_WAITING_NO) {
        *state = State::OFF_HOOK_WAITING_REMOTE;
        linphone->dial(mapNumber(number));
      }
    }
  );
}

///////////////// Main Logic ////////////////////////////////////////


template <typename Scheduler>
  requires unifex::scheduler<Scheduler>
unifex::sender auto asyncMain(
  std::shared_ptr<Phone> phone, 
  std::shared_ptr<Subprocess> proc, 
  std::shared_ptr<Sound> sound,
  std::shared_ptr<State> state, 
  Scheduler&& scheduler) {

  return unifex::let_value(
    Linphone::create(proc, sound, std::forward<Scheduler>(scheduler)),
    [phone, state](auto& linphone) {
      return unifex::sequence(
        discard_value(unifex::when_all( // run in parallel
          monitorLinphoneEvents(phone, linphone, state),
          monitorPhoneHookEvents(phone, linphone, state),
          monitorDigitEvents(phone, state),
          monitorNumberEvents(phone, linphone, state)
        )),
        linphone->cleanup()
      );
    }
  );
}


// one and only thread running the whole app
unifex::timed_single_thread_context ctx_;

int main() {
  auto sch = ctx_.get_scheduler();

  auto sound = Sound::create();
  auto phone = Phone::create(20, sound, sch);
  auto proc = std::make_shared<Subprocess>();
  auto state = std::make_shared<State>(State::ON_HOOK_WAITING);

  unifex::sync_wait(
    unifex::on(
      sch,
      unifex::sequence(
        asyncMain(phone, proc, sound, state, sch),
        phone->cleanup()
      )
    )
  );

  return 0;
}
