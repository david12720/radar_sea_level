#ifndef TILE_STORE_H
#define TILE_STORE_H

#include "mapped_file.h"
#include "dt2_read.h"

/* ---------- portable mutex (C++11 std::mutex or platform fallback) ---------- */
#if __cplusplus >= 201103L && !defined(ELEV_NO_STDMUTEX)
#  include <mutex>
   struct TsMutex {
       std::mutex m;
       void lock()   { m.lock(); }
       void unlock() { m.unlock(); }
   };
#elif defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
   struct TsMutex {
       CRITICAL_SECTION cs;
       TsMutex()  { InitializeCriticalSection(&cs); }
       ~TsMutex() { DeleteCriticalSection(&cs); }
       void lock()   { EnterCriticalSection(&cs); }
       void unlock() { LeaveCriticalSection(&cs); }
   private:
       TsMutex(const TsMutex&);
       TsMutex& operator=(const TsMutex&);
   };
#else
#  include <pthread.h>
   struct TsMutex {
       pthread_mutex_t m;
       TsMutex()  { pthread_mutex_init(&m, 0); }
       ~TsMutex() { pthread_mutex_destroy(&m); }
       void lock()   { pthread_mutex_lock(&m); }
       void unlock() { pthread_mutex_unlock(&m); }
   private:
       TsMutex(const TsMutex&);
       TsMutex& operator=(const TsMutex&);
   };
#endif

struct TsLock {
    TsMutex& m_;
    explicit TsLock(TsMutex& m) : m_(m) { m_.lock(); }
    ~TsLock() { m_.unlock(); }
private:
    TsLock(const TsLock&);
    TsLock& operator=(const TsLock&);
};
/* --------------------------------------------------------------------------- */

class TileStore {
public:
    explicit TileStore(const char* tiles_dir);

    const unsigned char* get(int lat_floor, int lon_floor,
                             bool* out_missing, bool* out_io_err);

private:
    static const int CAPACITY = 256;

    struct Slot {
        unsigned int key;
        MappedFile   mapped;
        int          prev;
        int          next;
    };

    void move_to_head(int idx);
    int  evict_tail();

    Slot slots_[CAPACITY];
    int  lru_head_;
    int  lru_tail_;
    char tiles_dir_[TILES_DIR_MAX];
    TsMutex mutex_;
};

#endif /* TILE_STORE_H */
