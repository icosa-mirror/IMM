# IMM web decoder contract

This document describes the schema-v5 document boundary and the independently
versioned paint-packet boundary. The authoritative declarations are in `include/imm_web_decoder.h`;
this document records the ownership and marshaling rules that are not expressible
in the C header.

## Versioning

1. `IMM_WEB_OUTPUT_SCHEMA_VERSION` and `imm_web_schema_version()` identify the
   complete C-ABI and worker-document contract. The current version is 5.
2. The worker writes that version to both inspection summaries and decoded
   documents. The main-thread client rejects any version other than the one it
   was compiled to consume.
3. A document field addition, removal, reinterpretation, layout change, or
   ownership change requires a document-schema increment. Paint packets carry
   their own schema version because they are an opaque resource payload rather
   than part of the JavaScript `ImmDocument` object shape.
4. Unsupported versions are errors. They must not be accepted by ignoring
   unknown fields or by interpreting a packet using the nearest known version.

## C ABI and scalar layout

1. All exported structs use fixed-width integer fields except the byte-source
   length, which is `size_t` because it is passed directly to Wasm32 entry points.
2. Emscripten/Wasm memory is little-endian. The worker uses `DataView` with
   `littleEndian=true` for every multibyte field.
3. All ABI structs have native `static_assert` size checks. The schema-v5 sizes
   consumed by the worker are:

   | Structure | Bytes |
   |---|---:|
   | `ImmWebDocumentSummary` | 72 |
   | `ImmWebError` | 176 |
   | `ImmWebLayerInfo` | 280 |
   | `ImmWebTransform` | 36 |
   | `ImmWebAnimationInfo` | 16 |
   | `ImmWebStrokeInfo` | 40 |
   | `ImmWebStrokePoint` | 56 |
   | `ImmWebPictureInfo` | 28 |
   | `ImmWebSoundInfo` | 64 |
   | `ImmWebPlaybackInfo` | 32 |
   | `ImmWebTimelineLayerInfo` | 296 |
   | `ImmWebAnimationKey` | 80 |
   | `ImmWebChapterInfo` | 24 |
   | `ImmWebKeepAliveInfo` | 32 |
   | `ImmWebPaintPacketHeader` | 64 |
   | `ImmWebPaintGeometryRecord` | 48 |

4. Text fields are fixed-capacity UTF-8 byte arrays terminated by the first zero
   byte. Producers must leave room for the terminator.
5. Callers allocate all output structs and arrays. Count getters are queried
   before bulk array getters, and a bulk getter never writes beyond its supplied
   capacity.

## Current worker marshaling

1. `inspect` copies source bytes into Wasm, calls `imm_web_inspect`, copies the
   72-byte summary into JavaScript values, then frees all temporary Wasm memory.
2. `decode` imports the complete scene. `openMetadata` retains the source and
   native scene so later `decodeDrawing` and `decodeLayerAsset` operations can
   populate requested resources.
3. Scene and timeline metadata remain field-oriented. Paint geometry is built by
   one `imm_web_build_drawing_packet(layer_id, drawing_id, error)` call per
   requested drawing.
4. The native builder emits at most one geometry record for each of the five
   brush types. Every record contains WebGL-ready positions, colours,
   directions, visibility modes, stroke masks, draw-in progress, and
   triangle-list indices.
5. Geometry attribute lengths and index bounds are validated again on the main
   thread before a document or staged drawing is accepted.
6. A native paint packet is copied once out of non-transferable Wasm memory.
   Attribute arrays are zero-copy views into that one packet buffer, which is
   transferred to the main thread. Picture and sound payloads retain their
   existing individual transferable buffers.

## Paint packet schema 1

1. Every packet is one contiguous, little-endian byte range. The 64-byte header
   starts at byte zero; its `records_offset` identifies an array of 48-byte
   geometry records.
2. Header fields, in order, are schema version, total byte size, 64-bit resource
   ID, generation, layer ID, drawing ID, stroke count, source-point count,
   geometry count, biggest stroke, records offset, and four reserved words.
3. A resource ID is `(uint64_t(layer_id) << 32) | drawing_id`. Generation is
   nonzero and is currently 1. A future replacement of the same resource must
   increment its generation.
4. Each geometry record stores brush type, vertex and triangle counts, index
   component width, then offsets for positions, colours, directions, visibility,
   masks, progress, and indices, followed by index count.
5. Float and 32-bit index ranges are aligned to four bytes; 16-bit indices are
   aligned to two bytes. Byte attributes have no additional alignment. All
   offsets are relative to the start of the packet.
6. The consumer rejects unknown schema versions, inconsistent byte sizes or
   identities, duplicate/unknown brushes, invalid component widths, misaligned
   or out-of-range spans, inconsistent triangle counts, and out-of-range indices.

## Geometry invariants

1. `positions`, `colors`, `directions`, and `progress` are `Float32Array` values.
   `visibility` and `masks` are `Uint8Array` values.
2. Positions and directions contain three components per vertex, colours contain
   four, and every remaining vertex attribute contains one.
3. Indices are a triangle list. They use `Uint16Array` through 65,535 vertices
   and `Uint32Array` above that threshold.
4. Every index is smaller than the record's vertex count, `triangleCount` equals
   `indices.length / 3`, and a drawing cannot repeat a brush-type record.
5. Brush type values are 0 through 4, with section counts 2, 2, 7, 7, and 4.
6. Stroke masks retain the low seven bits of the source stroke index. Directional
   visibility remains a separate byte attribute.

## Ownership and lifetime

1. The browser owns input `ArrayBuffer` objects until transferring them to the
   worker. Transfer detaches the sender's buffer.
2. Wasm owns imported sequence, stroke-store, and staged-source memory until
   `imm_web_release_scene` or worker termination.
3. Pointers returned by `_malloc` are valid only until `_free`; JavaScript views
   into Wasm memory must not be retained across memory growth or another call.
4. A paint-packet pointer is valid until `imm_web_release_drawing_packet`, the
   next drawing build, or scene release. The worker copies it and invokes release
   immediately; no Wasm packet or temporary expanded geometry survives transfer.
5. Worker output buffers are owned by the worker until `postMessage` transfers
   them. The main thread owns them after transfer and Three.js references them
   until its corresponding geometry is disposed.
6. `release` deterministically releases the retained native scene and staged
   source. Terminating the worker is the cancellation fallback and releases all
   remaining Wasm state.
7. Only one scene is retained by a decoder worker. Multiple simultaneous
   documents require separate worker/client instances.

## Migration constraint

Schema 5 remains the document contract while paint packet schema 1 replaces the
normal per-stroke geometry path. Both boundaries keep browser objects and native
renderer handles out of their public formats.
