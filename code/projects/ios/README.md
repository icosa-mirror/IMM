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

## Unity static plugin dependencies

Building `ImmUnity` also requires Python 3 and LLVM's `llvm-nm` and
`llvm-objcopy` (`brew install llvm`). After combining the plugin archive, the
build gives all symbols defined by its bundled JPEG dependency an
`_imm_unity` prefix, including references from IMM and the codec itself.
This prevents the host's JPEG implementation from satisfying calls compiled
against IMM's different JPEG struct layout. The archive check rejects leftover
original codec references and changes to other exported symbols before replacing
the output. The standalone viewer and Godot archives are not rewritten by this step.
