//
// Created by ZhouK on 2024/5/8.
//

#ifndef THREADPOOL__UTILITY_H
#define THREADPOOL__UTILITY_H

namespace KTP
{

// type trait
struct Normal   {};  // normal task
struct Urgent   {};  // urgent task
struct Sequence {};  // sequence tasks

} // KTP

#endif //THREADPOOL__UTILITY_H
