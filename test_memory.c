#include "memory.h"
#include <stdio.h>
#include <string.h>

static int tests_run = 0, tests_passed = 0;

#define TEST(name) void name(void)
#define RUN(t) do { printf("  %-40s", #t); tests_run++; t(); tests_passed++; printf("PASS\n"); } while(0)
#define ASSERT(cond) do { if (!(cond)) { printf("FAIL (line %d)\n", __LINE__); return; } } while(0)

TEST(test_init_empty) {
    MemStore s; mem_init(&s);
    ASSERT(mem_count(&s) == 0);
}

TEST(test_put_and_get) {
    MemStore s; mem_init(&s);
    ASSERT(mem_put(&s, "name", "alice", 0, 0) == MEM_OK);
    char buf[64];
    ASSERT(mem_get(&s, "name", buf, sizeof(buf), 0) == MEM_OK);
    ASSERT(strcmp(buf, "alice") == 0);
}

TEST(test_get_nonexistent) {
    MemStore s; mem_init(&s);
    char buf[64];
    ASSERT(mem_get(&s, "nope", buf, sizeof(buf), 0) == MEM_ERR_NOT_FOUND);
}

TEST(test_put_overwrites) {
    MemStore s; mem_init(&s);
    mem_put(&s, "k", "v1", 0, 0);
    mem_put(&s, "k", "v2", 0, 0);
    char buf[64];
    mem_get(&s, "k", buf, sizeof(buf), 0);
    ASSERT(strcmp(buf, "v2") == 0);
}

TEST(test_delete) {
    MemStore s; mem_init(&s);
    mem_put(&s, "k", "v", 0, 0);
    ASSERT(mem_delete(&s, "k") == MEM_OK);
    ASSERT(mem_count(&s) == 0);
}

TEST(test_exists) {
    MemStore s; mem_init(&s);
    mem_put(&s, "k", "v", 0, 0);
    ASSERT(mem_exists(&s, "k", 0) == 1);
    ASSERT(mem_exists(&s, "nope", 0) == 0);
}

TEST(test_expired_get) {
    MemStore s; mem_init(&s);
    mem_put(&s, "k", "v", 10, 0);
    char buf[64];
    ASSERT(mem_get(&s, "k", buf, sizeof(buf), 20) == MEM_ERR_EXPIRED);
}

TEST(test_gc) {
    MemStore s; mem_init(&s);
    mem_put(&s, "k", "v", 10, 0);
    mem_put(&s, "k2", "v2", 100, 0);
    ASSERT(mem_gc(&s, 20) == 1);
    ASSERT(mem_count(&s) == 1);
}

TEST(test_ttl_zero) {
    MemStore s; mem_init(&s);
    mem_put(&s, "k", "v", 0, 0);
    char buf[64];
    ASSERT(mem_get(&s, "k", buf, sizeof(buf), 999999) == MEM_OK);
}

TEST(test_search_prefix) {
    MemStore s; mem_init(&s);
    mem_put(&s, "user:1", "a", 0, 0);
    mem_put(&s, "user:2", "b", 0, 0);
    mem_put(&s, "item:1", "c", 0, 0);
    MemEntry res[8];
    ASSERT(mem_search(&s, "user:", res, 8) == 2);
}

TEST(test_readonly) {
    MemStore s; mem_init(&s);
    mem_put(&s, "k", "v", 0, 0x01);
    ASSERT(mem_update(&s, "k", "new", 0) != MEM_OK);
}

TEST(test_update_version) {
    MemStore s; mem_init(&s);
    mem_put(&s, "k", "v1", 0, 0);
    mem_update(&s, "k", "v2", 0);
    int idx = 0;
    for (int i = 0; i < (int)s.count; i++)
        if (strcmp(s.entries[i].key, "k") == 0) idx = i;
    ASSERT(s.entries[idx].version == 2);
}

TEST(test_snapshot_restore) {
    MemStore s; mem_init(&s);
    mem_put(&s, "k", "v1", 0, 0);
    MemSnapshotLog log; snap_init(&log);
    mem_snapshot(&s, &log, "before", 0);
    mem_update(&s, "k", "v2", 0);
    mem_restore(&s, &log, 0);
    char buf[64];
    mem_get(&s, "k", buf, sizeof(buf), 0);
    ASSERT(strcmp(buf, "v1") == 0);
}

TEST(test_diff_added) {
    MemStore s; mem_init(&s);
    mem_put(&s, "a", "1", 0, 0);
    MemSnapshotLog log; snap_init(&log);
    mem_snapshot(&s, &log, "snap", 0);
    mem_put(&s, "b", "2", 0, 0);
    char diff[256];
    mem_diff(&s, &log, 0, diff, sizeof(diff));
    ASSERT(strstr(diff, "+b=2") != NULL);
}

TEST(test_diff_removed) {
    MemStore s; mem_init(&s);
    mem_put(&s, "a", "1", 0, 0);
    mem_put(&s, "b", "2", 0, 0);
    MemSnapshotLog log; snap_init(&log);
    mem_snapshot(&s, &log, "snap", 0);
    mem_delete(&s, "a");
    char diff[256];
    mem_diff(&s, &log, 0, diff, sizeof(diff));
    ASSERT(strstr(diff, "-a") != NULL);
}

TEST(test_version_history) {
    MemStore s; mem_init(&s);
    mem_put(&s, "k", "v1", 0, 0);
    mem_update(&s, "k", "v2", 0);
    MemVersionHistory hist;
    mem_version_history(&s, "k", &hist);
    ASSERT(hist.count == 1);
    ASSERT(hist.versions[0].version == 2);
    ASSERT(strcmp(hist.versions[0].value, "v2") == 0);
}

int main(void) {
    printf("flux-memory-c tests\n\n");
    RUN(test_init_empty);
    RUN(test_put_and_get);
    RUN(test_get_nonexistent);
    RUN(test_put_overwrites);
    RUN(test_delete);
    RUN(test_exists);
    RUN(test_expired_get);
    RUN(test_gc);
    RUN(test_ttl_zero);
    RUN(test_search_prefix);
    RUN(test_readonly);
    RUN(test_update_version);
    RUN(test_snapshot_restore);
    RUN(test_diff_added);
    RUN(test_diff_removed);
    RUN(test_version_history);
    printf("\n%d/%d passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
