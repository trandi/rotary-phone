#pragma once

#include <unifex/then.hpp>
#include <unifex/sender_concepts.hpp>

template<typename S> requires unifex::sender<S>
unifex::sender auto discard_value(S&& sender) noexcept {
  return unifex::then(
   (S&&) sender,
    [](auto&& ...) {
      // just discard/swallow the result
    }
  );
}
