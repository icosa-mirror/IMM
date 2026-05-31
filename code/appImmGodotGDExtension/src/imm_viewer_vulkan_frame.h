#pragma once

#include <cstdint>

namespace godot
{
    bool ImmViewerGodotBeginVulkanTextureFrame(uint64_t instance_handle,
                                               uint64_t physical_device_handle,
                                               uint64_t device_handle,
                                               uint64_t queue_handle,
                                               uint64_t queue_family_index,
                                               uint64_t color_image_handle,
                                               uint64_t color_image_view_handle,
                                               uint32_t color_format,
                                               int width,
                                               int height);
    void ImmViewerGodotEndVulkanTextureFrame();
}
