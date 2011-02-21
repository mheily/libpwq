#ifndef _PTWQ_WINDOWS_PLATFORM_H
#define _PTWQ_WINDOWS_PLATFORM_H 1

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#undef LIST_HEAD
#include "./queue.h"

typedef HANDLE pthread_t;
typedef CRITICAL_SECTION pthread_spinlock_t;
typedef CRITICAL_SECTION pthread_mutex_t;

#endif  /* _PTWQ_WINDOWS_PLATFORM_H */
