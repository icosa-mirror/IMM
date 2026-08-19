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

After the unsigned Simulator runtime and presented-drawable visual lane passes,
CI also builds the `iphoneos` arm64 application and packages
`appImmViewerIOS-unsigned-arm64.ipa`. This unsigned IPA is the signing-ready
release input; App Store or ad hoc distribution still requires an Apple
distribution identity and provisioning profile outside repository CI.
