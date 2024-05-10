#include <chrono>
#include <iostream>
#include "TPScheduler.h"
int main()
{
  KTP::TPScheduler scheduler(1, 100, 100);
  KTP::ThreadPool pool1(10);
  scheduler.Hosting(&pool1);
//for(int i=0;i<10;i++)
//{
//  KTP::ThreadPool *pool = new KTP::ThreadPool(10);
//  scheduler.Hosting(pool);
//}


  auto start = std::chrono::high_resolution_clock::now();

  std::vector<std::future<int>> futures;
  for(int i=0;i<10000;i++)
  {
	futures.push_back(scheduler.Submit<KTP::Normal>([](){std::this_thread::sleep_for(std::chrono::microseconds (1));return 1;}));
  }

  for(auto &f : futures)
  {
	f.get();
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  std::cout << "Function execution time: " << duration.count() << " microseconds" << std::endl;

  while (true)
  {
	std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return 0;
}
