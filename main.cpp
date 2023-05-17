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


int main() {
  initPhone(); 

  unifex::timed_single_thread_context ctx;

  /*
  Subprocess proc;
  
  proc.runCmd("stress --cpu 4 --timeout 3");
  proc.runCmd("stress --cpu 2 --timeout 5");
   
  unifex::sync_wait(proc.complete());
  */

  unifex::sync_wait(unifex::on(ctx.get_scheduler(), ringS(100, 20)));


  // sync_wait(ring(100, 20));

  //sync_wait(unifex::for_each(numberStream(), [](const string& number) {
  //  cout << "Streamed Number: " << number << endl;
  //}));

  // sync_wait(ring(100, 20));

  //while (true) {
  //  sync_wait(getAndPrintNumber());
  // }

  stopPhone();

  return 0;
}
