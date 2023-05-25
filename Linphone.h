#include "Subprocess.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <fmt/core.h>

#include <unifex/sender_concepts.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/repeat_effect_until.hpp>
#include <unifex/v1/async_scope.hpp>
#include <unifex/just.hpp>
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
  template <unifex::typed_sender Ring>
  inline unifex::typed_sender auto monitorIncomingCalls(Ring ringSender, std::function<bool()> offHook) {
    auto delayMs = std::chrono::milliseconds(3000);

    auto senderWhenCallIncoming = unifex::sequence(
                unifex::repeat_effect_until(
                  ringSender,
                  offHook
                ),
                proc_->startCmd("linphonecsh generic 'bla'") // actually answer
              );

    auto senderWhenNoCallIncoming = unifex::just();


    return unifex::repeat_effect(
      unifex::sequence(
        unifex::let_value(
          proc_->runCmd("linphonecsh status hook"),
          [&](const auto& res) -> unifex::variant_sender<decltype(senderWhenCallIncoming), decltype(senderWhenNoCallIncoming)> {
            if(res.find("Incoming call") != std::string::npos) {
              std::cout << "Unbelievable !!!!!" << std::endl;
              return senderWhenCallIncoming;
            } else {
              std::cout << "RIght !!!!!" << std::endl;
              return senderWhenNoCallIncoming;
            }
          }
        ),
        unifex::schedule_after(unifex::current_scheduler, delayMs)
      )
    );
  }


private:

  Subprocess* proc_;
};
