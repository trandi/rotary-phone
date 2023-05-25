#pragma once

#include <iostream>
#include <string_view>
#include <string>
#include <thread>
#include <memory>
#include <fstream>
#include <cstdio>

#include <unifex/create.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/new_thread_context.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/on.hpp>
#include <unifex/v1/async_scope.hpp>
#include <unifex/let_value.hpp>
#include <unifex/just.hpp>


class Subprocess {

  // this HAS to be in the header to use return type deduction, which we have to
  inline unifex::typed_sender auto doRunCmd(const std::string& cmd) {
    return unifex::create<std::string>([cmd](auto& receiver) {
      // take over the captured var in case sender gests suspended and lambda object dissapears
      std::string c = std::move(cmd);
      std::cout << "Running command: " << c << std::endl;
      auto res = Subprocess::exec(cmd.data());
      receiver.set_value(std::move(res));
    });
  }


public:

  explicit Subprocess(unifex::v1::async_scope* scope): scope_(scope){
  }

  // starts a command in the background and does NOT wait for it
  // returns a sender<void>
  inline unifex::typed_sender auto startCmd(const std::string& cmd) {
    // wrap the spawning in a sender so as to start only when connected to a receiver
    return unifex::create<void>([this, cmd](auto& receiver) {
      // spawning detached means no one can inspect the result -> it requires sender<void>
      scope_->detached_spawn_on(ctx_.get_scheduler(), unifex::let_value(doRunCmd(cmd), [](auto) {
        // discard the returned output
        return unifex::just();
      }));

      receiver.set_value();
    });
  }

  // starts a command and waits for it to finish
  // returns the stdout from console
  inline unifex::typed_sender auto runCmd(const std::string& cmd) {
    return doRunCmd(cmd);
  }

private:

  // runs a command and grabs the output
  static std::string exec(const char* cmd) {
    std::string res;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);

    if(pipe) {
      char buff[256];
      while (!feof(pipe.get())) {
        if(fgets(buff, 256, pipe.get())) {
          res += buff;
        }
      }
    }

    return res;
  }

  

  unifex::new_thread_context ctx_;
  unifex::v1::async_scope* scope_;

};
