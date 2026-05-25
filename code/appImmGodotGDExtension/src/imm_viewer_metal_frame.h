#pragma once

#include <cstdint>

namespace godot
{
#if defined(__APPLE__)
    bool ImmViewerGodotBeginMetalTextureFrame(uint64_t command_queue_handle,
                                              uint64_t color_texture_handle,
                                              int width,
                                              int height);
    void ImmViewerGodotEndMetalTextureFrame();
#else
    inline bool ImmViewerGodotBeginMetalTextureFrame(uint64_t, uint64_t, int, int)
    {
        return false;
    }

    inline void ImmViewerGodotEndMetalTextureFrame()
    {
    }
#endif
}
