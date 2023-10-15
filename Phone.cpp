#include "Phone.h"

#include <iostream>
#include <unordered_map>
#include <pigpio.h>
#include <chrono>

#include <unifex/on.hpp>
#include <unifex/never.hpp>
#include <unifex/range_stream.hpp>
#include <unifex/transform_stream.hpp>
#include <unifex/just.hpp>
#include <unifex/just_done.hpp>
#include <unifex/sequence.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/timed_single_thread_context.hpp>
#include <unifex/async_manual_reset_event.hpp>
#include <unifex/async_auto_reset_event.hpp>

using namespace std::chrono;
using namespace std::chrono_literals;


using Digit = uint8_t;
using Pin = int;
using MicrosTick = uint32_t;

// GPIO
constexpr Pin PIN_RING_ENABLE = 2; // Connector PIN 3
constexpr Pin PIN_RING_LEFT = 3; // Connector PIN 5
constexpr Pin PIN_RING_RIGHT = 4; // Connector PIN 7
// below are connecting to GND when triggered, so should be normally Pulled UP
constexpr Pin PIN_BUTTON = 14; // Connector PIN 8
constexpr Pin PIN_DIAL_ENABLE = 17; // Connector PIN 10
constexpr Pin PIN_DIAL_PULSE = 15; // Connector PIN 11
constexpr Pin PIN_HOOK = 18; // Connector PIN 12

namespace {

std::unordered_map<Pin, MicrosTick> lastCallback;
bool debounce(Pin pin, MicrosTick tick) {
  auto prevTick = lastCallback[pin];
  lastCallback[pin] = tick;
  // we want at leat 10ms between triggers
  return tick - prevTick > 10000;
}

unsigned currentDigit, prevDigit;
std::string currentNumber, prevNumber;
unifex::async_auto_reset_event digitReady, numberReady;

void pulseCallback(Pin gpio, int level, MicrosTick tick) {
  if(debounce(gpio, tick)) {
    if(gpio == PIN_DIAL_ENABLE) {
      if(level) {
        // just obtained a new digit
        prevDigit = currentDigit;
        currentNumber += std::to_string(prevDigit);
        digitReady.set();
      } else {
        currentDigit = 0;
      }
    } else if(gpio == PIN_DIAL_PULSE && level) {
      currentDigit = (currentDigit + 1) % 10;
    }
  }
} 


void buttonCallback(Pin gpio, int level, MicrosTick tick) {
  if(debounce(gpio, tick)) {
    if(level) {
      std::cout << "Button pressed, number recorded: " << currentNumber << std::endl;
      prevNumber = currentNumber;
      currentNumber = "";
      numberReady.set();
    }
  }
} 

bool hook{true}; //HAS to start with the hook in place
unifex::async_auto_reset_event hookStatusChange;

void hookCallback(Pin gpio, int level, MicrosTick tick) {
  if(debounce(gpio, tick)) {
    hook = level;
    hookStatusChange.set();
    std::cout << "Hook: " << (hook ? "true" : "false") << std::endl;
  }
}
 
} // namespace



///////////  PhoneHelper //////////


/*static*/ void Phone::hardwareSetup() {
  std::cout << "PiGPIO initialisation, version: " << gpioInitialise() << std::endl;

  std::cout << "Configure RING: " << (
    gpioSetMode(PIN_RING_ENABLE, PI_OUTPUT) == 0
    ? "success" : "failure") << std::endl;
 
  std::cout << "Configure TOGGLE: " << (
    gpioSetMode(PIN_RING_LEFT, PI_OUTPUT) == 0 && gpioSetMode(PIN_RING_RIGHT, PI_OUTPUT) == 0
    ? "success" : "failure") << std::endl;
 
  std::cout << "Configure BUTTON: " << (
    gpioSetMode(PIN_BUTTON, PI_INPUT) == 0 &&
    gpioSetPullUpDown(PIN_BUTTON, PI_PUD_UP) == 0 &&
    gpioSetAlertFunc(PIN_BUTTON, buttonCallback) == 0
    ? "success" : "failure") << std::endl;

  std::cout << "Configure PULSES: " << (
    gpioSetMode(PIN_DIAL_ENABLE, PI_INPUT) == 0 &&
    gpioSetPullUpDown(PIN_DIAL_ENABLE, PI_PUD_UP) == 0 &&
    gpioSetAlertFunc(PIN_DIAL_ENABLE, pulseCallback) == 0 &&

    gpioSetMode(PIN_DIAL_PULSE, PI_INPUT) == 0 &&
    gpioSetPullUpDown(PIN_DIAL_PULSE, PI_PUD_UP) == 0 &&
    gpioSetAlertFunc(PIN_DIAL_PULSE, pulseCallback) == 0
    ? "success" : "failure") << std::endl;

  std::cout << "Configure HOOK: " << (
    gpioSetMode(PIN_HOOK, PI_INPUT) == 0 &&
    gpioSetPullUpDown(PIN_HOOK, PI_PUD_UP) == 0 &&
    gpioSetAlertFunc(PIN_HOOK, hookCallback) == 0
    ? "success" : "failure") << std::endl;
}

/*static*/ void Phone::hardwareTerminate() {
  gpioTerminate();
}


/*static*/ void Phone::setBellMode(bool enabled) {
  gpioWrite(PIN_RING_ENABLE, enabled);
}

/*static*/ void Phone::hitBell(bool left) {
  if(left) {
    gpioWrite(PIN_RING_LEFT, 1);
    gpioWrite(PIN_RING_RIGHT,0);
  } else {
    gpioWrite(PIN_RING_LEFT, 0);
    gpioWrite(PIN_RING_RIGHT, 1);
  }
}


/*static*/ unifex::type_erased_stream<bool> Phone::hookStateStream() {
  return unifex::type_erase<bool>(
    unifex::transform_stream(hookStatusChange.stream(), []() {
      return hook;
    })
  );
}

/*static*/ unifex::type_erased_stream<unsigned> Phone::digitStream() {
  return unifex::type_erase<unsigned>(
    unifex::transform_stream(digitReady.stream(), []() {
      return prevDigit;
    })
  );
}

/*static*/ unifex::type_erased_stream<std::string> Phone::numberStream() {
  return unifex::type_erase<std::string>(
    unifex::transform_stream(numberReady.stream(), []() {
      return prevNumber;
    })
  );
}
