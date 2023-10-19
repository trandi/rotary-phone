#include "Subprocess.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <fmt/core.h>

#include <unifex/on.hpp>
#include <unifex/sender_concepts.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/repeat_effect_until.hpp>
#include <unifex/v1/async_scope.hpp>
#include <unifex/any_sender_of.hpp>
#include <unifex/timed_single_thread_context.hpp>
#include <unifex/variant_sender.hpp>
#include <unifex/async_auto_reset_event.hpp>
#include <unifex/transform_stream.hpp>

class Linphone {
public:
  enum class Event {
    INCOMING_CALL,
    REMOTE_ANSWER,
    REMOTE_HANGUP
  };


private:
  struct NotPubliclyConstructible{};

  unifex::async_auto_reset_event statusChange_;
  Event lastEvent_{Event::REMOTE_HANGUP}; 

  unifex::async_manual_reset_event actionCmdEvent_;
  std::string actionCmd_;

  std::shared_ptr<Subprocess> proc_;
  std::shared_ptr<Sound> sound_;
  unifex::async_scope backgroundScope_;

  

  unifex::sender auto handleEvents() {
    auto checkOnce = unifex::then(
      proc_->runCmd("linphonecsh status hook"),     
      [&](const std::string statusRes) {
        std::string_view status(statusRes);
        // std::cout << "Linphone status: " << status << std::endl;
        if(lastEvent_ != Event::INCOMING_CALL && status.starts_with("Incoming call")) {
          lastEvent_ = Event::INCOMING_CALL;
          statusChange_.set();
        } else if(lastEvent_ != Event::REMOTE_ANSWER && status.starts_with("hook=answwered")) {
          lastEvent_ = Event::REMOTE_ANSWER;
          statusChange_.set();
        } else if(lastEvent_ != Event::REMOTE_HANGUP && status.starts_with("hook=on-hook")) {
          lastEvent_ = Event::REMOTE_HANGUP;
          statusChange_.set();
        }
      }
    );

    // infinite loop
    return unifex::repeat_effect(
      unifex::sequence(
        checkOnce,
        // wait a bit before checking again
        unifex::schedule_after(std::chrono::milliseconds(1000))
      )
    ); 
  }


  unifex::sender auto handleActions() {
    // sender<std::string>
    auto waitAndRunCmd = unifex::let_value(
      actionCmdEvent_.async_wait(), // wait for command to arrive
      [&] {
        auto cmd = actionCmd_;
        actionCmdEvent_.reset();
        return proc_->runCmd(cmd);
      }
    );


    // infinite loop
    return unifex::repeat_effect(
      // map to sender<void> to please repeat_effect
      unifex::then(
        waitAndRunCmd,
        [](const auto& res) {
          // swallow the result
        } 
      )
    );
  }


public:
  // returns sender<std::shared_ptr<Linphone>>
  template <typename Scheduler>
    requires unifex::scheduler<Scheduler>
  static unifex::sender auto create(
    std::shared_ptr<Subprocess> proc, 
    std::shared_ptr<Sound> sound,  
    Scheduler&& scheduler
  ) {
    return unifex::then(  
      proc->runCmd("linphonecsh init -c ./linphonerc"),
      [proc, sound, &scheduler] (const auto& startRes) {
        std::cout << "Started linphone daemon: " << startRes << std::endl;
        
        return std::make_shared<Linphone>(
          NotPubliclyConstructible{}, 
          std::move(proc),
          std::move(sound),
          std::forward<Scheduler>(scheduler)
        );
      }
    );
  }

  // don't allow move or copy 
  Linphone(Linphone&&) = delete;
  Linphone& operator=(Linphone&) = delete;
  Linphone(const Linphone&) = delete;
  Linphone& operator=(const Linphone&) = delete;

  template <typename Scheduler>
  requires unifex::scheduler<Scheduler> //has to be Timed scheduler
  explicit Linphone(
    NotPubliclyConstructible, 
    std::shared_ptr<Subprocess> proc,
    std::shared_ptr<Sound> sound, 
    Scheduler&& scheduler): proc_(std::move(proc)), sound_(std::move(sound)) 
  {
    // BACKGROUND tasks
    backgroundScope_.detached_spawn_on(
      std::forward<Scheduler>(scheduler),
      handleEvents()
    ); 

    backgroundScope_.detached_spawn_on(
      std::forward<Scheduler>(scheduler),
      handleActions()
    );
  }

  ~Linphone() = default;

  // async cleanup
  unifex::sender auto cleanup() {
    return backgroundScope_.cleanup();
  }

  ////////////   API   /////////////////////

  // ACTIONS
  void dial(std::string_view destination) {
    std::string cmd = fmt::format("linphonecsh dial \"sip:{}\"", destination);
    std::cout << "Making call to: " << cmd << std::endl;
    sound_->mixer->setVolume(70);

    actionCmd_ = cmd;
    actionCmdEvent_.set();
  }

  void hangUp() {
    std::cout << "Terminate call" << std::endl;
    sound_->mixer->setVolume(3);
    actionCmd_ = "linphonecsh generic 'terminate'";
    actionCmdEvent_.set();
  }

  void answer() {
    std::cout << "Answering call" << std::endl;
    sound_->mixer->setVolume(70);
    actionCmd_ = "linphonecsh generic 'answer'";
    actionCmdEvent_.set();
  } 


  // EVENTS
  unifex::type_erased_stream<Event> eventStream() {
    return unifex::type_erase<Event>(
      unifex::transform_stream(statusChange_.stream(), [&] {
        return lastEvent_;
      })
    );
  }  


  /////////////////////////////////////

};



std::ostream& operator<<(std::ostream& os, const Linphone::Event& event) {
  switch (event) {
    case Linphone::Event::INCOMING_CALL:
      os << "INCOMING_CALL";
      break;
    case Linphone::Event::REMOTE_ANSWER:
      os << "REMOTE_ANSWER";
      break;
    case Linphone::Event::REMOTE_HANGUP:
      os << "REMOTE_HANGUP";
      break;
  }
  return os;
}
