# IMM model layer format

Model layers use metadata version 1. All scalar fields use the existing IMM
little-endian representation.

| Metadata field | Type | Values |
| --- | --- | --- |
| Version | uint32 | 1 |
| Shading model | uint32 | 0: Unlit; 1: Smooth |
| Wireframe | uint32 | 0 or 1 |

The asset contains a uint64 byte length followed by the existing `piMesh`
version-0 serialized payload. Shading and wireframe flags are preserved; the
format does not introduce new lighting or wireframe rendering behavior.

The importer validates the payload before invoking the allocating mesh reader:

1. The payload is nonempty and no larger than 256 MiB, with no trailing bytes.
2. Bounds contain finite values.
3. There are one to eight vertex streams with matching nonzero vertex counts,
   zero divisors, and nonzero strides divisible by four.
4. Each stream has one to eight attributes. Attribute types must be defined by
   `piMesh`, component counts must be one to four, normalization must be Boolean,
   and each attribute must fit within its stride.
5. There are one to 48 index arrays of triangles or quads, with 32-bit indices.
   Every referenced vertex must exist.
6. Every count and byte range must fit within the serialized payload.

Imported model layers own their decoded mesh. `LayerModel::Deinit` releases it
and resets the mesh so repeated cleanup is safe. Exporter model assignment
copies every stream, index array and the bounds through the mesh representation;
it does not use the generic single-stream `piMesh::Clone` implementation.

The earlier metadata version 0 implementation had no mesh import/export support.
It is not accepted as a populated model document by the version-1 implementation.
