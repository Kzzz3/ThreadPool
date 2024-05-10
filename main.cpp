#include <iostream>
#include "TPScheduler.h"
int main()
{
  KTP::TPScheduler scheduler;
  KTP::ThreadPool pool1(1);
  KTP::ThreadPool pool2(2);
  KTP::ThreadPool pool3(3);
  scheduler.Hosting(&pool1);
  scheduler.Hosting(&pool2);
  scheduler.Hosting(&pool3);

  scheduler.Submit<KTP::Normal>([]
								{ std::cout << "Normal task 1\n"; });
  scheduler.Submit<KTP::Normal>([]
								{ std::cout << "Normal task 2\n"; });

  scheduler.Submit<KTP::Urgent>([]
								{ std::cout << "Urgent task 1\n"; });
  scheduler.Submit<KTP::Urgent>([]
								{ std::cout << "Urgent task 2\n"; });

  scheduler.Submit<KTP::Sequence>([]
								  { std::cout << "Sequence task 1\n"; },
								  []
								  { std::cout << "Sequence task 2\n"; },
								  []
								  { std::cout << "Sequence task 3\n"; });

  auto r1 = scheduler.Submit<KTP::Normal>([]
										  { return 1; });

  auto r2 = scheduler.Submit<KTP::Urgent>([]
										  { return 2; });

  auto r3 = scheduler.Submit<KTP::Sequence>([]
											{ return 3; },
											[]
											{ return 4; },
											[]
											{ return 5; });

  std::cout << r1.get() << std::endl;
  std::cout << r2.get() << std::endl;
  std::cout << std::get<0>(r3).get() << std::endl;
  std::cout << std::get<1>(r3).get() << std::endl;
  std::cout << std::get<2>(r3).get() << std::endl;

  while (true)
  {
	std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return 0;
}
