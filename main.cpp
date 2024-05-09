#include <iostream>
#include "ThreadPool.h"
int main()
{
  KTP::ThreadPool pool(4);
  pool.Submit<KTP::Sequence>([](){return 1;},
							 [](){return;});
  while(true){}
}
