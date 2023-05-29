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

struct Linphone {


  explicit Linphone(Subprocess* proc);


  // TODO 
  inline unifex::typed_sender auto  makeCall(std::string_view destination) {
    std::string cmd = fmt::format("linphonec -s \"sip:{}\"", destination);
    std::cout << "Making call to: " << cmd << std::endl;

    return proc_->runCmd(cmd);
  }


  // HAS to be run on a timed_single_thread or similar scheduler
  template <typename RingSender>
  inline unifex::typed_sender auto monitorIncomingCalls(RingSender ringSender) {
    auto delayMs = std::chrono::milliseconds(3000);
    
    auto senderWhenCallIncoming = 
      unifex::then(
        unifex::let_value(
          ringSender, // should stop when we're ready to answer
          [p = proc_]() {
            std::cout << "~~~~~~ 3" << std::endl;
            return p->runCmd("linphonecsh generic 'answer'"); // actually answer
          }
        ),
        [](const auto& answerRes) {
          std::cout << "~~~~~ 4: " << answerRes << std::endl;
          // swallow returned value to make this a Sender<void>
        }
      );

    return unifex::repeat_effect(
      unifex::sequence(
        unifex::let_value(
          proc_->runCmd("linphonecsh status hook"),
          [&](const auto& statusRes) 
            -> unifex::variant_sender<decltype(unifex::just()), decltype(senderWhenCallIncoming)> {
            if(statusRes.find("Incoming call") != std::string::npos) {
              std::cout << "Incoming call detected... " << std::endl;
              return senderWhenCallIncoming; 
            } else {
              // if no incoming call, do nothing
              return unifex::just();
            }
          }
        ),
        unifex::schedule_after(unifex::current_scheduler, delayMs)
      )
    );
  }


private:

  Subprocess* proc_;
  std::shared_ptr<FILE> daemonProcPipe_;
  unifex::timed_single_thread_context ctx_;
};
