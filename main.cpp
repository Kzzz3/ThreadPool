#include <chrono>
#include <iostream>
#include "TPScheduler.h"
int main()
{
  {
	KTP::ThreadPool pool1(1);

	auto t1 = pool1.Submit<KTP::Normal>([]()
										{ return; });
	auto t2 = pool1.Submit<KTP::Normal>([]()
										{ return 2; });

	auto res1 = pool1.WaitTasksDone(t1, t2);

	auto t3 = pool1.Submit<KTP::Sequence>([]()
										  { return 3; },
										  []()
										  { return 4; });
	auto res2 = pool1.WaitTasksDone(t3);

	int a = 0;
  }
  return 0;
}
