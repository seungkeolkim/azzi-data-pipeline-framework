# AZZI Data Pipeline Framework

Stage 0.5 skeleton for adding observability and cross-frame runtime metadata without wiring real
RTSP ingest. The code keeps the Stage 0 ownership model (store owns data; nodes only carry handles)
and demonstrates the required visibility features.

## What's included
- **ChannelRuntimeMeta** with TTL helpers and downstream control knobs stored by
  `ChannelRuntimeMetaStore` using a mutex-protected read/write API.
- **NodeRuntimeStateStore** providing per-node in/out/drop/error counters.
- **BoundedPointerQueue** with DropOldest behavior so push callers can log and release dropped
  handles.
- Dummy decode → dummy detection → output nodes that
  - generate synthetic frames,
  - write vehicle observations into the channel runtime meta,
  - read TTL-based presence in the output node,
  - enforce the release chain (object → buffer → frame),
  - log start/stop, drops, and sampled outputs.

## Building and running
```bash
cmake -S . -B build
cmake --build build
./build/azzi_pipeline
```

The executable prints the sampled logs showing vehicle presence decisions and counter summaries.

## Future options (documented only)
- Swap the mutex backend for lock-free/atomic channel runtime meta handling once the pipeline
  reaches later stages.
- Compile-time switches (e.g., `#define`) for choosing the store backend once configuration is
  formalized.
