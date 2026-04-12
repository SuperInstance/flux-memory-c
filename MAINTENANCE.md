# MAINTENANCE.md

## Architecture

Static allocation throughout — all buffers are fixed-size arrays inside structs. No `malloc`, `free`, or `calloc`. Safe for embedded and kernel-like environments.

### Key Design Decisions

- **Swap-delete**: `mem_delete` swaps with the last entry for O(1) removal. Order is not preserved.
- **Version history**: Currently only the latest version is stored (no malloc to track history). If full history is needed, a ring buffer could be embedded in MemEntry at the cost of entry size.
- **Timestamps**: Caller passes `uint64_t now` — no internal clock dependency.

## Extending

- Increase `MEM_ENTRIES_MAX`, `MEM_KEY_MAX`, `MEM_VAL_MAX` for larger stores (at memory cost).
- Add a hash index for O(1) lookup if linear scan becomes a bottleneck.
- `mem_diff` currently only reports added/removed keys. Value changes could be added with `~key=new_value\n`.

## Testing

Run `make test` to build and execute all tests. Each test is self-contained — no shared state between tests.

## Known Limitations

- No thread safety (caller must synchronize).
- Keys/values are copied with `strcpy` — not safe for binary data.
- Snapshot log is bounded at 8 entries.
