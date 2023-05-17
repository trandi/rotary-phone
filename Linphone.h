#include "Subprocess.hpp"

#include <iostream>
#include <string>
#include <fmt/core.h>


inline void makeCall(std::string_view destination, Subprocess& proc) {
  std::string cmd = fmt::format("linphonec -s \"sip:{}\"", destination);
  std::cout << "Making call to: " << cmd << std::endl;

  proc.runCmd(cmd);
}


