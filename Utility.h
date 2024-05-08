//
// Created by ZhouK on 2024/5/8.
//

#ifndef THREADPOOL__UTILITY_H
#define THREADPOOL__UTILITY_H

namespace KTP
{

// type trait
struct Normal   {};  // normal task (for type inference)
struct Urgent   {};  // urgent task (for type inference)
struct Sequence {};  // sequence tasks (for type inference)

} // KTP

#endif //THREADPOOL__UTILITY_H
