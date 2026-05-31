#include "imm_viewer_vulkan_frame.h"

#include "appImmGodot/src/imm_godot_plugin.h"

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
                                               int height)
    {
        ImmGodotVulkanFrame frame = {};
        frame.version = 1;
        frame.instance = reinterpret_cast<void *>(instance_handle);
        frame.physicalDevice = reinterpret_cast<void *>(physical_device_handle);
        frame.device = reinterpret_cast<void *>(device_handle);
        frame.graphicsQueue = reinterpret_cast<void *>(queue_handle);
        frame.graphicsQueueFamilyIndex = static_cast<uint32_t>(queue_family_index);
        frame.colorImage = reinterpret_cast<void *>(color_image_handle);
        frame.colorImageView = reinterpret_cast<void *>(color_image_view_handle);
        frame.colorFormat = color_format;
        frame.width = width;
        frame.height = height;
        return ImmGodot_BeginVulkanFrame(&frame) == 0;
    }

    void ImmViewerGodotEndVulkanTextureFrame()
    {
        ImmGodot_EndVulkanFrame();
    }
}
