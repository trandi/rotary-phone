#include "Phone.h"

#include <iostream>
#include <unifex/sync_wait.hpp>
#include <unifex/task.hpp>
#include <unifex/for_each.hpp>

using namespace std;
using namespace unifex;

task<void> getAndPrintNumber() {
  auto number = co_await nextNumber();
  cout << "Number: " << number << endl;
}

int main() {
  initPhone(); 

  sync_wait(unifex::for_each(numberStream(), [](const string& number) {
    cout << "Streamed Number: " << number << endl;
  }));

  // sync_wait(ring(100, 20));

  //while (true) {
  //  sync_wait(getAndPrintNumber());
  // }

  stopPhone();

  return 0;
}
