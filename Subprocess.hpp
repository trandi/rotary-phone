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


struct Subprocess {


/*
  // starts a command in the background and does NOT wait for it
  // returns a sender<void>
  inline unifex::typed_sender auto startCmd(const std::string& cmd) {
    // wrap the spawning in a sender so as to start only when connected to a receiver
    return unifex::create<std::shared_ptr<FILE>>([this, cmd](auto& receiver) {
      std::cout << "Popen cmd: " << cmd << std::endl;
      std::shared_ptr<FILE> pipe(popen(cmd.data(), "r"), pclose);
      receiver.set_value(pipe);
    });
  }
*/

  // starts a command and waits for it to finish
  // returns the stdout from console
  // this HAS to be in the header to use return type deduction, which we have to
  inline auto runCmd(const std::string& cmd) {
    return unifex::just_from([cmd]() {
      return Subprocess::exec(cmd.data());
    });
  }

private:

  // runs a command and grabs the output
  static std::string exec(const std::string& cmd) {
    // program needs to run as root because of PIGPIO, make sure that commands run as pi
    std::string actualCmd = "su pi -c '" + cmd + "'";
    std::cout << "Popen cmd and wait for output: " << actualCmd << std::endl;
    std::shared_ptr<FILE> pipe(popen(actualCmd.data(), "r"), pclose);

    std::string res("");
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

};
