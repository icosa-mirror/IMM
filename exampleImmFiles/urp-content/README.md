# URP content fixtures

These ten small IMM documents contain one content layer and a camera spawn at
`(0, 0, 3)`: four authorable paint brush sections, five picture formats, and an
unlit model with four vertices and two triangles.
`manifest.json` identifies each format and distinguishes surfaces from panorama
backdrops. The stereo panorama uses different red and green halves; cubemap faces
have distinct red values. They contain no private artwork.

Regenerate with the Windows native libraries built in Release and the Vulkan SDK
available to CMake:

```powershell
cmake -S code/libImmCore/tests/vulkan -B artifacts/vulkan-model-tests -DIMM_VULKAN_MODEL_SMOKE=ON
cmake --build artifacts/vulkan-model-tests --config Release --target imm_urp_content_fixtures
artifacts/vulkan-model-tests/Release/imm_urp_content_fixtures.exe artifacts/regenerated-urp-content exampleImmFiles/urp-content
```

The utility exports each document, imports it using both paint storage techniques,
checks decoded content type, dimensions, brush geometry or model mesh, and compares generated
files and manifest byte for byte with this directory. It runs without initializing
a GPU. The same check is registered as `urp_content_fixture_roundtrip` in CTest and
is required by the Windows build workflow.

These fixtures establish authoring and import inputs. Unity mono/stereo rendering,
depth, transparency, MSAA and skybox ordering require separate rendering checks.
The model uses the version-1 layer format documented in
[`docs/imm-model-layer-format.md`](../../docs/imm-model-layer-format.md). Its
document roundtrip establishes persistence and import, not rendering correctness.
