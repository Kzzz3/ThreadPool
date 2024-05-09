#include <iostream>
#include "ThreadPool.h"
int main()
{
  KTP::ThreadPool pool(4);
  auto t = pool.Submit<KTP::Sequence>([](){return 1;},
							 [](){return;});

  std::cout << std::get<0>(t).get() << std::endl;

  while (true){}
    return 0;
}
