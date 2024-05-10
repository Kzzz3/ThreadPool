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

  void ThreadLoop();

  size_t GetTaskNum();
  size_t GetThreadNum();

  void AddThread(int num = 1);
  void DeleteThread(int num = 1);

  template<typename TF>
  auto WaitTasksDone(TF &futures);

  template<typename ...Futures>
  auto WaitTasksDone(Futures &... futures);

  // return non-void type
  template<typename T, typename F, typename R = std::invoke_result_t<F>>
  requires std::is_same_v<T, Normal>
  auto Submit(F &&task) -> std::future<R>;

  template<typename T, typename F, typename R = std::invoke_result_t<F>>
  requires std::is_same_v<T, Urgent>
  auto Submit(F &&task) -> std::future<R>;

  template<typename T, typename... Fs>
  requires std::is_same_v<T, Sequence>
  auto Submit(Fs &&... tasks) -> std::tuple<std::future<std::invoke_result_t<Fs>>...>;

 private:
  std::atomic<bool> is_terminated_ = false;

  std::atomic<int> free_thread_num_ = 0;
  std::atomic<int> working_thread_num_ = 0;
  std::atomic<int> waiting_delete_thread_num_ = 0;

  TaskQueue<Task> task_queue_;
  std::counting_semaphore<LLONG_MAX> task_num_semaphore_;

  std::mutex map_lock_;
  std::map<std::jthread::id, std::jthread> thread_map_;
};


/*-------------------------------------------------------------------------------------------------------------------*/
// Implementation
/*-------------------------------------------------------------------------------------------------------------------*/
ThreadPool::ThreadPool(int num)
	: task_num_semaphore_(0)
{
  AddThread(num);
}

ThreadPool::~ThreadPool()
{
  is_terminated_ = true;
  DeleteThread(GetThreadNum());

  std::lock_guard<std::mutex> lock(map_lock_);
  for (auto &[id, thread] : thread_map_)
  {
	if (thread.joinable())
	{
	  thread.join();
	}
  }
}

void ThreadPool::ThreadLoop()
{
  std::function < void() > task;
  while (!is_terminated_)
  {
	{
	  std::lock_guard<std::mutex> lock(map_lock_);
	  if (waiting_delete_thread_num_ > 0)
	  {
		thread_map_.erase(std::this_thread::get_id());
		waiting_delete_thread_num_--;
		free_thread_num_--;
		return;
	  }
	}

	task_num_semaphore_.acquire();
	if (is_terminated_ || !task_queue_.Pop(task))
	  continue;

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
	std::jthread tmp(&ThreadPool::ThreadLoop, this);

	auto tmp_id = tmp.get_id();
	{
	  std::lock_guard<std::mutex> lock(map_lock_);
	  thread_map_[tmp_id] = std::move(tmp);
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
	task_num_semaphore_.release();
  }
}

template<typename TF>
auto ThreadPool::WaitTasksDone(TF &futures)
{
  return std::apply([&](auto &... futures)
					{
					  return std::make_tuple(futures.get()...);
					}, futures);
}

template<typename ...Futures>
auto ThreadPool::WaitTasksDone(Futures &... futures)
{
  return std::make_tuple(std::invoke([&](auto &future)
									 {
									   if constexpr (!std::is_void_v<decltype(future.get())>)
									   {
										 return future.get();
									   }
									   else
									   {
										 return nullptr;
									   }
									 }, futures)...);
}

template<typename T, typename F, typename R>
requires std::is_same_v<T, Normal>
auto ThreadPool::Submit(F &&task) -> std::future<R>
{
  std::function < R() > task_func(std::forward<F>(task));
  std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
  task_queue_.PushBack([task_func, task_promise]()
					   {
						 if constexpr (!std::is_void_v<R>)
						 {
						   task_promise->set_value(task_func());
						 }
						 else
						 {
						   task_func();
						   task_promise->set_value();
						 }
					   });
  task_num_semaphore_.release();
  return task_promise->get_future();
}

template<typename T, typename F, typename R>
requires std::is_same_v<T, Urgent>
auto ThreadPool::Submit(F &&task) -> std::future<R>
{
  std::function < R() > task_func(std::forward<F>(task));
  std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
  task_queue_.PushFront([task_func, task_promise]()
						{
						  if constexpr (!std::is_void_v<R>)
						  {
							task_promise->set_value(task_func());
						  }
						  else
						  {
							task_func();
							task_promise->set_value();
						  }
						});
  task_num_semaphore_.release();
  return task_promise->get_future();
}

template<typename T, typename... Fs>
requires std::is_same_v<T, Sequence>
auto ThreadPool::Submit(Fs &&... tasks) -> std::tuple<std::future<std::invoke_result_t<Fs>>...>
{
  auto tasks_func = std::make_tuple(std::function < std::invoke_result_t<Fs>() > (std::forward<Fs>(tasks))...);
  auto tasks_promises = std::make_tuple((std::make_shared<std::promise<std::invoke_result_t<Fs>>>)()...);
  auto tasks_futures =
	  std::apply([](auto &... promises)
				 { return std::make_tuple(promises->get_future()...); }, tasks_promises);

  task_queue_.PushBack([tasks_func, tasks_promises]()
					   {
						 std::apply([&](auto &... promises)
									{
									  std::apply([&](auto &... tasks)
												 {
												   // 使用折叠表达式来对每个 promise 应用 set_value
												   (std::invoke([&](auto &&task, auto &promise)
																{
																  using ReturnT = std::invoke_result_t<decltype(task)>;
																  if constexpr (!std::is_void_v<ReturnT>)
																  {
																	promise->set_value(task());
																  }
																  else
																  {
																	task();
																	promise->set_value();
																  }
																}, tasks, promises), ...);
												 }, tasks_func);
									}, tasks_promises);
					   });
  task_num_semaphore_.release();

  return tasks_futures;
}

} // KTP
#endif //THREADPOOL__THREADPOOL_H