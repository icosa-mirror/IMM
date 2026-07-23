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
                                               uint64_t depth_image_handle,
                                               uint64_t depth_image_view_handle,
                                               uint32_t depth_format,
                                               int width,
                                               int height,
                                               bool clear_external_depth)
    {
        ImmGodotVulkanFrame frame = {};
        frame.version = 2;
        frame.instance = reinterpret_cast<void *>(instance_handle);
        frame.physicalDevice = reinterpret_cast<void *>(physical_device_handle);
        frame.device = reinterpret_cast<void *>(device_handle);
        frame.graphicsQueue = reinterpret_cast<void *>(queue_handle);
        frame.graphicsQueueFamilyIndex = static_cast<uint32_t>(queue_family_index);
        frame.colorImage = reinterpret_cast<void *>(color_image_handle);
        frame.colorImageView = reinterpret_cast<void *>(color_image_view_handle);
        frame.colorFormat = color_format;
        frame.depthImage = reinterpret_cast<void *>(depth_image_handle);
        frame.depthImageView = reinterpret_cast<void *>(depth_image_view_handle);
        frame.depthFormat = depth_format;
        frame.width = width;
        frame.height = height;
        frame.flags = clear_external_depth ? ImmGodotVulkanFrameFlag_ClearExternalDepth : 0;
        return ImmGodot_BeginVulkanFrame(&frame) == 0;
    }

    void ImmViewerGodotEndVulkanTextureFrame()
    {
        ImmGodot_EndVulkanFrame();
    }
}
