#include <string>

#include <unifex/task.hpp>
#include <unifex/type_erased_stream.hpp>

void initPhone();
void stopPhone();

unifex::task<void> ring(unsigned count, unsigned freq);
unifex::type_erased_stream<std::string> numberStream();
