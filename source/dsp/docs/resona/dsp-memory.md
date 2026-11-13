# DSP buffer placement

Include `<resona/module.h>`. Append a pointer to an immutable memory profile
at the end of `resona_module`, after `cpu_profile`:

```cpp
const resona_buffer_request requests[] = {
    {1, RESONA_MEMORY_FAST, 1024, 128},
    {2, RESONA_MEMORY_PSRAM, 262144, 128},
};
const resona_memory_profile memory = {
    RESONA_MEMORY_VERSION, sizeof(resona_memory_profile), 2, requests
};
```

In init, resolve IDs and store data pointers in your state:

```cpp
const resona_buffer *b = resona_buffer_find(host->memory, 2);
state->delay = b ? static_cast<float *>(b->data) : nullptr;
```

Buffers are zeroed before init, aligned as requested, and remain valid for the
instance lifetime. Requests need unique nonzero IDs, nonzero sizes and16,32,64
or128-byte alignment; at most16 requests. Allocation never occurs in process.
State defaults to fast SRAM; set the optional module `state_region` tail to
`RESONA_MEMORY_PSRAM` for PSRAM state. There is no fixed160 KiB quota.
State and additional buffers use the
available shared heaps, preserving platform reserves and the old instance during
replacement. No automatic fallback changes your placement. A failed allocation,
CPU trial or isolation check leaves the previous effect intact.

Fast SRAM is useful for hot workspaces; PSRAM accommodates larger delay lines,
tables and granular sample buffers. CPU admission uses the selected placement.
The Device page and `resona-lab chain status` show buffer and complete arena
sizes per effect. `resona-lab pack info PACK.rpk` lists individual requests.
Complete arenas include state/stack/bounce/alignment overhead; package files
and kernel metadata consume more memory. Neither physical capacity nor a hard
request sanity bound is an available-user-memory promise.

Buffers require DSP memory/resource5. Rebuilding with the state-placement tail
requires shared-state/resource6 firmware even with a null buffer profile. Older module prefixes remain supported.
Preview mirrors memory layout and instance lifetime but has desktop timing and
executes native code without device protection. Each device instance has its own
protected arenas; stay within each buffer yourself. Do not retain pointers after
unload or access other instances, private firmware or models.

## Large main state

The optional final field of `resona_module` is `state_region`: zero or
`RESONA_MEMORY_FAST`, or `RESONA_MEMORY_PSRAM`. See
[Shared State Delay](../../examples/shared_state/README.md) for a512 KiB PSRAM
state. This requires resource-report6 firmware; Lab refuses unsupported targets
before transfer for large/PSRAM state. META binds placement to the module.

The old160 KiB quota is removed. Absolute1 MiB fast/4 MiB PSRAM declaration
sanities prevent overflow or impossible requests; they do not grant that memory.
All five slots share available heaps. Runtime overhead, reserves, fragmentation
and an existing instance retained during replacement can still refuse a load.
Admission does not evict other algorithms, erase presets or change CPU limits.

Explicit executable placement is described in [Flexible Code](../../examples/flexible_code/README.md). Code may use the dedicated fast pool or shared SRAM/PSRAM; state and buffers remain independently placed. Requires matching resource7 firmware.
