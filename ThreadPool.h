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

  void AddThread(int num);
  void DeleteThread(int num);

  size_t GetTaskNum();
  size_t GetThreadNum();

  // return void type
  template<typename T, typename F>
  requires std::is_same<T, Normal>::value
	  && std::is_void<std::invoke_result_t<F>>::value
  auto submit(F &&task);

  template<typename T, typename F>
  requires std::is_same<T, Urgent>::value
	  && std::is_void<std::invoke_result_t<F>>::value
  auto submit(F &&task);

  template<typename T, typename F, typename... Fs>
  requires std::is_same<T, Sequence>::value
	  && std::is_void<std::invoke_result_t<F>>::value
	  && (... && std::is_void<std::invoke_result_t<Fs>>::value)
  auto submit(F &&task, Fs &&... tasks);

  // return non-void type
  template<typename T = normal, typename F, typename R = std::invoke_result_t<F>>
  requires std::is_same<T, Normal>::value
	  && (!std::is_void<std::invoke_result_t<F>>::value)
  auto submit(F &&task) -> std::future<R>;

  template<typename T, typename F, typename R = std::invoke_result_t<F>>
  requires std::is_same<T, Urgent>::value
	  && (!std::is_void<std::invoke_result_t<F>>::value)
  auto submit(F &&task) -> std::future<R>;

  template<typename T, typename F, typename... Fs>
  requires std::is_same<T, Sequence>::value
	  && std::is_void<std::invoke_result_t<F>>::value
	  && (!std::conjunction_v<std::is_void<std::invoke_result_t<Fs>>...>)
  auto submit(F &&task, Fs &&... tasks);

public:
  bool isTerminated = false;

  std::atomic<int> free_thread_num_ = 0;
  std::atomic<int> working_thread_num_ = 0;
  std::atomic<int> waiting_delete_thread_num_ = 0;

  std::mutex map_lock_;
  std::map<std::thread::id, std::thread> thread_map_;

  TaskQueue<Task> task_queue_;
  std::counting_semaphore<LLONG_MAX> task_semaphore_;
};

} // KTP

#endif //THREADPOOL__THREADPOOL_H
