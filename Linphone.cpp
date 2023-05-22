#include "Linphone.h"

#include <optional>
#include <unifex/sync_wait.hpp>

Linphone::Linphone(Subprocess* proc): proc_(proc) {
  // TODO there should be 1 and only sync_wait in the whole program
  std::optional<std::string>  res = 
    unifex::sync_wait(proc_->runCmd("linphonecsh init -c ~/.config/linphone/linphonerc"));
  
  if(!res.has_value() || !res.value().empty()) {
    throw std::runtime_error("Can't initialise Linphone: " + res.value_or("error"));
  }

  
}
