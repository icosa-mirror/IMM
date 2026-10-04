#include <cstdio>
#include <stdexcept>
#include "../../../appImmUnity/src/imm_unity_vulkan_feature_capture.h"

namespace F = ImmUnityVulkanFeatures;
static const void* expectedRequest;
static const void* expectedAllocator;
static int32_t createResult;
static VkDevice createdDevice;
static int calls, registrations, removals, otherCalls;
static UnityVulkanInitCallback callback;
static void Require(bool condition) { if (!condition) throw std::runtime_error("Vulkan device-feature observation contract"); }
static int32_t UNITY_INTERFACE_API Create(VkPhysicalDevice, const void* request, const void* allocator, VkDevice* output)
{
    Require(request == expectedRequest && allocator == expectedAllocator);
    ++calls; *output = createdDevice; return createResult;
}
static void UNITY_INTERFACE_API Other() { ++otherCalls; }
static PFN_vkVoidFunction UNITY_INTERFACE_API GetProc(VkInstance, const char* name)
{
    if (std::strcmp(name, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Create);
    if (std::strcmp(name, "vkOther") == 0) return Other;
    return nullptr;
}
static bool UNITY_INTERFACE_API Add(UnityVulkanInitCallback value, void*, int32_t priority)
{
    Require(priority == 0); callback = value; ++registrations; return true;
}
static bool UNITY_INTERFACE_API Remove(UnityVulkanInitCallback value)
{
    Require(value == callback); ++removals; return true;
}
int main()
{
    try
    {
        struct Features { F::Header header; uint32_t flags[5]; };
        Features multiview = {{1000053001u, nullptr}, {1, 0, 0, 0, 0}};
        Features core = {{49u, nullptr}, {1, 1, 1, 1, 0}};
        F::Header unrelated = {999u, &multiview};
        Require(F::RequestedMultiview(&unrelated));
        Require(!F::RequestedMultiview(&core));
        core.flags[4] = 1; Require(F::RequestedMultiview(&core));
        Require(!F::RequestedMultiview(nullptr));
        IUnityGraphicsVulkanV2 api = {};
        api.AddInterceptInitialization = Add; api.RemoveInterceptInitialization = Remove;
        Require(F::Install(&api) && F::Install(&api) && registrations == 1);
        const auto proxy = callback(GetProc, nullptr);
        proxy(nullptr, "vkOther")(); Require(otherCalls == 1);
        Require(!proxy(nullptr, "missing") && !proxy(nullptr, nullptr));
        // Reusing our own hook must not build a recursive forwarding chain.
        Require(callback(proxy, nullptr) == proxy);
        const auto create = reinterpret_cast<F::CreateDevice>(proxy(nullptr, "vkCreateDevice"));
        F::Header request = {3, &unrelated};
        expectedRequest = &request; expectedAllocator = &core;
        createdDevice = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(123));
        VkDevice output = nullptr;
        Require(create(nullptr, &request, expectedAllocator, &output) == 0 && calls == 1);
        Require(output == createdDevice && request.next == &unrelated && multiview.flags[0] == 1);
        Require(F::State().IsKnown(output) && F::State().IsEnabled(output));
        Require(!F::State().IsKnown(nullptr));
        const auto previous = output;
        createdDevice = reinterpret_cast<VkDevice>(static_cast<uintptr_t>(456));
        createResult = -4;
        Require(create(nullptr, &request, expectedAllocator, &output) == -4);
        Require(!F::State().IsKnown(output) && F::State().IsEnabled(previous));
        createResult = 0; multiview.flags[0] = 0;
        Require(create(nullptr, &request, expectedAllocator, &output) == 0);
        Require(F::State().IsKnown(output) && !F::State().IsEnabled(output));
        F::Remove(); F::Remove(); Require(removals == 1);
        std::puts("IMM_VULKAN_FEATURE_CAPTURE PASS unchanged forwarding, successful-device identity and KHR/core feature decoding");
        return 0;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "IMM_VULKAN_FEATURE_CAPTURE FAIL %s\n", error.what()); return 1; }
}
