#include "tile_store.h"
#include <string.h>

static const unsigned int EMPTY_KEY = 0xFFFFFFFFu;

static unsigned int pack_key(int lat_floor, int lon_floor)
{
    return (unsigned int)(lat_floor + 90) << 16 | (unsigned int)(lon_floor + 180);
}

TileStore::TileStore(const char* tiles_dir)
    : lru_head_(0)
    , lru_tail_(CAPACITY - 1)
{
    snprintf(tiles_dir_, TILES_DIR_MAX, "%s", tiles_dir);

    for (int i = 0; i < CAPACITY; ++i) {
        slots_[i].key  = EMPTY_KEY;
        slots_[i].prev = i - 1;
        slots_[i].next = i + 1;
    }
    slots_[0].prev          = -1;
    slots_[CAPACITY-1].next = -1;
}

void TileStore::move_to_head(int idx)
{
    if (idx == lru_head_) return;

    int p = slots_[idx].prev;
    int n = slots_[idx].next;

    if (p >= 0) slots_[p].next = n;
    if (n >= 0) slots_[n].prev = p;
    if (idx == lru_tail_) lru_tail_ = p;

    slots_[idx].prev = -1;
    slots_[idx].next = lru_head_;
    if (lru_head_ >= 0) slots_[lru_head_].prev = idx;
    lru_head_ = idx;
}

const unsigned char* TileStore::get(int lat_floor, int lon_floor,
                                    bool* out_missing, bool* out_io_err)
{
    *out_missing = false;
    *out_io_err  = false;

    unsigned int key = pack_key(lat_floor, lon_floor);

    std::lock_guard<std::mutex> lock(mutex_);

    for (int i = 0; i < CAPACITY; ++i) {
        if (slots_[i].key == key) {
            if (slots_[i].mapped.data()) {
                move_to_head(i);
                return slots_[i].mapped.data();
            }
            if (slots_[i].mapped.is_missing()) {
                *out_missing = true;
                return NULL;
            }
            // io error slot — retry load
            break;
        }
    }

    // evict tail slot
    int idx = lru_tail_;
    slots_[idx].mapped.close();
    slots_[idx].key = EMPTY_KEY;

    char path[TILES_DIR_MAX + 32];
    if (!build_tile_path(tiles_dir_, lat_floor, lon_floor, path, (int)sizeof(path))) {
        *out_io_err = true;
        return NULL;
    }

    if (slots_[idx].mapped.open(path, DTED_FILE_SIZE)) {
        slots_[idx].key = key;
        move_to_head(idx);
        return slots_[idx].mapped.data();
    }

    if (slots_[idx].mapped.is_missing()) {
        slots_[idx].key = key;
        move_to_head(idx);
        *out_missing = true;
        return NULL;
    }

    *out_io_err = true;
    return NULL;
}
