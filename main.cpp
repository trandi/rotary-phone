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


unifex::timed_single_thread_context ctx_;


auto incomingCalls(std::shared_ptr<Phone> phone, std::shared_ptr<Linphone> linphone) {
  return linphone->monitorIncomingCalls(
    phone->ring(
      20, // ring freq in Hz
      15, // max no of rings
      [phone]() { // returns if call is currently answered or not
        return !phone->hookStatus();
      }
    )
  );
}

auto outgoingCalls(std::shared_ptr<Phone> phone, std::shared_ptr<Linphone> linphone) {
  return unifex::for_each(
    phone->numberStream(),
    [](const std::string& number) {
      // TODO actually DIAL it, rather than just printing
      std::cout << "Number received: " << number << std::endl;
    }
  );
}

unifex::sender auto asyncMain(std::shared_ptr<Phone> phone, Subprocess* proc) {
  return unifex::let_value(
    Linphone::create(proc),
    [phone](auto& linphone) {
      return unifex::when_all(
        incomingCalls(phone, linphone),
        outgoingCalls(phone, linphone)
      );
    }
  );
}



int main() {
  auto phone = Phone::create();
  Subprocess proc;

  unifex::sync_wait(
    unifex::on(
      ctx_.get_scheduler(),
      asyncMain(phone, &proc)
    )
  );

  return 0;
}
