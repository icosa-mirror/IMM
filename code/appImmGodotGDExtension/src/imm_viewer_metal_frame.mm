#include "imm_viewer_metal_frame.h"

#if defined(__APPLE__)

#include "appImmGodot/src/imm_godot_plugin.h"

#import <Metal/Metal.h>

#include <cstdlib>

namespace
{
    MTLRenderPassDescriptor *g_active_pass_descriptor = nil;
}

namespace godot
{
    bool ImmViewerGodotBeginMetalTextureFrame(uint64_t command_queue_handle,
                                              uint64_t color_texture_handle,
                                              uint64_t depth_texture_handle,
                                              int width,
                                              int height)
    {
        ImmViewerGodotEndMetalTextureFrame();

        id<MTLCommandQueue> command_queue = (__bridge id<MTLCommandQueue>)(reinterpret_cast<void *>(command_queue_handle));
        id<MTLTexture> color_texture = (__bridge id<MTLTexture>)(reinterpret_cast<void *>(color_texture_handle));
        id<MTLTexture> depth_texture = (__bridge id<MTLTexture>)(reinterpret_cast<void *>(depth_texture_handle));
        if (command_queue == nil || color_texture == nil || width <= 0 || height <= 0)
        {
            return false;
        }

        MTLRenderPassDescriptor *pass_descriptor = [[MTLRenderPassDescriptor renderPassDescriptor] retain];
        pass_descriptor.colorAttachments[0].texture = color_texture;
        // This target is an IMM-only intermediate that is composited into
        // Godot's scene later. Clear it every frame so pixels from an earlier
        // document or camera pose cannot survive when the current draw covers
        // less of the texture.
        pass_descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        const char *clear_test = std::getenv("IMM_GODOT_METAL_CLEAR_TEST");
        if (clear_test != nullptr && clear_test[0] != '\0' && clear_test[0] != '0')
        {
            pass_descriptor.colorAttachments[0].clearColor = MTLClearColorMake(1.0, 0.0, 0.0, 1.0);
        }
        else
        {
            pass_descriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
        }
        pass_descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        if (depth_texture != nil)
        {
            // The Godot IMM player uses normal-Z/LESS. This intermediate is
            // sampled after the native frame and compared with Godot's
            // reverse-Z scene depth by the shared RenderingDevice compositor.
            pass_descriptor.depthAttachment.texture = depth_texture;
            pass_descriptor.depthAttachment.loadAction = MTLLoadActionClear;
            pass_descriptor.depthAttachment.storeAction = MTLStoreActionStore;
            pass_descriptor.depthAttachment.clearDepth = 1.0;
        }

        ImmGodotMetalFrame frame = {};
        frame.version = 1;
        frame.mode = ImmGodotMetalFrameMode_CommandQueueRenderPass;
        frame.commandQueue = (__bridge void *)command_queue;
        frame.renderPassDescriptor = (__bridge void *)pass_descriptor;
        frame.width = width;
        frame.height = height;

        if (ImmGodot_BeginMetalFrame(&frame) != 0)
        {
            [pass_descriptor release];
            return false;
        }

        g_active_pass_descriptor = pass_descriptor;
        return true;
    }

    void ImmViewerGodotEndMetalTextureFrame()
    {
        if (g_active_pass_descriptor == nil)
        {
            return;
        }

        ImmGodot_EndMetalFrame();
        [g_active_pass_descriptor release];
        g_active_pass_descriptor = nil;
    }
}

#endif
