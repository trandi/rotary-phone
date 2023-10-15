#include "Subprocess.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <fmt/core.h>

#include <unifex/on.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/repeat_effect_until.hpp>
#include <unifex/v1/async_scope.hpp>
#include <unifex/any_sender_of.hpp>
#include <unifex/timed_single_thread_context.hpp>
#include <unifex/variant_sender.hpp>

class Linphone {
  struct NotPubliclyConstructible{};

public:
  // returns sender<std::shared_ptr<Linphone>>
  static unifex::sender auto create(Subprocess* proc) {
    return unifex::then(  
      proc->runCmd("linphonecsh init -c ./linphonerc"),
      [&](const auto& startRes) {
        std::cout << "Started linphone daemon: " << startRes << std::endl;
        
        return std::make_shared<Linphone>(NotPubliclyConstructible{}, proc);
      }
    );
  }

  // allow move
  Linphone(Linphone&&) = default;
  Linphone& operator=(Linphone&) = default;
  // 1 single linphonec daemon so no copies
  Linphone(const Linphone&) = delete;
  Linphone& operator=(const Linphone&) = delete;

  explicit Linphone(NotPubliclyConstructible, Subprocess* proc): proc_(proc) {}
  ~Linphone() = default;


  // TODO 
  inline auto  makeCall(std::string_view destination) {
    std::string cmd = fmt::format("linphonec -s \"sip:{}\"", destination);
    std::cout << "Making call to: " << cmd << std::endl;

    return proc_->runCmd(cmd);
  }


  // HAS to be run on a timed_single_thread or similar scheduler
  // RingSender needs to be a Sender<bool>
  template <typename RingSender>
  inline auto monitorIncomingCalls(RingSender ringSender) {
    auto delayMs = std::chrono::milliseconds(1000);

    // unifex::sender<void>
    auto answerCmdSender = unifex::then(
      proc_->runCmd("linphonecsh generic 'answer'"),
      [](const auto& statusRes) {
        std::cout << "Answered: " << statusRes << std::endl;
      }
    );

    // has to be unifex::sender<void> so that we can use it in unifex::repeat_effect  
    auto senderWhenCallIncoming = 
      unifex::let_value(
          ringSender, // should return a bool indicating if it was answered or not
          [answerCmdSender](const bool& answered)
            -> unifex::variant_sender<decltype(unifex::just()), decltype(answerCmdSender)> {
            std::cout << "Ring finished: " << answered << std::endl;
            if(answered) {
              return answerCmdSender; // actually answer
            } else {
              return unifex::just(); // do nothing
            }
          }
      );


    return unifex::repeat_effect(
      unifex::sequence(
        unifex::let_value(
          proc_->runCmd("linphonecsh status hook"),
          [senderWhenCallIncoming](const auto& statusRes) 
            -> unifex::variant_sender<decltype(unifex::just()), decltype(senderWhenCallIncoming)> {
            if(statusRes.find("Incoming call") != std::string::npos) {
              std::cout << "Incoming call detected... " << std::endl;
              return senderWhenCallIncoming; 
            } else {
              return unifex::just(); // no incoming call, do nothing
            }
          }
        ),
        unifex::schedule_after(delayMs) // wait a bit before repeating
      )
    );
  }


private:
  Subprocess* proc_;
};
