#ifndef CONCURRENT_QUEUE_H
#define CONCURRENT_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class ConcurrentQueue : public std::queue<T>
{
  std::mutex push_mutex;
  std::mutex pop_mutex;
  std::condition_variable is_empty;
  
public:
  ConcurrentQueue() = default;
  virtual ~ConcurrentQueue() = default;

  T pop();
  
  void push(T&& elt) noexcept;
  void push(const T& elt);
};

template<typename T>
T ConcurrentQueue<T>::pop()
{
  std::unique_lock<std::mutex> lock(pop_mutex);
  while(std::queue<T>::empty()) is_empty.wait(lock);
  T elt = std::queue<T>::front();
  std::queue<T>::pop();
  return elt;
}

template<typename T>
void ConcurrentQueue<T>::push(T&& elt) noexcept
{
  std::lock_guard<std::mutex> lock(push_mutex);
  std::queue<T>::push(std::move(elt));
  is_empty.notify_all();
}

template<typename T>
void ConcurrentQueue<T>::push(const T& elt)
{
  std::lock_guard<std::mutex> lock(push_mutex);
  std::queue<T>::push(elt);
  is_empty.notify_all();
}


#endif /* CONCURRENT_QUEUE_H */
