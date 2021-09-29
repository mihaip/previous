#include "config.h"

#if HAVE_NANOSLEEP
#ifdef __MINGW32__
#include <unistd.h>
#else
#include <sys/time.h>
#endif
#endif
#include <errno.h>

#include "host.h"
#include "configuration.h"
#include "main.h"
#include "log.h"
#include "memory.h"
#include "newcpu.h"

/* NeXTdimension blank handling, see nd_sdl.c */
void nd_display_blank(int num);
void nd_video_blank(int num);

#define NUM_BLANKS 3
static const char* BLANKS[] = {
  "main","nd_main","nd_video"  
};

static volatile Uint32 blank[NUM_BLANKS];
static Uint32       vblCounter[NUM_BLANKS];
static Sint64       cycleCounterStart;
static Sint64       cycleDivisor;
static Uint64       perfCounterStart;
static Uint64       perfFrequency;
static bool         perfCounterFreqInt;
static Uint64       perfDivisor;
static double       perfMultiplicator;
static Uint64       pauseTimeStamp;
static bool         enableRealtime;
static bool         osDarkmatter;
static bool         currentIsRealtime;
static Uint64       hardClockExpected;
static Uint64       hardClockActual;
static time_t       unixTimeStart;
static lock_t       timeLock;
static Uint64       saveTime;

// external
extern Sint64       nCyclesMainCounter;
extern struct regstruct regs;

static inline Uint64 real_time(void) {
    Uint64 rt = (SDL_GetPerformanceCounter() - perfCounterStart);
    if (perfCounterFreqInt) {
        rt /= perfDivisor;
    } else {
        rt *= perfMultiplicator;
    }
    return rt;
}

#define DAY_TO_US (1000000ULL * 60 * 60 * 24)

// Report counter capacity
void host_report_limits(void) {
    Uint64 cycleCounterLimit, perfCounterLimit, perfCounter;
    
    Log_Printf(LOG_WARN, "[Hosttime] Timing system reset:");
    
    cycleCounterLimit  = INT64_MAX - nCyclesMainCounter;
    cycleCounterLimit /= cycleDivisor;
    cycleCounterLimit /= DAY_TO_US;
    
    Log_Printf(LOG_WARN, "[Hosttime] Cycle counter value: %lld", nCyclesMainCounter);
    Log_Printf(LOG_WARN, "[Hosttime] Cycle counter frequency: %lld MHz", cycleDivisor);
    Log_Printf(LOG_WARN, "[Hosttime] Cycle timer will overflow in %lld days", cycleCounterLimit);
    
    perfCounter        = SDL_GetPerformanceCounter();
    perfCounterLimit   = UINT64_MAX - perfCounter;
    Log_Printf(LOG_WARN, "[Hosttime] Realtime counter value: %lld", perfCounter);
    if (perfCounterFreqInt) {
        perfCounterLimit /= perfDivisor;
        if (perfCounterLimit > INT64_MAX)
            perfCounterLimit = INT64_MAX;
        perfCounterLimit /= DAY_TO_US;
        Log_Printf(LOG_WARN, "[Hosttime] Realtime counter frequency: %lld MHz", perfDivisor);
        Log_Printf(LOG_WARN, "[Hosttime] Realtime timer will overflow in %lld days", perfCounterLimit);
    } else {
        if (perfCounterLimit > (1ULL<<DBL_MANT_DIG)-1)
            perfCounterLimit = (1ULL<<DBL_MANT_DIG)-1;
        if (perfMultiplicator < 1.0)
            perfCounterLimit *= perfMultiplicator;
        else
            Log_Printf(LOG_WARN, "[Hosttime] Warning: Realtime counter cannot resolve microseconds.");
        perfCounterLimit /= DAY_TO_US;
        Log_Printf(LOG_WARN, "[Hosttime] Realtime counter frequency: %f MHz", 1.0/perfMultiplicator);
        Log_Printf(LOG_WARN, "[Hosttime] Realtime timer will start losing precision in %lld days", perfCounterLimit);
    }
}

void host_reset(void) {
    perfCounterStart  = SDL_GetPerformanceCounter();
    pauseTimeStamp    = perfCounterStart;
    perfFrequency     = SDL_GetPerformanceFrequency();
    unixTimeStart     = time(NULL);
    cycleCounterStart = 0;
    currentIsRealtime = false;
    hardClockExpected = 0;
    hardClockActual   = 0;
    enableRealtime    = ConfigureParams.System.bRealtime;
    osDarkmatter      = false;
    saveTime          = 0;
    
    for(int i = NUM_BLANKS; --i >= 0;) {
        vblCounter[i] = 0;
        blank[i]      = 0;
    }
    
    cycleDivisor = ConfigureParams.System.nCpuFreq;
    
    perfCounterFreqInt = (perfFrequency % 1000000ULL) == 0;
    perfDivisor        = perfFrequency / 1000000ULL;
    perfMultiplicator  = 1000000.0 / perfFrequency;
    
    host_report_limits();
    
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
}

const char DARKMATTER[] = "darkmatter";

void host_blank(int slot, int src, bool state) {
    int bit = 1 << slot;
    if(state) {
        blank[src] |=  bit;
        vblCounter[src]++;
    }
    else
        blank[src] &= ~bit;
    switch (src) {
        case ND_DISPLAY:   nd_display_blank(slot); break;
        case ND_VIDEO:     nd_video_blank(slot);   break;
    }
    
    // check first 4 bytes of version string in darkmatter/daydream kernel
    osDarkmatter = get_long(0x04000246) == do_get_mem_long((Uint8*)DARKMATTER);
}

bool host_blank_state(int slot, int src) {
    int bit = 1 << slot;
    return blank[src] & bit;
}

void host_hardclock(int expected, int actual) {
    if(abs(actual-expected) > 1000) {
        Log_Printf(LOG_WARN, "[Hardclock] expected:%dus actual:%dus\n", expected, actual);
    } else {
        hardClockExpected += expected;
        hardClockActual   += actual;
    }
}

// this can be used by other threads to read hostTime
Uint64 host_get_save_time() {
    Uint64 hostTime;
    host_lock(&timeLock);
    hostTime = saveTime;
    host_unlock(&timeLock);
    return hostTime / 1000000ULL;
}

// Return current time as microseconds
Uint64 host_time_us() {
    Uint64 hostTime;
    
    host_lock(&timeLock);
    
    if(currentIsRealtime) {
        hostTime = real_time();
    } else {
        hostTime  = nCyclesMainCounter - cycleCounterStart;
        hostTime /= cycleDivisor;
    }
    
    // save hostTime to be read by other threads
    saveTime = hostTime;
    
    // switch to realtime if...
    // 1) ...realtime mode is enabled and...
    // 2) ...either we are running darkmatter or the m68k CPU is in user mode
    bool state = (osDarkmatter || !(regs.s)) && enableRealtime;
    if(currentIsRealtime != state) {
        Uint64 realTime  = real_time();
        
        if(currentIsRealtime) {
            // switching from real-time to cycle-time
            cycleCounterStart = nCyclesMainCounter - realTime * cycleDivisor;
        } else {
            // switching from cycle-time to real-time
            Sint64 realTimeOffset = (Sint64)hostTime - realTime;
            if(realTimeOffset > 0) {
                // if hostTime is in the future, wait until realTime is there as well
                if(realTimeOffset > 10000LL)
                    host_sleep_us(realTimeOffset);
                else
                    while(real_time() < hostTime) {}
            }
        }
        currentIsRealtime = state;
    }
    
    host_unlock(&timeLock);
    
    return hostTime;
}

void host_time(Uint64* realTime, Uint64* hostTime) {
    *hostTime = host_time_us();
    *realTime = real_time();
}

// Return current time as seconds
Uint64 host_time_sec() {
    return host_time_us() / 1000000ULL;
}

// Return current time as milliseconds
Uint64 host_time_ms() {
    return host_time_us() / 1000ULL;
}

time_t host_unix_time() {
    return unixTimeStart + host_time_sec();
}

void host_set_unix_time(time_t now) {
    unixTimeStart = now - host_time_sec();
}

struct tm* host_unix_tm() {
    time_t tmp = host_unix_time();
    return gmtime(&tmp);
}

void host_set_unix_tm(struct tm* now) {
    time_t tmp = timegm(now);
    host_set_unix_time(tmp);
}

Sint64 host_real_time_offset() {
    Uint64 rt, vt;
    host_time(&rt, &vt);
    return (Sint64)vt-rt;
}

void host_pause_time(bool pausing) {
    if(pausing) {
        pauseTimeStamp = SDL_GetPerformanceCounter();
    } else {
        perfCounterStart += SDL_GetPerformanceCounter() - pauseTimeStamp;
    }
}

/*-----------------------------------------------------------------------*/
/**
 * Sleep for a given number of micro seconds.
 */
void host_sleep_us(Uint64 us) {
#if HAVE_NANOSLEEP
    struct timespec	ts;
    int		ret;
    ts.tv_sec = us / 1000000ULL;
    ts.tv_nsec = (us % 1000000ULL) * 1000;	/* micro sec -> nano sec */
    /* wait until all the delay is elapsed, including possible interruptions by signals */
    do {
        errno = 0;
        ret = nanosleep(&ts, &ts);
    } while ( ret && ( errno == EINTR ) );		/* keep on sleeping if we were interrupted */
#else
    Uint64 timeout = us;
    timeout += real_time();
    host_sleep_ms( (Uint32)(us / 1000ULL) );
    while(real_time() < timeout) {}
#endif
}

void host_sleep_ms(Uint32 ms) {
    SDL_Delay(ms);
}

void host_lock(lock_t* lock) {
  SDL_AtomicLock(lock);
}

int host_trylock(lock_t* lock) {
  return SDL_AtomicTryLock(lock);
}

void host_unlock(lock_t* lock) {
  SDL_AtomicUnlock(lock);
}

int host_atomic_set(atomic_int* a, int newValue) {
    return SDL_AtomicSet(a, newValue);
}

int host_atomic_get(atomic_int* a) {
    return SDL_AtomicGet(a);
}

bool host_atomic_cas(atomic_int* a, int oldValue, int newValue) {
    return SDL_AtomicCAS(a, oldValue, newValue);
}

thread_t* host_thread_create(thread_func_t func, const char* name, void* data) {
  return SDL_CreateThread(func, name, data);
}

int host_thread_wait(thread_t* thread) {
  int status;
  SDL_WaitThread(thread, &status);
  return status;
}

mutex_t* host_mutex_create(void) {
    return SDL_CreateMutex();
}

void host_mutex_lock(mutex_t* mutex) {
    SDL_LockMutex(mutex);
}

void host_mutex_unlock(mutex_t* mutex) {
    SDL_UnlockMutex(mutex);
}

void host_mutex_destroy(mutex_t* mutex) {
    SDL_DestroyMutex(mutex);
}

int host_num_cpus() {
  return  SDL_GetCPUCount();
}

static Uint64 lastVT;
static char   report[512];

const char* host_report(Uint64 realTime, Uint64 hostTime) {
    double dVT = hostTime - lastVT;
    dVT       /= 1000000.0;

    double hardClock = hardClockExpected;
    hardClock /= hardClockActual == 0 ? 1 : hardClockActual;
    
    char* r = report;
    r += sprintf(r, "[%s] hostTime:%llu hardClock:%.3fMHz", enableRealtime ? "Variable" : "CycleTime", hostTime, hardClock);

    for(int i = NUM_BLANKS; --i >= 0;) {
        r += sprintf(r, " %s:%.1fHz", BLANKS[i], (double)vblCounter[i]/dVT);
        vblCounter[i] = 0;
    }
    
    lastVT = hostTime;

    return report;
}

Uint8* host_malloc_aligned(size_t size) {
#if defined(HAVE_POSIX_MEMALIGN)
    void* result = NULL;
    posix_memalign(&result, 0x10000, size);
    return (Uint8*)result;
#elif defined(HAVE_ALIGNED_ALLOC)
    return (Uint8*)aligned_alloc(0x10000, size);
#elif defined(HAVE__ALIGNED_ALLOC)
    return (Uint8*)_aligned_alloc(0x10000, size);
#else
    return (Uint8*)malloc(size);
#endif
}
