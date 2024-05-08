#include <iostream>
#include "TaskQueue.h"
int main() {
    KTP::TaskQueue<int> tq;
	int a = 1;
  tq.PushBack(a);
    return 0;
}
