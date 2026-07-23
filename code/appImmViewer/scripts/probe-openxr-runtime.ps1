param(
    [string]$LoaderDll = ""
)

$ErrorActionPreference = "Stop"

function Get-ActiveOpenXrRuntimeJson {
    $runtimeKeys = @(
        "HKCU:\SOFTWARE\Khronos\OpenXR\1",
        "HKLM:\SOFTWARE\Khronos\OpenXR\1",
        "HKLM:\SOFTWARE\WOW6432Node\Khronos\OpenXR\1"
    )

    foreach ($key in $runtimeKeys) {
        if (-not (Test-Path $key)) {
            continue
        }

        $props = Get-ItemProperty -Path $key -ErrorAction SilentlyContinue
        if ($props -and $props.ActiveRuntime) {
            return $props.ActiveRuntime
        }
    }

    return $null
}

if ([string]::IsNullOrWhiteSpace($LoaderDll)) {
    $runtimeJson = Get-ActiveOpenXrRuntimeJson
    if ($runtimeJson -and (Test-Path $runtimeJson)) {
        $runtimeDir = Split-Path -Parent $runtimeJson
        $candidate = Join-Path $runtimeDir "bin\win64\openxr_loader.dll"
        if (Test-Path $candidate) {
            $LoaderDll = $candidate
        }
    }
}

if ([string]::IsNullOrWhiteSpace($LoaderDll) -or -not (Test-Path $LoaderDll)) {
    throw "OpenXR loader DLL was not found. Pass -LoaderDll or install a runtime that provides openxr_loader.dll."
}

$source = @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static unsafe class ImmOpenXrProbe
{
    private const int XR_SUCCESS = 0;
    private const int XR_ERROR_RUNTIME_FAILURE = -2;
    private const int XR_TYPE_EXTENSION_PROPERTIES = 2;
    private const int XR_TYPE_INSTANCE_CREATE_INFO = 3;
    private const int XR_TYPE_SYSTEM_GET_INFO = 4;
    private const int XR_TYPE_SYSTEM_PROPERTIES = 5;
    private const int XR_TYPE_VIEW_CONFIGURATION_VIEW = 41;
    private const int XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY = 1;
    private const int XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO = 2;
    private const ulong XR_CURRENT_API_VERSION = (1UL << 48);

    [StructLayout(LayoutKind.Sequential)]
    private unsafe struct XrExtensionProperties
    {
        public int type;
        public IntPtr next;
        public fixed byte extensionName[128];
        public uint extensionVersion;
    }

    [StructLayout(LayoutKind.Sequential)]
    private unsafe struct XrApplicationInfo
    {
        public fixed byte applicationName[128];
        public uint applicationVersion;
        public fixed byte engineName[128];
        public uint engineVersion;
        public ulong apiVersion;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct XrInstanceCreateInfo
    {
        public int type;
        public IntPtr next;
        public ulong createFlags;
        public XrApplicationInfo applicationInfo;
        public uint enabledApiLayerCount;
        public IntPtr enabledApiLayerNames;
        public uint enabledExtensionCount;
        public IntPtr enabledExtensionNames;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct XrSystemGetInfo
    {
        public int type;
        public IntPtr next;
        public int formFactor;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct XrSystemGraphicsProperties
    {
        public uint maxSwapchainImageHeight;
        public uint maxSwapchainImageWidth;
        public uint maxLayerCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct XrSystemTrackingProperties
    {
        public uint orientationTracking;
        public uint positionTracking;
    }

    [StructLayout(LayoutKind.Sequential)]
    private unsafe struct XrSystemProperties
    {
        public int type;
        public IntPtr next;
        public ulong systemId;
        public uint vendorId;
        public fixed byte systemName[256];
        public XrSystemGraphicsProperties graphicsProperties;
        public XrSystemTrackingProperties trackingProperties;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct XrViewConfigurationView
    {
        public int type;
        public IntPtr next;
        public uint recommendedImageRectWidth;
        public uint maxImageRectWidth;
        public uint recommendedImageRectHeight;
        public uint maxImageRectHeight;
        public uint recommendedSwapchainSampleCount;
        public uint maxSwapchainSampleCount;
    }

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private unsafe delegate int xrEnumerateInstanceExtensionPropertiesDelegate(
        byte* layerName,
        uint propertyCapacityInput,
        uint* propertyCountOutput,
        XrExtensionProperties* properties);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private unsafe delegate int xrCreateInstanceDelegate(
        XrInstanceCreateInfo* createInfo,
        ulong* instance);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int xrDestroyInstanceDelegate(ulong instance);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private unsafe delegate int xrGetSystemDelegate(
        ulong instance,
        XrSystemGetInfo* getInfo,
        ulong* systemId);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private unsafe delegate int xrGetSystemPropertiesDelegate(
        ulong instance,
        ulong systemId,
        XrSystemProperties* properties);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private unsafe delegate int xrEnumerateViewConfigurationsDelegate(
        ulong instance,
        ulong systemId,
        uint viewConfigurationTypeCapacityInput,
        uint* viewConfigurationTypeCountOutput,
        int* viewConfigurationTypes);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private unsafe delegate int xrEnumerateViewConfigurationViewsDelegate(
        ulong instance,
        ulong systemId,
        int viewConfigurationType,
        uint viewCapacityInput,
        uint* viewCountOutput,
        XrViewConfigurationView* views);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr LoadLibraryA(string lpFileName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    private static T Load<T>(IntPtr module, string name) where T : class
    {
        IntPtr proc = GetProcAddress(module, name);
        if (proc == IntPtr.Zero)
        {
            throw new Exception("Missing OpenXR export: " + name);
        }
        return (T)(object)Marshal.GetDelegateForFunctionPointer(proc, typeof(T));
    }

    private static void WriteFixedAscii(byte* dest, int capacity, string value)
    {
        byte[] bytes = Encoding.ASCII.GetBytes(value);
        int count = Math.Min(bytes.Length, capacity - 1);
        for (int i = 0; i < count; ++i)
        {
            dest[i] = bytes[i];
        }
        dest[count] = 0;
    }

    private static string ReadFixedAscii(byte* data, int capacity)
    {
        int len = 0;
        while (len < capacity && data[len] != 0)
        {
            ++len;
        }
        return Encoding.ASCII.GetString(data, len);
    }

    private static string ResultName(int result)
    {
        switch (result)
        {
            case XR_SUCCESS: return "XR_SUCCESS";
            case -1: return "XR_ERROR_VALIDATION_FAILURE";
            case XR_ERROR_RUNTIME_FAILURE: return "XR_ERROR_RUNTIME_FAILURE";
            case -3: return "XR_ERROR_OUT_OF_MEMORY";
            case -4: return "XR_ERROR_API_VERSION_UNSUPPORTED";
            case -6: return "XR_ERROR_INITIALIZATION_FAILED";
            case -7: return "XR_ERROR_FUNCTION_UNSUPPORTED";
            case -8: return "XR_ERROR_FEATURE_UNSUPPORTED";
            case -9: return "XR_ERROR_EXTENSION_NOT_PRESENT";
            case -34: return "XR_ERROR_FORM_FACTOR_UNSUPPORTED";
            case -35: return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
            case -51: return "XR_ERROR_RUNTIME_UNAVAILABLE";
            default: return "XR_RESULT_" + result;
        }
    }

    public static int Run(string loaderDll)
    {
        Console.WriteLine("IMM_OPENXR_PROBE loader=" + loaderDll);
        IntPtr module = LoadLibraryA(loaderDll);
        if (module == IntPtr.Zero)
        {
            Console.WriteLine("IMM_OPENXR_PROBE loadFailed=" + Marshal.GetLastWin32Error());
            return 1;
        }

        var xrEnumerateInstanceExtensionProperties = Load<xrEnumerateInstanceExtensionPropertiesDelegate>(module, "xrEnumerateInstanceExtensionProperties");
        var xrCreateInstance = Load<xrCreateInstanceDelegate>(module, "xrCreateInstance");
        var xrDestroyInstance = Load<xrDestroyInstanceDelegate>(module, "xrDestroyInstance");
        var xrGetSystem = Load<xrGetSystemDelegate>(module, "xrGetSystem");
        var xrGetSystemProperties = Load<xrGetSystemPropertiesDelegate>(module, "xrGetSystemProperties");
        var xrEnumerateViewConfigurations = Load<xrEnumerateViewConfigurationsDelegate>(module, "xrEnumerateViewConfigurations");
        var xrEnumerateViewConfigurationViews = Load<xrEnumerateViewConfigurationViewsDelegate>(module, "xrEnumerateViewConfigurationViews");

        uint extensionCount = 0;
        int result = xrEnumerateInstanceExtensionProperties(null, 0, &extensionCount, null);
        Console.WriteLine("IMM_OPENXR_PROBE enumerateExtensionsResult=" + result + " count=" + extensionCount);
        if (result != XR_SUCCESS)
        {
            return 1;
        }

        XrExtensionProperties[] extensions = new XrExtensionProperties[extensionCount];
        fixed (XrExtensionProperties* props = extensions)
        {
            for (uint i = 0; i < extensionCount; ++i)
            {
                props[i].type = XR_TYPE_EXTENSION_PROPERTIES;
            }
            result = xrEnumerateInstanceExtensionProperties(null, extensionCount, &extensionCount, props);
            Console.WriteLine("IMM_OPENXR_PROBE enumerateExtensionsFillResult=" + result + " count=" + extensionCount);
            if (result == XR_SUCCESS)
            {
                for (uint i = 0; i < extensionCount && i < 24; ++i)
                {
                    Console.WriteLine("IMM_OPENXR_PROBE extension=" + ReadFixedAscii(props[i].extensionName, 128) + " version=" + props[i].extensionVersion);
                }
            }
        }
        if (result != XR_SUCCESS)
        {
            return 1;
        }

        XrInstanceCreateInfo createInfo = new XrInstanceCreateInfo();
        createInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
        createInfo.applicationInfo.applicationVersion = 1;
        createInfo.applicationInfo.engineVersion = 1;
        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        WriteFixedAscii(createInfo.applicationInfo.applicationName, 128, "IMM OpenXR Probe");
        WriteFixedAscii(createInfo.applicationInfo.engineName, 128, "IMM");

        ulong instance = 0;
        result = xrCreateInstance(&createInfo, &instance);
        Console.WriteLine("IMM_OPENXR_PROBE createInstanceResult=" + result + " resultName=" + ResultName(result) + " instance=" + instance);
        if (result != XR_SUCCESS)
        {
            return 1;
        }

        XrSystemGetInfo systemInfo = new XrSystemGetInfo();
        systemInfo.type = XR_TYPE_SYSTEM_GET_INFO;
        systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        ulong systemId = 0;
        result = xrGetSystem(instance, &systemInfo, &systemId);
        Console.WriteLine("IMM_OPENXR_PROBE getHmdSystemResult=" + result + " resultName=" + ResultName(result) + " systemId=" + systemId);
        if (result == XR_SUCCESS)
        {
            XrSystemProperties properties = new XrSystemProperties();
            properties.type = XR_TYPE_SYSTEM_PROPERTIES;
            result = xrGetSystemProperties(instance, systemId, &properties);
            Console.WriteLine("IMM_OPENXR_PROBE getSystemPropertiesResult=" + result);
            if (result == XR_SUCCESS)
            {
                Console.WriteLine("IMM_OPENXR_PROBE systemName=" + ReadFixedAscii(properties.systemName, 256));
                Console.WriteLine("IMM_OPENXR_PROBE maxSwapchain=" + properties.graphicsProperties.maxSwapchainImageWidth + "x" + properties.graphicsProperties.maxSwapchainImageHeight + " maxLayers=" + properties.graphicsProperties.maxLayerCount);
                Console.WriteLine("IMM_OPENXR_PROBE tracking orientation=" + properties.trackingProperties.orientationTracking + " position=" + properties.trackingProperties.positionTracking);
            }

            uint viewConfigurationCount = 0;
            result = xrEnumerateViewConfigurations(instance, systemId, 0, &viewConfigurationCount, null);
            Console.WriteLine("IMM_OPENXR_PROBE enumerateViewConfigurationsResult=" + result + " count=" + viewConfigurationCount);
            if (result == XR_SUCCESS && viewConfigurationCount > 0)
            {
                int[] viewConfigurationTypes = new int[viewConfigurationCount];
                fixed (int* viewConfigurationTypePtr = viewConfigurationTypes)
                {
                    result = xrEnumerateViewConfigurations(instance, systemId, viewConfigurationCount, &viewConfigurationCount, viewConfigurationTypePtr);
                    Console.WriteLine("IMM_OPENXR_PROBE enumerateViewConfigurationsFillResult=" + result + " count=" + viewConfigurationCount);
                    if (result == XR_SUCCESS)
                    {
                        for (uint i = 0; i < viewConfigurationCount; ++i)
                        {
                            Console.WriteLine("IMM_OPENXR_PROBE viewConfigurationType=" + viewConfigurationTypePtr[i]);
                        }
                    }
                }
            }

            uint stereoViewCount = 0;
            result = xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &stereoViewCount, null);
            Console.WriteLine("IMM_OPENXR_PROBE enumerateStereoViewsResult=" + result + " count=" + stereoViewCount);
            if (result == XR_SUCCESS && stereoViewCount > 0)
            {
                XrViewConfigurationView[] views = new XrViewConfigurationView[stereoViewCount];
                fixed (XrViewConfigurationView* viewPtr = views)
                {
                    for (uint i = 0; i < stereoViewCount; ++i)
                    {
                        viewPtr[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
                    }
                    result = xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, stereoViewCount, &stereoViewCount, viewPtr);
                    Console.WriteLine("IMM_OPENXR_PROBE enumerateStereoViewsFillResult=" + result + " count=" + stereoViewCount);
                    if (result == XR_SUCCESS)
                    {
                        for (uint i = 0; i < stereoViewCount; ++i)
                        {
                            Console.WriteLine("IMM_OPENXR_PROBE stereoView[" + i + "] recommended=" + viewPtr[i].recommendedImageRectWidth + "x" + viewPtr[i].recommendedImageRectHeight + " max=" + viewPtr[i].maxImageRectWidth + "x" + viewPtr[i].maxImageRectHeight + " samples=" + viewPtr[i].recommendedSwapchainSampleCount + "/" + viewPtr[i].maxSwapchainSampleCount);
                        }
                    }
                }
            }
        }

        int destroyResult = xrDestroyInstance(instance);
        Console.WriteLine("IMM_OPENXR_PROBE destroyInstanceResult=" + destroyResult);

        return destroyResult == XR_SUCCESS ? 0 : 1;
    }
}
"@

Add-Type -TypeDefinition $source -CompilerOptions "/unsafe"
$exitCode = [ImmOpenXrProbe]::Run((Resolve-Path $LoaderDll))
if ($exitCode -ne 0) {
    exit $exitCode
}
