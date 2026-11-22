/* Tiny cross-platform mutex helper for the plugin's bounded queues. */
#ifndef WORMHOLE_QUEUE_H
#define WORMHOLE_QUEUE_H

#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION wh_mutex_t;
#define wh_mutex_init(m) InitializeCriticalSection(m)
#define wh_mutex_lock(m) EnterCriticalSection(m)
#define wh_mutex_unlock(m) LeaveCriticalSection(m)
#define wh_mutex_free(m) DeleteCriticalSection(m)
typedef HANDLE wh_thread_t;
#define WH_THREAD_API DWORD WINAPI
#define WH_THREAD_ARG LPVOID
#define WH_THREAD_RET 0
#define wh_thread_create(t, fn, arg) (*(t) = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(fn), (arg), 0, NULL), *(t) != NULL)
#define wh_thread_join(t) do { WaitForSingleObject((t), INFINITE); CloseHandle((t)); } while (0)
#define wh_sleep_ms(ms) Sleep(ms)
#else
#include <pthread.h>
typedef pthread_mutex_t wh_mutex_t;
typedef pthread_t wh_thread_t;
#define WH_THREAD_API void *
#define WH_THREAD_ARG void *
#define WH_THREAD_RET NULL
#define wh_thread_create(t, fn, arg) (pthread_create((t), NULL, (fn), (arg)) == 0)
#define wh_thread_join(t) pthread_join((t), NULL)
#include <time.h>
#define wh_sleep_ms(ms) do { struct timespec ts; ts.tv_sec = (ms) / 1000; ts.tv_nsec = (long)((ms) % 1000) * 1000000L; nanosleep(&ts, NULL); } while (0)
#define wh_mutex_init(m) pthread_mutex_init(m, NULL)
#define wh_mutex_lock(m) pthread_mutex_lock(m)
#define wh_mutex_unlock(m) pthread_mutex_unlock(m)
#define wh_mutex_free(m) pthread_mutex_destroy(m)
#endif

#endif
