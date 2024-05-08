//
// Created by ZhouK on 2024/5/8.
//

#ifndef THREADPOOL__THREADPOOL_H
#define THREADPOOL__THREADPOOL_H

#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <future>
#include <semaphore>
#include <functional>
#include <type_traits>

#include "Utility.h"
#include "TaskQueue.h"

namespace KTP
{

class ThreadPool
{
  using Task = std::function<void()>;

 public:
  ThreadPool(int num = 1);

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = delete;
  ~ThreadPool();

  void ThreadCycling();

  size_t GetTaskNum();
  size_t GetThreadNum();

  void AddThread(int num);
  void DeleteThread(int num);

  // return void type
  template<typename T, typename F>
  requires std::is_same<T, Normal>::value
      && std::is_void<std::invoke_result_t<F>>::value
  auto submit(F &&task);

  template<typename T, typename F>
  requires std::is_same<T, Urgent>::value
      && std::is_void<std::invoke_result_t<F>>::value
  auto submit(F &&task);

  template<typename T, typename... Fs>
  requires std::is_same<T, Sequence>::value
      && (... && std::is_void<std::invoke_result_t<Fs>>::value)
  auto submit(Fs &&... tasks);

  // return non-void type
  template<typename T, typename F, typename R = std::invoke_result_t<F>>
  requires std::is_same<T, Normal>::value
      && (!std::is_void<std::invoke_result_t<F>>::value)
  auto submit(F &&task) -> std::future<R>;

  template<typename T, typename F, typename R = std::invoke_result_t<F>>
  requires std::is_same<T, Urgent>::value
      && (!std::is_void<std::invoke_result_t<F>>::value)
  auto submit(F &&task) -> std::future<R>;

  template<typename T, typename... Fs>
  requires std::is_same<T, Sequence>::value
      && (!std::disjunction_v<std::is_void<std::invoke_result_t<Fs>>...>)
  auto submit(Fs &&... tasks) -> std::tuple<std::future<std::invoke_result_t<Fs>>...>;

 public:
  std::atomic<bool> isTerminated = false;

  std::atomic<int> free_thread_num_ = 0;
  std::atomic<int> working_thread_num_ = 0;
  std::atomic<int> waiting_delete_thread_num_ = 0;

  std::mutex map_lock_;
  std::map<std::jthread::id, std::jthread> thread_map_;

  TaskQueue<Task> task_queue_;
  std::counting_semaphore<LLONG_MAX> task_semaphore_;
};


/*-------------------------------------------------------------------------------------------------------------------*/
// Implementation
/*-------------------------------------------------------------------------------------------------------------------*/
ThreadPool::ThreadPool(int num)
    : task_semaphore_(0)
{
  AddThread(num);
}

ThreadPool::~ThreadPool()
{
  //TODO: add some code here
}

void ThreadPool::ThreadCycling()
{
  std::function < void() > task;
  while (!isTerminated)
  {
    if (waiting_delete_thread_num_ > 0)
    {
      std::lock_guard<std::mutex> lock(map_lock_);
      thread_map_.erase(std::this_thread::get_id());
      waiting_delete_thread_num_--;
      return;
    }

    task_semaphore_.acquire();
    if (!task_queue_.Size())
    {
      continue;
    }

    task_queue_.Pop(task);

    free_thread_num_--;
    working_thread_num_++;
    task();
    working_thread_num_--;
    free_thread_num_++;
  }
}

size_t ThreadPool::GetTaskNum()
{
  return task_queue_.Size();
}

size_t ThreadPool::GetThreadNum()
{
  std::lock_guard<std::mutex> lock(map_lock_);
  return thread_map_.size();
}

void ThreadPool::AddThread(int num)
{
  num = num > 0 ? num : 1;
  for (int i = 0; i < num; i++)
  {
    std::jthread tmp(&ThreadPool::ThreadCycling, this);
    tmp.detach();
    {
      std::lock_guard<std::mutex> lock(map_lock_);
      thread_map_[tmp.get_id()] = std::move(tmp);
      free_thread_num_++;
    }
  }
}

void ThreadPool::DeleteThread(int num)
{
  num = num > 0 ? num : 1;
  for (int i = 0; i < num; i++)
  {
    waiting_delete_thread_num_++;
    task_semaphore_.release();
  }
}

template<typename T, typename F>
requires std::is_same<T, Normal>::value
    && std::is_void<std::invoke_result_t<F>>::value
auto ThreadPool::submit(F &&task)
{
  task_queue_.PushBack([task]()
                       { task(); });
  task_semaphore_.release();
}

template<typename T, typename F>
requires std::is_same<T, Urgent>::value
    && std::is_void<std::invoke_result_t<F>>::value
auto ThreadPool::submit(F &&task)
{
  task_queue_.PushFront([task]()
                        { task(); });
  task_semaphore_.release();
}

template<typename T, typename... Fs>
requires std::is_same<T, Sequence>::value
    && (... && std::is_void<std::invoke_result_t<Fs>>::value)
auto ThreadPool::submit(Fs &&... tasks)
{
  (std::invoke(std::forward<Fs>(tasks)), ...);
  task_semaphore_.release();
}

template<typename T, typename F, typename R>
requires std::is_same<T, Normal>::value
    && (!std::is_void<std::invoke_result_t<F>>::value)
auto ThreadPool::submit(F &&task) -> std::future<R>
{
  std::function < R() > task_func(std::forward<F>(task));
  std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
  task_queue_.PushBack([task_func, task_promise]()
                       { task_promise->set_value(task_func()); });
  task_semaphore_.release();
  return task_promise->get_future();
}

template<typename T, typename F, typename R>
requires std::is_same<T, Urgent>::value
    && (!std::is_void<std::invoke_result_t<F>>::value)
auto ThreadPool::submit(F &&task) -> std::future<R>
{
  std::function < R() > task_func(std::forward<F>(task));
  std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
  task_queue_.PushFront([task_func, task_promise]()
                        { task_promise->set_value(task_func()); });
  task_semaphore_.release();
  return task_promise->get_future();
}

template<typename T, typename... Fs>
requires std::is_same<T, Sequence>::value
    && (!std::disjunction_v<std::is_void<std::invoke_result_t<Fs>>...>)
auto ThreadPool::submit(Fs &&... tasks) -> std::tuple<std::future<std::invoke_result_t<Fs>>...>
{
  std::tuple<std::shared_ptr<std::promise<std::invoke_result_t<Fs>>>...> task_promises;
  ((std::get<std::shared_ptr<std::promise<std::invoke_result_t<Fs>>>>(task_promises) =
        std::make_shared<std::promise<std::invoke_result_t<Fs>>>()), ...);

  task_queue_.PushBack([task_promises, tasks = std::make_tuple(std::forward<Fs>(tasks)...)]() mutable
                       {
                         std::apply([&task_promises](auto &&... task)
                                    {
                                      ((task_promises.template get<std::shared_ptr<std::promise<std::invoke_result_t<
                                          decltype(task)>>>>()->set_value(task())), ...);
                                    }, tasks);
                       });
  task_semaphore_.release();

  return {task_promises.template get<std::shared_ptr<std::promise<std::invoke_result_t<Fs>>>>()->get_future()...};
}

} // KTP
#endif //THREADPOOL__THREADPOOL_H
