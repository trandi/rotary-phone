#pragma once

#include <iostream>
#include <string_view>
#include <string>
#include <cstdlib>
#include <thread>
#include <memory>

#include <unifex/create.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/new_thread_context.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/on.hpp>
#include <unifex/v1/async_scope.hpp>

class Subprocess {
  // this HAS to be in the header to use return type deduction, which we have to
  inline unifex::typed_sender auto doRunCmd(const std::string& cmd) {
    return unifex::create<>([cmd](auto& receiver) {
      // take over the captured var in case sender gests suspended and lambda object dissapears
      std::string c = std::move(cmd);
      std::cout << "Running command: " << c << std::endl;
      system(c.data());
      receiver.set_value();
    });
  }

  unifex::new_thread_context ctx_;
  unifex::v1::async_scope scope_;

public:

  inline void runCmd(const std::string& cmd){
    scope_.detached_spawn_on(ctx_.get_scheduler(), doRunCmd(cmd));
  }
  
  inline auto complete() {
    return scope_.complete();
  }


};
