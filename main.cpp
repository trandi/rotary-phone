#include "Phone.h"
#include "Linphone.h"

#include <iostream>
#include <unifex/sync_wait.hpp>
#include <unifex/task.hpp>
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


unifex::timed_single_thread_context ctx;

/*
unifex::typed_sender auto  asyncMain(Subprocess* proc) {
  Linphone linphone(proc);
  
  return linphone.monitorIncomingCalls(
    
  );

}
*/
int main() {
  auto phone = Phone::create();
  unifex::v1::async_scope scope;
  Subprocess proc(&scope);
  Linphone linphone(&proc);

  scope.detached_spawn_on(
    ctx.get_scheduler(), 
    linphone.monitorIncomingCalls(
      unifex::just_from([](){std::cout << "RINGING !" << std::endl;}),//phone->ring(10, 20),
      [&phone]() { return !phone->hookStatus(); }
    )
  );

  unifex::sync_wait(scope.complete());


  // unifex::sync_wait(unifex::on(ctx.get_scheduler(), ring(100, 20)));


  // sync_wait(ring(100, 20));

  //sync_wait(unifex::for_each(numberStream(), [](const string& number) {
  //  cout << "Streamed Number: " << number << endl;
  //}));

  // sync_wait(ring(100, 20));

  //while (true) {
  //  sync_wait(getAndPrintNumber());
  // }


  return 0;
}
