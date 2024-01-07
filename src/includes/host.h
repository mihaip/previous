/*
  Previous - host.h

  This file is distributed under the GNU General Public License, version 2
  or at your option any later version. Read the file gpl.txt for details.
*/

#pragma once

#ifndef PREV_HOST_H
#define PREV_HOST_H

#include <SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REALTIME_INT_LVL 1

#ifdef __MINGW32__
#define FMT_ll "I64"
#define FMT_zu "u"
#else
#define FMT_ll "ll"
#define FMT_zu "zu"
#endif

enum {
    MAIN_DISPLAY,
    ND_DISPLAY,
    ND_VIDEO,
};

typedef SDL_atomic_t       atomic_int;
typedef SDL_SpinLock       lock_t;
typedef SDL_Thread         thread_t;
typedef SDL_ThreadFunction thread_func_t;
typedef SDL_mutex          mutex_t;

extern void        host_reset(void);
extern void        host_blank_count(int src, bool state);
extern int         host_reset_blank_counter(int src);
extern uint64_t    host_time_us(void);
extern uint64_t    host_time_ms(void);
extern uint64_t    host_time_sec(void);
extern void        host_time(uint64_t* realTime, uint64_t* hostTime);
extern time_t      host_unix_time(void);
extern void        host_set_unix_time(time_t now);
extern struct tm*  host_unix_tm(void);
extern void        host_set_unix_tm(struct tm* now);
extern uint64_t    host_get_save_time(void);
extern void        host_sleep_ms(uint32_t ms);
extern void        host_sleep_us(uint64_t us);
extern int         host_num_cpus(void);
extern void        host_hardclock(int expected, int actual);
extern int64_t     host_real_time_offset(void);
extern void        host_pause_time(bool pausing);
extern const char* host_report(uint64_t realTime, uint64_t hostTime);

extern void        host_lock(lock_t* lock);
extern void        host_unlock(lock_t* lock);
extern int         host_trylock(lock_t* lock);
extern int         host_atomic_set(atomic_int* a, int newValue);
extern int         host_atomic_get(atomic_int* a);
extern int         host_atomic_add(atomic_int* a, int value);
extern bool        host_atomic_cas(atomic_int* a, int oldValue, int newValue);
extern mutex_t*    host_mutex_create(void);
extern void        host_mutex_lock(mutex_t* mutex);
extern void        host_mutex_unlock(mutex_t* mutex);
extern void        host_mutex_destroy(mutex_t* mutex);
extern thread_t*   host_thread_create(thread_func_t, const char* name, void* data);
extern int         host_thread_wait(thread_t* thread);

#ifdef __cplusplus
}
#endif

#endif /* PREV_HOST_H */
