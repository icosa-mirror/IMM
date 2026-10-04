#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include "IUnityGraphicsVulkanMinimal.h"

// Read logical-device creation requests. Physical-device support alone does not
// establish that a feature was enabled. This observer never changes the request.
namespace ImmUnityVulkanFeatures
{
struct Header { uint32_t type; const void* next; };

inline bool RequestedMultiview(const void* chain)
{
    for (int count = 0; chain && count < 64; ++count)
    {
        Header header = {};
        std::memcpy(&header, chain, sizeof(header));
        int field = -1;
        if (header.type == 1000053001u) field = 0; // VkPhysicalDeviceMultiviewFeatures
        if (header.type == 49u) field = 4; // VkPhysicalDeviceVulkan11Features
        if (field >= 0)
        {
            uint32_t enabled = 0;
            std::memcpy(&enabled, static_cast<const unsigned char*>(chain) + sizeof(Header) + field * sizeof(uint32_t), sizeof(enabled));
            return enabled != 0;
        }
        chain = header.next;
    }
    return false;
}

using CreateDevice = int32_t(UNITY_INTERFACE_API*)(VkPhysicalDevice, const void*, const void*, VkDevice*);
struct Capture
{
    std::atomic<PFN_vkGetInstanceProcAddr> getProc{nullptr};
    std::atomic<CreateDevice> createDevice{nullptr};
    std::atomic<VkDevice> device{nullptr};
    std::atomic<bool> multiview{false};
    IUnityGraphicsVulkanV2* registered = nullptr;
    bool IsKnown(VkDevice value) const { return value && device.load(std::memory_order_acquire) == value; }
    bool IsEnabled(VkDevice value) const { return IsKnown(value) && multiview.load(std::memory_order_relaxed); }
};
inline Capture& State() { static Capture capture; return capture; }

inline int32_t UNITY_INTERFACE_API ObserveCreateDevice(VkPhysicalDevice physical, const void* request, const void* allocator, VkDevice* device)
{
    auto original = State().createDevice.load(std::memory_order_acquire);
    if (!original) return -3; // VK_ERROR_INITIALIZATION_FAILED
    Header header = {};
    if (request) std::memcpy(&header, request, sizeof(header));
    const bool multiview = RequestedMultiview(header.next);
    const int32_t result = original(physical, request, allocator, device);
    if (result == 0 && device && *device)
    {
        State().multiview.store(multiview, std::memory_order_relaxed);
        State().device.store(*device, std::memory_order_release);
    }
    return result;
}
inline PFN_vkVoidFunction UNITY_INTERFACE_API ObserveGetProc(VkInstance instance, const char* name)
{
    const auto original = State().getProc.load(std::memory_order_acquire);
    if (!original || !name) return nullptr;
    const auto function = original(instance, name);
    if (function && function != reinterpret_cast<PFN_vkVoidFunction>(ObserveCreateDevice) && std::strcmp(name, "vkCreateDevice") == 0)
    {
        State().createDevice.store(reinterpret_cast<CreateDevice>(function), std::memory_order_release);
        return reinterpret_cast<PFN_vkVoidFunction>(ObserveCreateDevice);
    }
    return function;
}
inline PFN_vkGetInstanceProcAddr UNITY_INTERFACE_API ObserveInitialization(PFN_vkGetInstanceProcAddr original, void*)
{
    if (!original || original == ObserveGetProc) return original;
    State().getProc.store(original, std::memory_order_release);
    return ObserveGetProc;
}
inline bool Install(IUnityGraphicsVulkanV2* unity)
{
    if (State().registered) return State().registered == unity;
    if (!unity || !unity->AddInterceptInitialization ||
        !unity->AddInterceptInitialization(ObserveInitialization, nullptr, 0)) return false;
    State().registered = unity;
    return true;
}
inline void Remove()
{
    auto* unity = State().registered;
    if (unity && unity->RemoveInterceptInitialization)
        unity->RemoveInterceptInitialization(ObserveInitialization);
    State().registered = nullptr;
}
}
