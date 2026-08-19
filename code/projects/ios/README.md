# Standalone iOS viewer

`appImmViewerIOS` is the native UIKit/Metal standalone viewer. It supports iOS
15 or newer on 64-bit Metal-capable iPhone and iPad devices. The application
opens the bundled `sample1.imm` at startup and accepts `.imm` files through the
document picker or the system Open In flow.

The UIKit shell owns `MTKView`/`CAMetalLayer` presentation, landscape
orientation, resize handling, touch gestures, document picking, and foreground,
background, and audio-interruption notifications. The reusable
`MetalPlayerCore` owns IMM loading, rendering, playback timing, and AVFoundation
audio without depending on AppKit or UIKit.

Release packaging is intentionally not configured until the unsigned Simulator
runtime and visual lane passes, as required by the iOS product plan.
