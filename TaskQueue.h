//
// Created by ZhouK on 2024/5/8.
//

#ifndef THREADPOOL__TASKQUEUE_H
#define THREADPOOL__TASKQUEUE_H

#include<mutex>
#include<deque>

namespace KTP
{

template<typename T>
class TaskQueue
{
  using size_type = typename std::deque<T>::size_type;

 public:
  TaskQueue() = default;
  TaskQueue(TaskQueue &&) = default;
  TaskQueue(const TaskQueue &) = delete;

  bool Pop(T &);
  size_type Size();
  void PushBack(T &);
  void PushBack(T &&);
  void PushFront(T &);
  void PushFront(T &&);

 private:
  std::mutex lock_;
  std::deque<T> queue_;
};

template<typename T>
bool TaskQueue<T>::Pop(T &tmp)
{
  std::lock_guard<std::mutex> lock(lock_);
  if (!queue_.empty())
  {
	tmp = std::move(queue_.front());
	queue_.pop_front();
	return true;
  }
  return false;
}

template<typename T>
typename TaskQueue<T>::size_type TaskQueue<T>::Size()
{
  std::lock_guard<std::mutex> lock(lock_);
  return queue_.size();
}

template<typename T>
void TaskQueue<T>::PushBack(T &value)
{
  std::lock_guard<std::mutex> lock(lock_);
  queue_.emplace_back(value);
}

template<typename T>
void TaskQueue<T>::PushBack(T &&value)
{
  std::lock_guard<std::mutex> lock(lock_);
  queue_.emplace_back(std::move(value));
}

template<typename T>
void TaskQueue<T>::PushFront(T &value)
{
  std::lock_guard<std::mutex> lock(lock_);
  queue_.emplace_front(value);
}

template<typename T>
void TaskQueue<T>::PushFront(T &&value)
{
  std::lock_guard<std::mutex> lock(lock_);
  queue_.emplace_front(std::move(value));
}

}

#endif //THREADPOOL__TASKQUEUE_H
