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

  // return void type
  template<typename T, typename F>
  requires std::is_same_v<T, Normal>
	  && std::is_void<std::invoke_result_t<F>>::value
  auto Submit(F &&task);

  template<typename T, typename F>
  requires std::is_same_v<T, Urgent>
	  && std::is_void<std::invoke_result_t<F>>::value
  auto Submit(F &&task);

  template<typename T, typename... Fs>
  requires std::is_same_v<T, Sequence>
	  && std::conjunction_v<std::is_void<std::invoke_result_t<Fs>>...>
  auto Submit(Fs &&... tasks);

  // return non-void type
  template<typename T, typename F, typename R = std::invoke_result_t<F>>
  requires std::is_same_v<T, Normal>
	  && (!std::is_void<std::invoke_result_t<F>>::value)
  auto Submit(F &&task) -> std::future<R>;

  template<typename T, typename F, typename R = std::invoke_result_t<F>>
  requires std::is_same_v<T, Urgent>
	  && (!std::is_void<std::invoke_result_t<F>>::value)
  auto Submit(F &&task) -> std::future<R>;

  template<typename T, typename... Fs>
  requires std::is_same_v<T, Sequence>
	  && (!std::conjunction_v<std::is_void<std::invoke_result_t<Fs>>...>)
  auto Submit(Fs &&... tasks) -> std::tuple<std::future<std::invoke_result_t<Fs>>...>;

 private:
  int min_thread_num_ = 1;
  int max_thread_num_ = 10;
  std::atomic<int> interval_ = 100;
  std::atomic<TPID> next_tpid_ = 0;
  std::atomic<bool> is_terminated_ = false;

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
	  next_tpid_ = CalNextDeliveredTPID();
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
  return next_tpid;
}

ThreadPool *TPScheduler::UnHosting(TPID id)
{
  std::lock_guard<std::mutex> lock(map_lock_);
  if (pools_.contains(id))
  {
	auto pool = pools_[id];
	pools_.erase(id);
	return pool;
  }
  return nullptr;
}

template<typename T, typename F>
requires std::is_same_v<T, Normal>
	&& std::is_void<std::invoke_result_t<F>>::value
auto TPScheduler::Submit(F &&task)
{
  std::lock_guard<std::mutex> lock(map_lock_);
  if (!pools_.contains(next_tpid_))
  {
	next_tpid_ = CalNextDeliveredTPID();
  }

  assert(pools_.contains(next_tpid_));
  pools_[next_tpid_]->Submit<T>(std::forward<F>(task));
}

template<typename T, typename F>
requires std::is_same_v<T, Urgent>
	&& std::is_void<std::invoke_result_t<F>>::value
auto TPScheduler::Submit(F &&task)
{
  std::lock_guard<std::mutex> lock(map_lock_);
  if (!pools_.contains(next_tpid_))
  {
	next_tpid_ = CalNextDeliveredTPID();
  }

  assert(pools_.contains(next_tpid_));
  pools_[next_tpid_]->Submit<T>(std::forward<F>(task));

}

template<typename T, typename... Fs>
requires std::is_same_v<T, Sequence>
	&& std::conjunction_v<std::is_void<std::invoke_result_t<Fs>>...>
auto TPScheduler::Submit(Fs &&... tasks)
{
  std::lock_guard<std::mutex> lock(map_lock_);
  if (!pools_.contains(next_tpid_))
  {
	next_tpid_ = CalNextDeliveredTPID();
  }

  assert(pools_.contains(next_tpid_));
  pools_[next_tpid_]->Submit<T>(std::forward<Fs>(tasks)...);
}

// return non-void type
template<typename T, typename F, typename R>
requires std::is_same_v<T, Normal>
	&& (!std::is_void<std::invoke_result_t<F>>::value)
auto TPScheduler::Submit(F &&task) -> std::future<R>
{
  std::lock_guard<std::mutex> lock(map_lock_);
  if (!pools_.contains(next_tpid_))
  {
	next_tpid_ = CalNextDeliveredTPID();
  }

  assert(pools_.contains(next_tpid_));
  return pools_[next_tpid_]->Submit<T, F, R>(std::forward<F>(task));
}

template<typename T, typename F, typename R>
requires std::is_same_v<T, Urgent>
	&& (!std::is_void<std::invoke_result_t<F>>::value)
auto TPScheduler::Submit(F &&task) -> std::future<R>
{
  std::lock_guard<std::mutex> lock(map_lock_);
  if (!pools_.contains(next_tpid_))
  {
	next_tpid_ = CalNextDeliveredTPID();
  }

  assert(pools_.contains(next_tpid_));
  return pools_[next_tpid_]->Submit<T, F, R>(std::forward<F>(task));
}

template<typename T, typename... Fs>
requires std::is_same_v<T, Sequence>
	&& (!std::conjunction_v<std::is_void<std::invoke_result_t<Fs>>...>)
auto TPScheduler::Submit(Fs &&... tasks) -> std::tuple<std::future<std::invoke_result_t<Fs>>...>
{
  std::lock_guard<std::mutex> lock(map_lock_);
  if (!pools_.contains(next_tpid_))
  {
	next_tpid_ = CalNextDeliveredTPID();
  }

  assert(pools_.contains(next_tpid_));
  return pools_[next_tpid_]->Submit<T, Fs...>(std::forward<Fs>(tasks)...);
}

}

#endif //THREADPOOL__TPSCHEDULER_H_
