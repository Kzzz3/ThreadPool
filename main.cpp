#include <chrono>
#include <iostream>
#include "TPScheduler.h"
int main()
{

  // must make sure the pool is destroyed after the scheduler
  KTP::ThreadPool pool1(5);
  KTP::TPScheduler scheduler(1, 10, 1000);

  auto tp1 = scheduler.Hosting(&pool1);

  for (int i = 0; i < 100000; i++)
  {
	auto t2 = scheduler.Submit<KTP::Normal>([]()
											{ return 2; });
  }

  scheduler.UnHosting(tp1);



//  KTP::ThreadPool pool1(10);
//
//  auto t1 = pool1.Submit<KTP::Normal>([]()
//									  { return; });
//  auto t2 = pool1.Submit<KTP::Normal>([]()
//									  { return 2; });
//
//  auto res1 = pool1.WaitTasksDone(t1, t2);
//
//  auto t3 = pool1.Submit<KTP::Sequence>([]()
//										{ return 3; },
//										[]()
//										{ return 4; });
//  auto res2 = pool1.WaitTasksDone(t3);

  return 0;
}
