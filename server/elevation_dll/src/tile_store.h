#ifndef TILE_STORE_H
#define TILE_STORE_H

#include "mapped_file.h"
#include "dt2_read.h"
#include <mutex>

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
    std::mutex mutex_;
};

#endif /* TILE_STORE_H */
