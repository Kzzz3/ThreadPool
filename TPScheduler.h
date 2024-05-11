//
// Created by kzzz3 on 2024/5/10.
//

#include <map>
#include <cassert>
#include "ThreadPool.h"

#ifndef THREADPOOL__TPSCHEDULER_H_
#define THREADPOOL__TPSCHEDULER_H_

namespace KTP
{

using TPID = int;

class TPScheduler
{
 public:
  TPScheduler();
  TPScheduler(int min_thread_num, int max_thread_num, int interval);

  TPScheduler(const TPScheduler &) = delete;
  TPScheduler(TPScheduler &&) = delete;
  ~TPScheduler();

  void ThreadLoop();

  TPID CalNextDeliveredTPID();

  TPID Hosting(ThreadPool *pool);
  ThreadPool *UnHosting(TPID id);

  template<typename TF>
  static auto WaitTasksDone(TF &futures);

  template<typename ...Futures>
  static auto WaitTasksDone(Futures &... futures);

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
  int min_thread_num_ = 1;
  int max_thread_num_ = 10;
  std::atomic<int> interval_ = 100;
  std::atomic<bool> is_terminated_ = false;
  std::atomic<TPID> next_delivered_tpid_ = 0;

  std::mutex map_lock_;
  std::map<TPID, ThreadPool *> pools_;

  std::jthread scheduler_thread_;
};

TPScheduler::TPScheduler()
	: scheduler_thread_(&TPScheduler::ThreadLoop, this)
{
}

TPScheduler::TPScheduler(int min_thread_num, int max_thread_num, int interval)
	: min_thread_num_(min_thread_num), max_thread_num_(max_thread_num), interval_(interval),
	  scheduler_thread_(&TPScheduler::ThreadLoop, this)
{
}

TPScheduler::~TPScheduler()
{
  is_terminated_ = true;

  if (scheduler_thread_.joinable())
  {
	scheduler_thread_.join();
  }
}

void TPScheduler::ThreadLoop()
{
  while (!is_terminated_)
  {
	std::this_thread::sleep_for(std::chrono::milliseconds(interval_));

	// thread pool management
	std::lock_guard<std::mutex> lock(map_lock_);

	for (auto &pool : pools_)
	{
	  auto task_num = pool.second->GetTaskNum();
	  auto thread_num = pool.second->GetThreadNum();

	  assert(thread_num >= min_thread_num_ && thread_num <= max_thread_num_);

	  if (task_num)
	  {
		int nums = std::min(max_thread_num_ - thread_num, task_num - thread_num);
		if (nums > 0)
		{
		  pool.second->AddThread(nums);
		}
	  }
	  else if (thread_num > min_thread_num_)
	  {
		pool.second->DeleteThread();
	  }

	  //Calculate next delivered TPID
	  next_delivered_tpid_ = CalNextDeliveredTPID();
	}
  }
}

TPID TPScheduler::CalNextDeliveredTPID()
{
  TPID least_task_tpid = 0;
  int min_task_num = INT_MAX;
  for (auto &pool : pools_)
  {
	auto task_num = pool.second->GetTaskNum();
	if (task_num < min_task_num)
	{
	  min_task_num = task_num;
	  least_task_tpid = pool.first;
	}
  }
  return least_task_tpid;
}

TPID TPScheduler::Hosting(ThreadPool *pool)
{
  static TPID next_tpid = 0;
  std::lock_guard<std::mutex> lock(map_lock_);
  pools_[++next_tpid] = pool;
  next_delivered_tpid_ = CalNextDeliveredTPID();
  return next_tpid;
}

ThreadPool *TPScheduler::UnHosting(TPID id)
{
  std::lock_guard<std::mutex> lock(map_lock_);
  if (pools_.contains(id))
  {
	auto pool = pools_[id];
	pools_.erase(id);
	next_delivered_tpid_ = CalNextDeliveredTPID();
	return pool;
  }
  return nullptr;
}

template<typename TF>
auto TPScheduler::WaitTasksDone(TF &futures)
{
  return std::apply([&](auto &... futures)
					{
					  return std::make_tuple(futures.get()...);
					}, futures);
}

template<typename ...Futures>
auto TPScheduler::WaitTasksDone(Futures &... futures)
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

// return non-void type
template<typename T, typename F, typename R>
requires std::is_same_v<T, Normal>
auto TPScheduler::Submit(F &&task) -> std::future<R>
{
  std::lock_guard<std::mutex> lock(map_lock_);

  assert(pools_.contains(next_delivered_tpid_));
  return pools_[next_delivered_tpid_]->Submit<T, F, R>(std::forward<F>(task));
}

template<typename T, typename F, typename R>
requires std::is_same_v<T, Urgent>
auto TPScheduler::Submit(F &&task) -> std::future<R>
{
  std::lock_guard<std::mutex> lock(map_lock_);

  assert(pools_.contains(next_delivered_tpid_));
  return pools_[next_delivered_tpid_]->Submit<T, F, R>(std::forward<F>(task));
}

template<typename T, typename... Fs>
requires std::is_same_v<T, Sequence>
auto TPScheduler::Submit(Fs &&... tasks) -> std::tuple<std::future<std::invoke_result_t<Fs>>...>
{
  std::lock_guard<std::mutex> lock(map_lock_);

  assert(pools_.contains(next_delivered_tpid_));
  return pools_[next_delivered_tpid_]->Submit<T, Fs...>(std::forward<Fs>(tasks)...);
}

}

#endif //THREADPOOL__TPSCHEDULER_H_
