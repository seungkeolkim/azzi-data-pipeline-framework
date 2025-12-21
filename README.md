# azzi-data-pipeline-framework

## stream-pipeline (Stage 0 scaffolding)

This archive contains header-only contracts for Stage 0:
Decode -> Dummy Detection -> Output.

Key principles:
- Queues carry pointers only (no frame data copies).
- Frame buffers are owned by FrameBufferStoreInterface; FrameMetadata stores an opaque handle.
- Objects are owned by ObjectStoreInterface; FrameMetadata stores opaque object handles.
- Result/status types are component-specific (no shared global Status/Result).
