#ifndef RANDOM_GENERATOR_H
#define RANDOM_GENERATOR_H

#include <coroutine>

struct RandomGenerator
{
  struct promise_type;
  using handle_type = std::coroutine_handle<promise_type>;

  struct promise_type
  {
    int value;

    RandomGenerator get_return_object() {
      return RandomGenerator(handle_type::from_promise(*this));
    }

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() {}
    
    template<std::convertible_to<int> From> // C++20 concept
    std::suspend_always yield_value(From &&from) {
      value = from;
      return {};
    }
    void return_void() {}
  };
 
  handle_type _h;
 
  RandomGenerator(handle_type h) : _h(h) {}
  ~RandomGenerator() { _h.destroy(); }

  int operator()() { return _h.promise().value;  }
};

#endif /* RANDOM_GENERATOR_H */
