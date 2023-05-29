#include "Linphone.h"

#include <optional>
#include <unifex/sync_wait.hpp>

Linphone::Linphone(Subprocess* proc): proc_(proc) {
  // TODO there should be 1 and only sync_wait in the whole program
  //std::optional<std::shared_ptr<FILE>>  res = 
  //  unifex::sync_wait(proc_->startCmd("linphonecsh init -c ./linphonerc"));

  unifex::sync_wait(proc_->runCmd("linphonecsh init -c ./linphonerc"));
/*
  if(!res.has_value()) {
    throw std::runtime_error("Can't initialise Linphone ");
  } else {
    daemonProcPipe_ = res.value();
    std::cout << "Linphone initialised, linphonecsh running in the background." << std::endl;  
  }
*/
  
}
