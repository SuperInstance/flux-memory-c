# flux-memory-c

Pure C11 key-value memory fabric with TTL, versioning, and snapshots.

Zero dependencies. No malloc. Stack/embedded friendly.

## Features

- **Key-value store** — up to 256 entries, 48-char keys, 512-char values
- **TTL** — per-entry expiration, `0` = never expires
- **Versioning** — version counter incremented on update
- **Snapshots** — up to 8 frozen point-in-time copies with restore
- **Diff** — compare current store to any snapshot (`+key=value` / `-key`)
- **Search** — prefix-based key lookup
- **GC** — sweep expired entries

## Build

```sh
make        # builds test binary
./test      # runs 16 tests
make clean
```

## API

See `memory.h` for full API. Key functions:

| Function | Purpose |
|----------|---------|
| `mem_init` | Zero-initialize a store |
| `mem_put` | Insert or overwrite a key |
| `mem_get` | Retrieve a key (checks expiry) |
| `mem_delete` | Remove a key |
| `mem_exists` | Check if key exists and not expired |
| `mem_update` | Update value (increments version, respects readonly flag) |
| `mem_search` | Find entries by key prefix |
| `mem_gc` | Remove all expired entries |
| `mem_snapshot` | Freeze current state |
| `mem_restore` | Overwrite store with snapshot |
| `mem_diff` | Compare store to snapshot |

## Constraints

- Fixed-size buffers, no dynamic allocation
- All memory managed by caller
- Timestamps passed in by caller (`uint64_t now`)
- Flags: `0x01` = readonly (prevents update)
