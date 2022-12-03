#include "config.h"

#if HAVE_NANOSLEEP
#ifdef __MINGW32__
#include <unistd.h>
#define timegm _mkgmtime
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


#define NUM_BLANKS 3
static SDL_atomic_t vblCounter[NUM_BLANKS];
static const char* BLANKS[] = {
  "main","nd_main","nd_video"  
};

static int64_t      cycleCounterStart;
static int64_t      cycleDivisor;
static uint64_t     perfCounterStart;
static uint64_t     perfFrequency;
static bool         perfCounterFreqInt;
static uint64_t     perfDivisor;
static double       perfMultiplicator;
static uint64_t     pauseTimeStamp;
static bool         enableRealtime;
static bool         osDarkmatter;
static bool         currentIsRealtime;
static uint64_t     hardClockExpected;
static uint64_t     hardClockActual;
static time_t       unixTimeStart;
static lock_t       timeLock;
static uint64_t     saveTime;

// external
extern int64_t      nCyclesMainCounter;
extern struct regstruct regs;

static inline uint64_t real_time(void) {
    uint64_t rt = (SDL_GetPerformanceCounter() - perfCounterStart);
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
    uint64_t cycleCounterLimit, perfCounterLimit, perfCounter;
    
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

// Check NeXT specific UNIX time limits and adjust time if needed
#define TIME_LIMIT_SECONDS 0

void host_check_unix_time(void) {
    struct tm* t = gmtime(&unixTimeStart);
    char* s = asctime(t);
    bool b = false;

    s[strlen(s)-1] = 0;
    Log_Printf(LOG_WARN, "[Hosttime] Unix time start: %s GMT", s);
    Log_Printf(LOG_WARN, "[Hosttime] Unix time will overflow in %f days", difftime(NEXT_MAX_SEC, unixTimeStart)/(24*60*60));
#if TIME_LIMIT_SECONDS
    if (unixTimeStart < NEXT_MIN_SEC || unixTimeStart >= NEXT_LIMIT_SEC) {
        unixTimeStart = NEXT_START_SEC;
        t = gmtime(&unixTimeStart);
        b = true;
    }
#else
    if (t->tm_year < NEXT_MIN_YEAR || t->tm_year >= NEXT_LIMIT_YEAR) {
        t->tm_year = NEXT_START_YEAR;
        unixTimeStart = timegm(t);
        b = true;
    }
#endif
    if (b) {
        s = asctime(t);
        s[strlen(s)-1] = 0;
        Log_Printf(LOG_WARN, "[Hosttime] Unix time is beyond valid range!");
        Log_Printf(LOG_WARN, "[Hosttime] Unix time is valid from Thu Jan 1 00:00:00 1970 through Thu Dec 31 23:59:59 2037 GMT");
        Log_Printf(LOG_WARN, "[Hosttime] Setting time to %s GMT", s);
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
        host_reset_blank_counter(i);
    }
    
    cycleDivisor = ConfigureParams.System.nCpuFreq;
    
    perfCounterFreqInt = (perfFrequency % 1000000ULL) == 0;
    perfDivisor        = perfFrequency / 1000000ULL;
    perfMultiplicator  = 1000000.0 / perfFrequency;
    
    host_report_limits();
    host_check_unix_time();
}

const char DARKMATTER[] = "darkmatter";

void host_blank_count(int src, bool state) {
    if (state) {
        SDL_AtomicAdd(&vblCounter[src], 1);
    }
    
    // check first 4 bytes of version string in darkmatter/daydream kernel
    osDarkmatter = get_long(0x04000246) == do_get_mem_long((uint8_t*)DARKMATTER);
}

int host_reset_blank_counter(int src) {
    return SDL_AtomicSet(&vblCounter[src], 0);
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
uint64_t host_get_save_time() {
    uint64_t hostTime;
    host_lock(&timeLock);
    hostTime = saveTime;
    host_unlock(&timeLock);
    return hostTime / 1000000ULL;
}

// Return current time as microseconds
uint64_t host_time_us() {
    uint64_t hostTime;
    
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
        uint64_t realTime  = real_time();
        
        if(currentIsRealtime) {
            // switching from real-time to cycle-time
            cycleCounterStart = nCyclesMainCounter - realTime * cycleDivisor;
        } else {
            // switching from cycle-time to real-time
            int64_t realTimeOffset = (int64_t)hostTime - realTime;
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

void host_time(uint64_t* realTime, uint64_t* hostTime) {
    *hostTime = host_time_us();
    *realTime = real_time();
}

// Return current time as seconds
uint64_t host_time_sec() {
    return host_time_us() / 1000000ULL;
}

// Return current time as milliseconds
uint64_t host_time_ms() {
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

int64_t host_real_time_offset() {
    uint64_t rt, vt;
    host_time(&rt, &vt);
    return (int64_t)vt-rt;
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
void host_sleep_us(uint64_t us) {
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
    uint64_t timeout = us;
    timeout += real_time();
    host_sleep_ms( (uint32_t)(us / 1000ULL) );
    while(real_time() < timeout) {}
#endif
}

void host_sleep_ms(uint32_t ms) {
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

static uint64_t lastVT;
static char   report[512];

const char* host_report(uint64_t realTime, uint64_t hostTime) {
    double dVT = hostTime - lastVT;
    dVT       /= 1000000.0;

    double hardClock = hardClockExpected;
    hardClock /= hardClockActual == 0 ? 1 : hardClockActual;
    
    char* r = report;
    r += sprintf(r, "[%s] hostTime:%llu hardClock:%.3fMHz", enableRealtime ? "Variable" : "CycleTime", hostTime, hardClock);

    for(int i = NUM_BLANKS; --i >= 0;) {
        r += sprintf(r, " %s:%.1fHz", BLANKS[i], (double)host_reset_blank_counter(i)/dVT);
    }
    
    lastVT = hostTime;

    return report;
}
