#ifndef FLUX_MEMORY_H
#define FLUX_MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define MEM_ENTRIES_MAX 256
#define MEM_KEY_MAX 48
#define MEM_VAL_MAX 512
#define SNAPSHOTS_MAX 8
#define VERSIONS_MAX 16

typedef enum {
    MEM_OK = 0,
    MEM_ERR_FULL,
    MEM_ERR_NOT_FOUND,
    MEM_ERR_EXPIRED,
    MEM_ERR_KEY_TOO_LONG,
    MEM_ERR_VAL_TOO_LONG,
    MEM_ERR_SNAPSHOTS_FULL,
    MEM_ERR_VERSIONS_FULL
} MemError;

typedef struct {
    char key[MEM_KEY_MAX];
    char value[MEM_VAL_MAX];
    uint64_t created;
    uint64_t modified;
    uint64_t ttl;
    uint32_t version;
    uint8_t flags;
} MemEntry;

typedef struct {
    MemEntry entries[MEM_ENTRIES_MAX];
    uint16_t count;
} MemStore;

typedef struct {
    MemEntry entries[MEM_ENTRIES_MAX];
    uint16_t count;
    uint64_t taken_at;
    char label[32];
} MemSnapshot;

typedef struct {
    MemSnapshot snapshots[SNAPSHOTS_MAX];
    uint8_t count;
} MemSnapshotLog;

typedef struct {
    char value[MEM_VAL_MAX];
    uint32_t version;
    uint64_t timestamp;
} MemVersion;

typedef struct {
    MemVersion versions[VERSIONS_MAX];
    uint8_t count;
} MemVersionHistory;

void mem_init(MemStore *store);
MemError mem_put(MemStore *store, const char *key, const char *value, uint64_t ttl, uint8_t flags);
MemError mem_get(MemStore *store, const char *key, char *out, size_t outsz, uint64_t now);
MemError mem_delete(MemStore *store, const char *key);
int mem_exists(MemStore *store, const char *key, uint64_t now);
MemError mem_update(MemStore *store, const char *key, const char *value, uint64_t now);
int mem_search(MemStore *store, const char *prefix, MemEntry *results, int max);
int mem_expired(MemStore *store, uint64_t now, char *keys, int max_keys);
int mem_gc(MemStore *store, uint64_t now);
uint16_t mem_count(const MemStore *store);
MemError mem_version_history(const MemStore *store, const char *key, MemVersionHistory *hist);
void snap_init(MemSnapshotLog *log);
MemError mem_snapshot(const MemStore *store, MemSnapshotLog *log, const char *label, uint64_t now);
MemError mem_restore(MemStore *store, const MemSnapshotLog *log, int index);
int mem_diff(const MemStore *store, const MemSnapshotLog *log, int index, char *diff, size_t diffsz);

#endif
