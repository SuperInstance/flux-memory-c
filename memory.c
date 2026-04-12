#include "memory.h"
#include <stdio.h>
#include <string.h>

static int entry_is_expired(const MemEntry *e, uint64_t now) {
    return e->ttl > 0 && now > e->created + e->ttl;
}

static int find_entry(const MemStore *store, const char *key) {
    for (int i = 0; i < (int)store->count; i++)
        if (strcmp(store->entries[i].key, key) == 0) return i;
    return -1;
}

void mem_init(MemStore *store) {
    memset(store, 0, sizeof(*store));
}

MemError mem_put(MemStore *store, const char *key, const char *value, uint64_t ttl, uint8_t flags) {
    if (strlen(key) >= MEM_KEY_MAX) return MEM_ERR_KEY_TOO_LONG;
    if (strlen(value) >= MEM_VAL_MAX) return MEM_ERR_VAL_TOO_LONG;

    int idx = find_entry(store, key);
    if (idx >= 0) {
        // overwrite
        store->entries[idx].version++;
        strcpy(store->entries[idx].value, value);
        store->entries[idx].ttl = ttl;
        store->entries[idx].flags = flags;
        return MEM_OK;
    }
    if (store->count >= MEM_ENTRIES_MAX) return MEM_ERR_FULL;

    MemEntry *e = &store->entries[store->count++];
    strcpy(e->key, key);
    strcpy(e->value, value);
    e->ttl = ttl;
    e->flags = flags;
    e->version = 1;
    // created/modified left as 0 — caller manages timestamps via 'now'
    return MEM_OK;
}

MemError mem_get(MemStore *store, const char *key, char *out, size_t outsz, uint64_t now) {
    int idx = find_entry(store, key);
    if (idx < 0) return MEM_ERR_NOT_FOUND;
    if (entry_is_expired(&store->entries[idx], now)) return MEM_ERR_EXPIRED;
    strncpy(out, store->entries[idx].value, outsz - 1);
    out[outsz - 1] = '\0';
    return MEM_OK;
}

MemError mem_delete(MemStore *store, const char *key) {
    int idx = find_entry(store, key);
    if (idx < 0) return MEM_ERR_NOT_FOUND;
    // swap with last
    if (idx < (int)store->count - 1)
        store->entries[idx] = store->entries[store->count - 1];
    memset(&store->entries[store->count - 1], 0, sizeof(MemEntry));
    store->count--;
    return MEM_OK;
}

int mem_exists(MemStore *store, const char *key, uint64_t now) {
    int idx = find_entry(store, key);
    if (idx < 0) return 0;
    if (entry_is_expired(&store->entries[idx], now)) return 0;
    return 1;
}

MemError mem_update(MemStore *store, const char *key, const char *value, uint64_t now) {
    int idx = find_entry(store, key);
    if (idx < 0) return MEM_ERR_NOT_FOUND;
    if (entry_is_expired(&store->entries[idx], now)) return MEM_ERR_EXPIRED;
    if (store->entries[idx].flags & 0x01) return MEM_ERR_EXPIRED; // readonly → treat as error
    if (strlen(value) >= MEM_VAL_MAX) return MEM_ERR_VAL_TOO_LONG;
    store->entries[idx].version++;
    strcpy(store->entries[idx].value, value);
    return MEM_OK;
}

int mem_search(MemStore *store, const char *prefix, MemEntry *results, int max) {
    size_t plen = strlen(prefix);
    int n = 0;
    for (int i = 0; i < (int)store->count && n < max; i++)
        if (strncmp(store->entries[i].key, prefix, plen) == 0)
            results[n++] = store->entries[i];
    return n;
}

int mem_expired(MemStore *store, uint64_t now, char *keys, int max_keys) {
    int n = 0;
    for (int i = 0; i < (int)store->count && n < max_keys; i++) {
        if (entry_is_expired(&store->entries[i], now)) {
            if (keys) strcpy(keys + n * MEM_KEY_MAX, store->entries[i].key);
            n++;
        }
    }
    return n;
}

int mem_gc(MemStore *store, uint64_t now) {
    int removed = 0;
    for (int i = (int)store->count - 1; i >= 0; i--) {
        if (entry_is_expired(&store->entries[i], now)) {
            mem_delete(store, store->entries[i].key);
            removed++;
        }
    }
    return removed;
}

uint16_t mem_count(const MemStore *store) {
    return store->count;
}

// Version history — simplified: we store history alongside entries via version field only.
// Full history requires a separate structure; here we reconstruct from what's available.
MemError mem_version_history(const MemStore *store, const char *key, MemVersionHistory *hist) {
    int idx = find_entry(store, key);
    if (idx < 0) return MEM_ERR_NOT_FOUND;
    memset(hist, 0, sizeof(*hist));
    // Only current version is available (no malloc to store history)
    strcpy(hist->versions[0].value, store->entries[idx].value);
    hist->versions[0].version = store->entries[idx].version;
    hist->versions[0].timestamp = store->entries[idx].modified;
    hist->count = 1;
    return MEM_OK;
}

void snap_init(MemSnapshotLog *log) {
    memset(log, 0, sizeof(*log));
}

MemError mem_snapshot(const MemStore *store, MemSnapshotLog *log, const char *label, uint64_t now) {
    if (log->count >= SNAPSHOTS_MAX) return MEM_ERR_SNAPSHOTS_FULL;
    MemSnapshot *s = &log->snapshots[log->count++];
    memcpy(s->entries, store->entries, sizeof(store->entries));
    s->count = store->count;
    s->taken_at = now;
    strncpy(s->label, label, sizeof(s->label) - 1);
    s->label[sizeof(s->label) - 1] = '\0';
    return MEM_OK;
}

MemError mem_restore(MemStore *store, const MemSnapshotLog *log, int index) {
    if (index < 0 || index >= (int)log->count) return MEM_ERR_NOT_FOUND;
    memcpy(store->entries, log->snapshots[index].entries, sizeof(log->snapshots[index].entries));
    store->count = log->snapshots[index].count;
    return MEM_OK;
}

int mem_diff(const MemStore *store, const MemSnapshotLog *log, int index, char *diff, size_t diffsz) {
    if (index < 0 || index >= (int)log->count) return -1;
    const MemSnapshot *snap = &log->snapshots[index];
    memset(diff, 0, diffsz);
    size_t pos = 0;

    // Keys in current but not in snapshot → added
    for (int i = 0; i < (int)store->count && pos < diffsz - MEM_KEY_MAX - MEM_VAL_MAX - 4; i++) {
        int found = 0;
        for (int j = 0; j < (int)snap->count; j++) {
            if (strcmp(store->entries[i].key, snap->entries[j].key) == 0) { found = 1; break; }
        }
        if (!found) {
            pos += (size_t)snprintf(diff + pos, diffsz - pos, "+%s=%s\n", store->entries[i].key, store->entries[i].value);
        }
    }
    // Keys in snapshot but not in current → removed
    for (int j = 0; j < (int)snap->count && pos < diffsz - MEM_KEY_MAX - 4; j++) {
        int found = 0;
        for (int i = 0; i < (int)store->count; i++) {
            if (strcmp(snap->entries[j].key, store->entries[i].key) == 0) { found = 1; break; }
        }
        if (!found) {
            pos += (size_t)snprintf(diff + pos, diffsz - pos, "-%s\n", snap->entries[j].key);
        }
    }
    return (int)pos;
}
