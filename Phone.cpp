#include "Phone.h"

#include <iostream>
#include <unordered_map>
#include <pigpio.h>
#include <chrono>

#include <unifex/on.hpp>
#include <unifex/never.hpp>
#include <unifex/range_stream.hpp>
#include <unifex/transform_stream.hpp>
#include <unifex/task.hpp>
#include <unifex/just.hpp>
#include <unifex/just_done.hpp>
#include <unifex/sequence.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/timed_single_thread_context.hpp>
#include <unifex/async_manual_reset_event.hpp>
#include <unifex/async_auto_reset_event.hpp>

using namespace std;
using namespace unifex;
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

namespace {

timed_single_thread_context sleepExecutionContext;
// returns a Sender that can be co_awaited in a task<>
auto sleep(milliseconds ms) {
  // schedule_after needs a TimedScheduler, which is not the one tasks use
  return schedule_after(sleepExecutionContext.get_scheduler(), ms);
}

unordered_map<Pin, MicrosTick> lastCallback;
bool debounce(Pin pin, MicrosTick tick) {
  auto prevTick = lastCallback[pin];
  lastCallback[pin] = tick;
  // we want at leat 10ms between triggers
  return tick - prevTick > 10000;
}

unsigned currentDigit;
string currentNumber, prevNumber;
async_auto_reset_event numberReady;

void pulseCallback(Pin gpio, int level, MicrosTick tick) {
  if(debounce(gpio, tick)) {
    if(gpio == PIN_DIAL_ENABLE) {
      if(level) {
        currentNumber += to_string(currentDigit); 
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
      cout << "Button pressed, number recorded: " << currentNumber << endl;
      prevNumber = currentNumber;
      currentNumber = "";
      numberReady.set();
    }
  }
} 

} // namespace




void initPhone() {
  cout << "Phone STARTING" << endl;

  cout << "PiGPIO initialisation, version: " << gpioInitialise() << endl;

  cout << "Configure RING: " << (
    gpioSetMode(PIN_RING_ENABLE, PI_OUTPUT) == 0
    ? "success" : "failure") << endl;
 
  cout << "Configure TOGGLE: " << (
    gpioSetMode(PIN_RING_LEFT, PI_OUTPUT) == 0 && gpioSetMode(PIN_RING_RIGHT, PI_OUTPUT) == 0
    ? "success" : "failure") << endl;
 
  cout << "Configure BUTTON: " << (
    gpioSetMode(PIN_BUTTON, PI_INPUT) == 0 &&
    gpioSetPullUpDown(PIN_BUTTON, PI_PUD_UP) == 0 &&
    gpioSetAlertFunc(PIN_BUTTON, buttonCallback) == 0
    ? "success" : "failure") << endl;

  cout << "Configure PULSES: " << (
    gpioSetMode(PIN_DIAL_ENABLE, PI_INPUT) == 0 &&
    gpioSetPullUpDown(PIN_DIAL_ENABLE, PI_PUD_UP) == 0 &&
    gpioSetAlertFunc(PIN_DIAL_ENABLE, pulseCallback) == 0 &&

    gpioSetMode(PIN_DIAL_PULSE, PI_INPUT) == 0 &&
    gpioSetPullUpDown(PIN_DIAL_PULSE, PI_PUD_UP) == 0 &&
    gpioSetAlertFunc(PIN_DIAL_PULSE, pulseCallback) == 0
    ? "success" : "failure") << endl;
}

void stopPhone() {
  cout << "Phone DESTRUCTOR" << endl;
  gpioTerminate();
}

/*
// alternative using a async_manual_reset_event 
task<string> nextNumber() {
  // TODO why the heck doesn't it use the scheduler from the task !??
  co_await on(sleepExecutionContext.get_scheduler(), numberReady.async_wait());
  numberReady.reset();
  co_return prevNumber;
}
*/

type_erased_stream<string> numberStream() {
  return type_erase<string>(transform_stream(numberReady.stream(), []() {
    return prevNumber;
  }));
}


task<void> ring(unsigned  count, unsigned freq) {
  if (gpioWrite(PIN_RING_ENABLE, 1) != 0) {
    cout << "ERROR enabling RING" << endl;
  }

  auto delayMs = milliseconds(1000 / freq / 2);
  for (unsigned i=0; i<count; i++) {
    gpioWrite(PIN_RING_LEFT, 0);
    gpioWrite(PIN_RING_RIGHT, 1);
    co_await sleep(delayMs);
    gpioWrite(PIN_RING_LEFT, 1);
    gpioWrite(PIN_RING_RIGHT, 0);
    co_await sleep(delayMs);
  }

  if (gpioWrite(PIN_RING_ENABLE, 0) != 0) {
    cout << "ERROR disabling RING" << endl;
  }

  co_return;
}
