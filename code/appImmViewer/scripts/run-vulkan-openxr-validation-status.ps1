param(
    [string]$Configuration = "Release",
    [string]$Adb = $env:ADB,
    [int]$AndroidWaitSeconds = 10,
    [switch]$SkipBuild,
    [switch]$SkipWindowsVulkanSmoke,
    [switch]$SkipWindowsDirectXBaseline,
    [switch]$SkipFakeOpenXrProbe,
    [switch]$SkipStaticConfigChecks,
    [switch]$SkipAndroidManifestChecks,
    [switch]$SkipAndroidOpenXrComponentProbe,
    [switch]$SkipAndroidBuilds,
    [switch]$SkipAndroidRuntime,
    [switch]$SkipHardwareStateProbes,
    [switch]$SkipWindowsOpenXrProbe
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
$androidDir = Join-Path $repoRoot "code\projects\android"
$timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz"
$safeTimestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$reportDir = Join-Path $repoRoot "build\validation\vulkan-openxr-$safeTimestamp"
$reportPath = Join-Path $reportDir "validation-status.txt"

New-Item -ItemType Directory -Force $reportDir | Out-Null

$results = New-Object System.Collections.Generic.List[object]

function Add-Result {
    param(
        [string]$Name,
        [string]$Status,
        [string]$Evidence,
        [string]$Notes = ""
    )

    $results.Add([pscustomobject]@{
        Name = $Name
        Status = $Status
        Evidence = $Evidence
        Notes = $Notes
    }) | Out-Null
}

function Invoke-Captured {
    param(
        [string]$Name,
        [scriptblock]$Command,
        [string]$OutputName,
        [string[]]$BlockedPatterns = @(),
        [string[]]$ExpectedFailurePatterns = @(),
        [switch]$NonZeroIsBlocked
    )

    $outputPath = Join-Path $reportDir $OutputName
    try {
        $global:LASTEXITCODE = 0
        $output = & $Command 2>&1
        $exitCode = if ($LASTEXITCODE -ne $null) { $LASTEXITCODE } else { 0 }
        $output | Out-File -FilePath $outputPath -Encoding utf8

        $text = [string]::Join([Environment]::NewLine, @($output))
        foreach ($pattern in $BlockedPatterns) {
            if ($text -match $pattern) {
                Add-Result $Name "BLOCKED" $outputPath "Matched blocker: $pattern"
                return
            }
        }

        if ($exitCode -eq 0) {
            Add-Result $Name "PASS" $outputPath
            return
        }

        foreach ($pattern in $ExpectedFailurePatterns) {
            if ($text -match $pattern) {
                Add-Result $Name "FAIL" $outputPath "Expected incomplete-path failure: $pattern"
                return
            }
        }

        if ($NonZeroIsBlocked) {
            Add-Result $Name "BLOCKED" $outputPath "Exit code $exitCode from hardware/runtime-gated check"
        } else {
            Add-Result $Name "FAIL" $outputPath "Exit code $exitCode"
        }
    }
    catch {
        $message = $_.Exception.Message
        $message | Out-File -FilePath $outputPath -Encoding utf8

        foreach ($pattern in $BlockedPatterns) {
            if ($message -match $pattern) {
                Add-Result $Name "BLOCKED" $outputPath "Matched blocker: $pattern"
                return
            }
        }

        foreach ($pattern in $ExpectedFailurePatterns) {
            if ($message -match $pattern) {
                Add-Result $Name "FAIL" $outputPath "Expected incomplete-path failure: $pattern"
                return
            }
        }

        Add-Result $Name "FAIL" $outputPath $message
    }
}

function Resolve-AdbForStatus {
    param([string]$Requested)

    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-Path $Requested).Path
    }

    $cmd = Get-Command adb -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $localProperties = Join-Path $androidDir "local.properties"
    if (Test-Path $localProperties) {
        $sdkLine = Get-Content $localProperties | Where-Object { $_ -match "^sdk\.dir=" } | Select-Object -First 1
        if ($sdkLine) {
            $sdkDir = ($sdkLine -replace "^sdk\.dir=", "").Replace("\\", "\")
            $candidate = Join-Path $sdkDir "platform-tools\adb.exe"
            if (Test-Path $candidate) {
                return (Resolve-Path $candidate).Path
            }
        }
    }

    if ($env:ANDROID_SDK_ROOT) {
        $candidate = Join-Path $env:ANDROID_SDK_ROOT "platform-tools\adb.exe"
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    return ""
}

function Invoke-PwshFile {
    param(
        [string]$File,
        [string[]]$Arguments = @()
    )

    $pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
    if (-not $pwsh) {
        $pwsh = Get-Command powershell -ErrorAction SilentlyContinue
    }
    if (-not $pwsh) {
        throw "PowerShell executable was not found for child-process capture."
    }

    & $pwsh.Source -NoProfile -ExecutionPolicy Bypass -File $File @Arguments 2>&1
}

Invoke-Captured `
    -Name "Windows OpenXR dependencies" `
    -OutputName "windows-openxr-deps.txt" `
    -Command { & (Join-Path $scriptDir "check-openxr-deps.ps1") }

if (-not $SkipStaticConfigChecks) {
    Invoke-Captured `
        -Name "Static renderer/XR config" `
        -OutputName "static-renderer-xr-config.txt" `
        -Command {
            function Read-JsonFile([string]$PathValue) {
                if (-not (Test-Path $PathValue)) {
                    throw "Missing JSON config: $PathValue"
                }
                return Get-Content -LiteralPath $PathValue -Raw | ConvertFrom-Json
            }

            function Assert-RenderingConfig($Config, [string]$Name, [bool]$EnableVR, [string]$RenderingApi, [string]$XrRuntime) {
                if ([bool]$Config.Rendering.EnableVR -ne $EnableVR) {
                    throw "$Name EnableVR expected $EnableVR but was $($Config.Rendering.EnableVR)"
                }
                if ($Config.Rendering.RenderingAPI -ne $RenderingApi) {
                    throw "$Name RenderingAPI expected $RenderingApi but was $($Config.Rendering.RenderingAPI)"
                }
                if ($Config.Rendering.XRRuntime -ne $XrRuntime) {
                    throw "$Name XRRuntime expected $XrRuntime but was $($Config.Rendering.XRRuntime)"
                }
            }

            $settingsDefaultPath = Join-Path $repoRoot "code\appImmViewer\exe\settings.json"
            $settingsVulkanSmokePath = Join-Path $repoRoot "code\appImmViewer\exe\settings-vulkan-smoke.json"
            $settingsOpenXrProbePath = Join-Path $repoRoot "code\appImmViewer\exe\settings-openxr-probe.json"
            $settingsDefault = Read-JsonFile $settingsDefaultPath
            $settingsVulkanSmoke = Read-JsonFile $settingsVulkanSmokePath
            $settingsOpenXrProbe = Read-JsonFile $settingsOpenXrProbePath

            Assert-RenderingConfig $settingsDefault "settings.json" $false "Vulkan" "Legacy"
            Assert-RenderingConfig $settingsVulkanSmoke "settings-vulkan-smoke.json" $false "Vulkan" "Legacy"
            Assert-RenderingConfig $settingsOpenXrProbe "settings-openxr-probe.json" $true "Vulkan" "OpenXR"

            $gradlePath = Join-Path $androidDir "appImmViewer\build.gradle"
            if (-not (Test-Path $gradlePath)) {
                throw "Missing Android build.gradle: $gradlePath"
            }
            $gradleText = Get-Content -LiteralPath $gradlePath -Raw
            $requiredGradleSnippets = @(
                'def immNonVr = project.findProperty("immNonVr") ?: "ON"',
                'def immRendererApi = project.findProperty("immRendererApi") ?: (immNonVr == "ON" ? "Vulkan" : "GLES")',
                'def immXrRuntime = project.findProperty("immXrRuntime") ?: "Legacy"',
                'def immManifest = project.findProperty("immManifest") ?: (immNonVr == "ON" ? "nonVr" : "vr")',
                '"-DIMM_ANDROID_NON_VR=${immNonVr}"',
                '"-DIMM_ANDROID_RENDERER_API=${immRendererApi}"',
                '"-DIMM_ANDROID_XR_RUNTIME=${immXrRuntime}"',
                'manifest.srcFile immManifest == "vr" ? "src/vr/AndroidManifest.xml" : "src/nonVr/AndroidManifest.xml"'
            )
            foreach ($snippet in $requiredGradleSnippets) {
                if (-not $gradleText.Contains($snippet)) {
                    throw "Android build.gradle missing expected snippet: $snippet"
                }
            }

            $cmakePath = Join-Path $androidDir "appImmViewer\CMakeLists.txt"
            if (-not (Test-Path $cmakePath)) {
                throw "Missing Android CMakeLists.txt: $cmakePath"
            }
            $cmakeText = Get-Content -LiteralPath $cmakePath -Raw
            $requiredCmakeSnippets = @(
                'if (IMM_ANDROID_XR_RUNTIME STREQUAL "OpenXR")',
                'IMM_ANDROID_XR_RUNTIME_OPENXR=1',
                '${THIRDPARTY_DIR}/openxr-sdk/include'
            )
            foreach ($snippet in $requiredCmakeSnippets) {
                if (-not $cmakeText.Contains($snippet)) {
                    throw "Android CMakeLists.txt missing expected snippet: $snippet"
                }
            }

            Write-Output "settings.json=RenderingAPI Vulkan, XRRuntime Legacy, EnableVR false"
            Write-Output "settings-vulkan-smoke.json=RenderingAPI Vulkan, XRRuntime Legacy, EnableVR false"
            Write-Output "settings-openxr-probe.json=RenderingAPI Vulkan, XRRuntime OpenXR, EnableVR true"
            Write-Output "androidBuildGradle=$gradlePath"
            Write-Output "androidCMakeLists=$cmakePath"
            Write-Output "androidDefault=immNonVr ON, renderer Vulkan for non-VR, GLES for VR"
            Write-Output "androidXrRuntimeDefault=Legacy, explicit OpenXR probe through -PimmXrRuntime=OpenXR"
            Write-Output "androidCMakeDefines=IMM_ANDROID_NON_VR, IMM_ANDROID_RENDERER_API, IMM_ANDROID_XR_RUNTIME"
            Write-Output "androidManifestSelection=src/nonVr for immManifest nonVr, src/vr for immManifest vr"
        }
} else {
    Add-Result "Static renderer/XR config" "SKIPPED" "" "-SkipStaticConfigChecks"
}

if (-not $SkipStaticConfigChecks) {
    Invoke-Captured `
        -Name "Vulkan external image frame boundary" `
        -OutputName "vulkan-external-image-frame-boundary.txt" `
        -Command {
            $rendererHeaderPath = Join-Path $repoRoot "code\libImmCore\src\libRender\vulkan\piVulkan_Renderer.h"
            $rendererSourcePath = Join-Path $repoRoot "code\libImmCore\src\libRender\vulkan\piVulkan_Renderer.cpp"
            $fakeLoaderSourcePath = Join-Path $repoRoot "code\appImmViewer\scripts\fake_openxr_loader.cpp"
            if (-not (Test-Path $rendererHeaderPath)) {
                throw "Missing Vulkan renderer header: $rendererHeaderPath"
            }
            if (-not (Test-Path $rendererSourcePath)) {
                throw "Missing Vulkan renderer source: $rendererSourcePath"
            }
            if (-not (Test-Path $fakeLoaderSourcePath)) {
                throw "Missing fake OpenXR loader source: $fakeLoaderSourcePath"
            }

            $headerText = Get-Content -LiteralPath $rendererHeaderPath -Raw
            $sourceText = Get-Content -LiteralPath $rendererSourcePath -Raw
            $fakeLoaderText = Get-Content -LiteralPath $fakeLoaderSourcePath -Raw

            $requiredHeaderSnippets = @(
                "bool BeginExternalImageFrame(void *image, uint32_t vkFormat, int width, int height, int arrayLayers);",
                "bool BeginExternalImageFrame(void *image, void *imageView, uint32_t vkFormat, int width, int height);",
                "void EndExternalImageFrame(void);",
                "bool BeginExternalImageFrameWithView(void *image, void *imageView, uint32_t vkFormat, int width, int height, int arrayLayers, bool ownsImageView);"
            )
            foreach ($snippet in $requiredHeaderSnippets) {
                if (-not $headerText.Contains($snippet)) {
                    throw "Vulkan renderer header missing external image frame API: $snippet"
                }
            }

            $requiredSourceSnippets = @(
                "bool ownsImageView = true;",
                "bool piRendererVulkan::BeginExternalImageFrame(void *image, uint32_t vkFormat, int width, int height, int arrayLayers)",
                "viewInfo.image = static_cast<VkImage>(reinterpret_cast<uintptr_t>(image));",
                "viewInfo.viewType = arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;",
                "viewInfo.subresourceRange.layerCount = static_cast<uint32_t>(arrayLayers);",
                "mState->vkCreateImageView(mState->device, &viewInfo, nullptr, &imageView)",
                "BeginExternalImageFrameWithView(image, reinterpret_cast<void *>(imageView), vkFormat, width, height, arrayLayers, false)",
                "mState->externalFrameColorTexture->ownsImageView = true;",
                "Vulkan renderer began external image frame with owned image view",
                "bool piRendererVulkan::BeginExternalImageFrame(void *image, void *imageView, uint32_t vkFormat, int width, int height)",
                "return BeginExternalImageFrameWithView(image, imageView, vkFormat, width, height, 1, false);",
                "bool piRendererVulkan::BeginExternalImageFrameWithView(void *image, void *imageView, uint32_t vkFormat, int width, int height, int arrayLayers, bool ownsImageView)",
                "EndExternalImageFrame();",
                "image == nullptr || imageView == nullptr || width <= 0 || height <= 0 || arrayLayers <= 0 || vkFormat == 0",
                "colorTexture->externalHandle = reinterpret_cast<uint64_t>(image);",
                "colorTexture->image = static_cast<VkImage>(reinterpret_cast<uintptr_t>(image));",
                "colorTexture->imageView = static_cast<VkImageView>(reinterpret_cast<uintptr_t>(imageView));",
                "colorTexture->ownsImageView = ownsImageView;",
                "colorTexture->vkFormat = static_cast<VkFormat>(vkFormat);",
                "VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT",
                "colorTexture->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;",
                "CreateTexture(L`"imm_external_image_depth`", &depthInfo",
                "CreateRenderTarget(colorTexture, nullptr, nullptr, nullptr, depthTexture)",
                "mState->externalFrameRenderTarget = renderTarget;",
                "void piRendererVulkan::EndExternalImageFrame(void)",
                "mState->externalFrameRenderTarget = nullptr;",
                "const bool ownsVulkanImage = obj->externalHandle == 0;",
                "if ((ownsVulkanImage || obj->ownsImageView) && obj->imageView != VK_NULL_IMAGE_VIEW",
                "if (ownsVulkanImage && obj->image != 0",
                "Vulkan external texture wrapping is not implemented yet"
            )
            foreach ($snippet in $requiredSourceSnippets) {
                if (-not $sourceText.Contains($snippet)) {
                    throw "Vulkan renderer source missing external image boundary invariant: $snippet"
                }
            }

            $requiredFakeLoaderSnippets = @(
                "static constexpr int64_t XR_FAKE_SWAPCHAIN_FORMAT = 44;",
                "static constexpr uint32_t XR_FAKE_SWAPCHAIN_WIDTH = 1600;",
                "static constexpr uint32_t XR_FAKE_SWAPCHAIN_HEIGHT = 1600;",
                "static constexpr uint32_t XR_FAKE_SWAPCHAIN_ARRAY_SIZE = 2;",
                "(createInfo->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) == 0",
                "createInfo->format != XR_FAKE_SWAPCHAIN_FORMAT",
                "createInfo->width != XR_FAKE_SWAPCHAIN_WIDTH",
                "createInfo->height != XR_FAKE_SWAPCHAIN_HEIGHT",
                "createInfo->arraySize != XR_FAKE_SWAPCHAIN_ARRAY_SIZE",
                "return XR_ERROR_VALIDATION_FAILURE;"
            )
            foreach ($snippet in $requiredFakeLoaderSnippets) {
                if (-not $fakeLoaderText.Contains($snippet)) {
                    throw "Fake OpenXR loader missing swapchain contract invariant: $snippet"
                }
            }

            Write-Output "api=BeginExternalImageFrame(image,vkFormat,width,height,arrayLayers), BeginExternalImageFrame(image,imageView,vkFormat,width,height), EndExternalImageFrame"
            Write-Output "openXrShape=VkImage plus explicit VkFormat/dimensions/arrayLayers creates an owned VkImageView"
            Write-Output "fakeOpenXrSwapchain=requires color attachment usage, format 44, 1600x1600, arrayLayers 2"
            Write-Output "externalColor=VkImage plus optional caller-provided VkImageView, explicit VkFormat, color attachment layout"
            Write-Output "depth=internal imm_external_image_depth texture"
            Write-Output "renderTarget=external color plus internal depth"
            Write-Output "ownership=external images are not destroyed when externalHandle is non-zero; renderer-owned image views are destroyed"
            Write-Output "legacyIntegerWrapper=CreateTextureFromID remains explicitly unsupported"
        }
} else {
    Add-Result "Vulkan external image frame boundary" "SKIPPED" "" "-SkipStaticConfigChecks"
}

if (-not $SkipWindowsVulkanSmoke) {
    Invoke-Captured `
        -Name "Windows Vulkan non-VR sample1 smoke" `
        -OutputName "windows-vulkan-smoke.txt" `
        -Command {
            $arguments = @(
                "-Configuration", $Configuration,
                "-DurationSeconds", "60",
                "-PresentSeconds", "5",
                "-KeepArtifacts"
            )
            if ($SkipBuild) {
                $arguments += "-SkipBuild"
            }
            Invoke-PwshFile -File (Join-Path $scriptDir "run-vulkan-sample1-smoke.ps1") -Arguments $arguments
        }
} else {
    Add-Result "Windows Vulkan non-VR sample1 smoke" "SKIPPED" "" "-SkipWindowsVulkanSmoke"
}

if (-not $SkipWindowsDirectXBaseline) {
    Invoke-Captured `
        -Name "Windows DirectX baseline sample1 capture" `
        -OutputName "windows-directx-baseline.txt" `
        -Command {
            $arguments = @(
                "-Configuration", $Configuration,
                "-TimeoutSeconds", "90"
            )
            if ($SkipBuild) {
                $arguments += "-SkipBuild"
            }
            Invoke-PwshFile -File (Join-Path $scriptDir "capture_windows_directx_baseline.ps1") -Arguments $arguments
        }
} else {
    Add-Result "Windows DirectX baseline sample1 capture" "SKIPPED" "" "-SkipWindowsDirectXBaseline"
}

if (-not $SkipFakeOpenXrProbe) {
    $fakeLoaderDll = Join-Path $repoRoot "build\openxr-fake-loader\openxr_loader.dll"
    Invoke-Captured `
        -Name "Windows fake OpenXR loader build" `
        -OutputName "windows-openxr-fake-loader-build.txt" `
        -Command { & (Join-Path $scriptDir "build-fake-openxr-loader.ps1") }

    Invoke-Captured `
        -Name "Windows OpenXR fake-loader standalone probe" `
        -OutputName "windows-openxr-fake-loader-probe.txt" `
        -Command { Invoke-PwshFile -File (Join-Path $scriptDir "probe-openxr-runtime.ps1") -Arguments @("-LoaderDll", $fakeLoaderDll) }

    Invoke-Captured `
        -Name "Windows fake OpenXR swapchain contract" `
        -OutputName "windows-openxr-fake-loader-swapchain-contract.txt" `
        -Command {
            $source = @'
using System;
using System.Runtime.InteropServices;

public static class ImmFakeOpenXrSwapchainContract
{
    [StructLayout(LayoutKind.Sequential)]
    public struct XrSwapchainCreateInfoImm
    {
        public int type;
        public IntPtr next;
        public ulong createFlags;
        public ulong usageFlags;
        public long format;
        public uint sampleCount;
        public uint width;
        public uint height;
        public uint faceCount;
        public uint arraySize;
        public uint mipCount;
    }

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    public delegate int XrCreateSwapchainDelegate(ulong session, ref XrSwapchainCreateInfoImm createInfo, out ulong swapchain);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr LoadLibraryW(string fileName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr module, string procName);

    public static XrCreateSwapchainDelegate LoadCreateSwapchain(string dllPath)
    {
        IntPtr module = LoadLibraryW(dllPath);
        if (module == IntPtr.Zero)
        {
            throw new InvalidOperationException("LoadLibraryW failed: " + Marshal.GetLastWin32Error());
        }
        IntPtr proc = GetProcAddress(module, "xrCreateSwapchain");
        if (proc == IntPtr.Zero)
        {
            throw new InvalidOperationException("GetProcAddress(xrCreateSwapchain) failed: " + Marshal.GetLastWin32Error());
        }
        return (XrCreateSwapchainDelegate)Marshal.GetDelegateForFunctionPointer(proc, typeof(XrCreateSwapchainDelegate));
    }
}
'@
            Add-Type -TypeDefinition $source
            $xrCreateSwapchain = [ImmFakeOpenXrSwapchainContract]::LoadCreateSwapchain($fakeLoaderDll)

            $valid = New-Object ImmFakeOpenXrSwapchainContract+XrSwapchainCreateInfoImm
            $valid.type = 9
            $valid.usageFlags = 1
            $valid.format = 44
            $valid.sampleCount = 1
            $valid.width = 1600
            $valid.height = 1600
            $valid.faceCount = 1
            $valid.arraySize = 2
            $valid.mipCount = 1

            $badFormat = $valid
            $badFormat.format = 45
            $swapchain = [UInt64]0
            $badFormatResult = $xrCreateSwapchain.Invoke(0, [ref]$badFormat, [ref]$swapchain)
            if ($badFormatResult -ne -1) {
                throw "Fake loader accepted wrong swapchain format; result=$badFormatResult"
            }

            $badLayers = $valid
            $badLayers.arraySize = 1
            $swapchain = [UInt64]0
            $badLayersResult = $xrCreateSwapchain.Invoke(0, [ref]$badLayers, [ref]$swapchain)
            if ($badLayersResult -ne -1) {
                throw "Fake loader accepted wrong swapchain array size; result=$badLayersResult"
            }

            $swapchain = [UInt64]0
            $validResult = $xrCreateSwapchain.Invoke(0, [ref]$valid, [ref]$swapchain)
            if ($validResult -ne 0 -or $swapchain -ne 0x2468ace0) {
                throw "Fake loader rejected valid swapchain contract; result=$validResult swapchain=0x$($swapchain.ToString('x'))"
            }

            Write-Output "badFormatResult=$badFormatResult"
            Write-Output "badArraySizeResult=$badLayersResult"
            Write-Output "validResult=$validResult swapchain=0x$($swapchain.ToString('x'))"
            Write-Output "contract=format 44, 1600x1600, arrayLayers 2, color attachment"
        }

    Invoke-Captured `
        -Name "Windows app-side OpenXR fake-loader probe" `
        -OutputName "windows-openxr-fake-loader-app-probe.txt" `
        -Command {
            $exeDir = Join-Path $repoRoot "code\appImmViewer\exe"
            $exeName = if ($Configuration -ieq "Debug") { "appImmViewer_Debug.exe" } else { "appImmViewer_Release.exe" }
            $viewerExe = Join-Path $exeDir $exeName
            $debugLog = Join-Path $exeDir "debug.txt"
            if (-not (Test-Path $viewerExe)) {
                throw "Viewer executable was not found: $viewerExe"
            }
            if (-not (Test-Path $fakeLoaderDll)) {
                throw "Fake OpenXR loader was not found: $fakeLoaderDll"
            }
            Remove-Item -LiteralPath $debugLog -Force -ErrorAction SilentlyContinue
            $previousLoader = $env:IMM_OPENXR_LOADER_DLL
            $env:IMM_OPENXR_LOADER_DLL = $fakeLoaderDll
            try {
                $process = Start-Process -FilePath $viewerExe -ArgumentList @("settings-openxr-probe.json") -WorkingDirectory $exeDir -WindowStyle Hidden -PassThru
                if (-not $process.WaitForExit(30000)) {
                    Stop-Process -Id $process.Id -Force
                    throw "App-side OpenXR fake-loader probe timed out"
                }
            }
            finally {
                if ($null -eq $previousLoader) {
                    Remove-Item -Path "env:IMM_OPENXR_LOADER_DLL" -ErrorAction SilentlyContinue
                } else {
                    $env:IMM_OPENXR_LOADER_DLL = $previousLoader
                }
            }
            if (-not (Test-Path $debugLog)) {
                throw "App-side OpenXR fake-loader probe did not write debug log"
            }
            $log = Get-Content $debugLog -Raw
            $markers = @(
                "XR Runtime: OpenXR",
                "IMM_OPENXR_STANDALONE loader=$fakeLoaderDll",
                "IMM_OPENXR_STANDALONE createInstanceResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE getHmdSystemResult=0 resultName=XR_SUCCESS systemId=66",
                "IMM_OPENXR_STANDALONE getVulkanInstanceExtensionsProcResult=0 resultName=XR_SUCCESS available=1",
                "IMM_OPENXR_STANDALONE getVulkanInstanceExtensionsFillResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE vulkanInstanceExtensions=VK_KHR_surface VK_KHR_win32_surface",
                "IMM_OPENXR_STANDALONE getVulkanDeviceExtensionsProcResult=0 resultName=XR_SUCCESS available=1",
                "IMM_OPENXR_STANDALONE getVulkanDeviceExtensionsFillResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE vulkanDeviceExtensions=VK_KHR_swapchain",
                "IMM_OPENXR_STANDALONE getVulkanGraphicsRequirementsProcResult=0 resultName=XR_SUCCESS available=1",
                "IMM_OPENXR_STANDALONE getVulkanGraphicsRequirementsResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE enumerateStereoViewsFillResult=0 count=2",
                "IMM_OPENXR_STANDALONE vulkanGraphicsBinding type=1000025000 instance=0x1111222233334444 physicalDevice=0x2222333344445555 device=0x3333444455556666 queueFamily=7 queueIndex=1",
                "IMM_OPENXR_STANDALONE createSessionResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE enumerateSwapchainFormatsFillResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE createSwapchainResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE enumerateSwapchainImagesFillResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE rendererExternalImageFrameCandidate image=0x13579bdf vkFormat=44 width=1600 height=1600 arrayLayers=2",
                "IMM_OPENXR_STANDALONE beginSessionResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE waitFrameResult=0 resultName=XR_SUCCESS shouldRender=1",
                "IMM_OPENXR_STANDALONE beginFrameResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE acquireSwapchainImageResult=0 resultName=XR_SUCCESS index=0",
                "IMM_OPENXR_STANDALONE waitSwapchainImageResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE releaseSwapchainImageResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE endFrameResult=0 resultName=XR_SUCCESS layerCount=1 projectionViews=2",
                "IMM_OPENXR_STANDALONE endSessionResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE destroySwapchainResult=0 resultName=XR_SUCCESS",
                "IMM_OPENXR_STANDALONE destroySessionResult=0 resultName=XR_SUCCESS",
                "OpenXR standalone startup probe passed; the OpenXR VR backend is not implemented yet"
            )
            foreach ($marker in $markers) {
                if (-not $log.Contains($marker)) {
                    throw "App-side OpenXR fake-loader probe missing marker: $marker"
                }
            }
            $log -split "`r?`n" | Where-Object { $_ -match "IMM_OPENXR_STANDALONE|XR Runtime|OpenXR standalone startup probe" }
        }
} else {
    Add-Result "Windows fake OpenXR loader build" "SKIPPED" "" "-SkipFakeOpenXrProbe"
    Add-Result "Windows OpenXR fake-loader standalone probe" "SKIPPED" "" "-SkipFakeOpenXrProbe"
    Add-Result "Windows fake OpenXR swapchain contract" "SKIPPED" "" "-SkipFakeOpenXrProbe"
    Add-Result "Windows app-side OpenXR fake-loader probe" "SKIPPED" "" "-SkipFakeOpenXrProbe"
}

if (-not $SkipWindowsOpenXrProbe) {
    Invoke-Captured `
        -Name "Windows OpenXR runtime probe" `
        -OutputName "windows-openxr-runtime-probe.txt" `
        -Command { Invoke-PwshFile -File (Join-Path $scriptDir "probe-openxr-runtime.ps1") } `
        -BlockedPatterns @(
            "XR_ERROR_RUNTIME_FAILURE",
            "XR_ERROR_RUNTIME_UNAVAILABLE",
            "WirelessHmdNotConnected",
            "HmdNotFound"
        ) `
        -NonZeroIsBlocked

    $metaRuntimeJson = "C:\Program Files\Meta Horizon\Support\oculus-runtime\oculus_openxr_64.json"
    if (Test-Path $metaRuntimeJson) {
        Invoke-Captured `
            -Name "Windows Meta OpenXR runtime override probe" `
            -OutputName "windows-meta-openxr-runtime-override-probe.txt" `
            -Command {
                $previousRuntimeJson = $env:XR_RUNTIME_JSON
                $env:XR_RUNTIME_JSON = $metaRuntimeJson
                try {
                    $output = @(Invoke-PwshFile -File (Join-Path $scriptDir "probe-openxr-runtime.ps1"))
                }
                finally {
                    if ($null -eq $previousRuntimeJson) {
                        Remove-Item -Path "env:XR_RUNTIME_JSON" -ErrorAction SilentlyContinue
                    } else {
                        $env:XR_RUNTIME_JSON = $previousRuntimeJson
                    }
                }

                $text = [string]::Join([Environment]::NewLine, $output)
                if (-not $text.Contains("IMM_OPENXR_PROBE createInstanceResult=0 resultName=XR_SUCCESS")) {
                    throw "Meta OpenXR runtime override probe did not create an OpenXR instance"
                }
                if (-not $text.Contains("IMM_OPENXR_PROBE getHmdSystemResult=-35 resultName=XR_ERROR_FORM_FACTOR_UNAVAILABLE systemId=0")) {
                    throw "Meta OpenXR runtime override probe did not report the expected no-HMD system result"
                }
                if (-not $text.Contains("IMM_OPENXR_PROBE destroyInstanceResult=0")) {
                    throw "Meta OpenXR runtime override probe did not destroy the instance cleanly"
                }

                Write-Output "runtimeJson=$metaRuntimeJson"
                $output
            }
    } else {
        Add-Result "Windows Meta OpenXR runtime override probe" "SKIPPED" "" "Meta OpenXR runtime JSON not found"
    }
} else {
    Add-Result "Windows OpenXR runtime probe" "SKIPPED" "" "-SkipWindowsOpenXrProbe"
    Add-Result "Windows Meta OpenXR runtime override probe" "SKIPPED" "" "-SkipWindowsOpenXrProbe"
}

if (-not $SkipHardwareStateProbes) {
    Invoke-Captured `
        -Name "Windows SteamVR HMD state" `
        -OutputName "windows-steamvr-hmd-state.txt" `
        -Command {
            Write-Output "Clock: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')"
            $xrClientLog = "C:\Program Files (x86)\Steam\logs\xrclient_pwsh.txt"
            $vrServerLog = "C:\Program Files (x86)\Steam\logs\vrserver.txt"
            if (Test-Path $xrClientLog) {
                Write-Output "xrclient_pwsh:"
                Select-String -Path $xrClientLog -Pattern "VRInitError|OpenXR|CreateInstance|WirelessHmd|HmdNotFound" |
                    Select-Object -Last 20 |
                    ForEach-Object { $_.Line }
            } else {
                Write-Output "missing=$xrClientLog"
            }
            if (Test-Path $vrServerLog) {
                Write-Output "vrserver:"
                Select-String -Path $vrServerLog -Pattern "VRInitError|WirelessHmd|HmdNotFound|OpenXRInstance|No connected devices|Refusing connect" |
                    Select-Object -Last 30 |
                    ForEach-Object { $_.Line }
            } else {
                Write-Output "missing=$vrServerLog"
            }
            Get-Process |
                Where-Object { $_.ProcessName -match "vrserver|vrmonitor|vrcompositor|OVR|VirtualDesktop" } |
                Select-Object Id, ProcessName |
                Format-Table -AutoSize | Out-String
        } `
        -BlockedPatterns @(
            "WirelessHmdNotConnected",
            "HmdNotFound",
            "No connected devices found"
        )

    $adbStatePath = Resolve-AdbForStatus $Adb
    if (-not $adbStatePath) {
        Add-Result "Android Quest OS state" "BLOCKED" "" "adb not found"
    } else {
        Invoke-Captured `
            -Name "Android Quest OS state" `
            -OutputName "android-quest-os-state.txt" `
            -Command {
                Write-Output "Clock: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')"
                Write-Output "adb=$adbStatePath"
                & $adbStatePath devices
                $powerText = (& $adbStatePath shell dumpsys power) -join [Environment]::NewLine
                $activityText = (& $adbStatePath shell dumpsys activity activities) -join [Environment]::NewLine
                Write-Output "power-summary:"
                $powerText -split "`r?`n" |
                    Where-Object { $_ -match "mWakefulness=|mHalInteractiveModeEnabled=|mStayOn=|mScreenState=|mHoldingDisplaySuspendBlocker=" }
                Write-Output "activity-summary:"
                $activityText -split "`r?`n" |
                    Where-Object { $_ -match "topResumedActivity=|mCurrentFocus=|ResumedActivity|com\.oculus\.os\.vrlockscreen|com\.oculus\.guardian|org\.linuxfoundation\.imm\.player|isSleeping=" } |
                    Select-Object -First 80
                if ($powerText -match "mWakefulness=Asleep" -or $powerText -match "mScreenState=OFF") {
                    Write-Output "QUEST_BLOCKER: display asleep/off"
                }
                if ($activityText -match "com\.oculus\.os\.vrlockscreen" -or $activityText -match "com\.oculus\.guardian") {
                    Write-Output "QUEST_BLOCKER: VR lockscreen or Guardian is active"
                }
            } `
            -BlockedPatterns @(
                "QUEST_BLOCKER",
                "device unauthorized",
                "no devices/emulators found"
            )
    }
} else {
    Add-Result "Windows SteamVR HMD state" "SKIPPED" "" "-SkipHardwareStateProbes"
    Add-Result "Android Quest OS state" "SKIPPED" "" "-SkipHardwareStateProbes"
}

if (-not $SkipAndroidOpenXrComponentProbe) {
    $adbOpenXrPath = Resolve-AdbForStatus $Adb
    if (-not $adbOpenXrPath) {
        Add-Result "Android OpenXR runtime components" "BLOCKED" "" "adb not found"
    } else {
        Invoke-Captured `
            -Name "Android OpenXR runtime components" `
            -OutputName "android-openxr-runtime-components.txt" `
            -Command {
                Write-Output "Clock: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')"
                Write-Output "adb=$adbOpenXrPath"
                $devicesText = (& $adbOpenXrPath devices) -join [Environment]::NewLine
                Write-Output $devicesText
                if ($devicesText -match "unauthorized") {
                    Write-Output "OPENXR_COMPONENT_BLOCKER: device unauthorized"
                    return
                }
                if ($devicesText -notmatch "\bdevice\b") {
                    Write-Output "OPENXR_COMPONENT_BLOCKER: no Android device or emulator"
                    return
                }

                $packages = (& $adbOpenXrPath shell pm list packages) -join [Environment]::NewLine
                $openXrPackages = $packages -split "`r?`n" | Where-Object { $_ -match "openxr|runtimebroker|xr" }
                Write-Output "openxr-packages:"
                $openXrPackages
                if ($packages -notmatch "horizonos\.openxr\.runtimebroker") {
                    throw "Quest OpenXR runtime broker package was not found"
                }

                Write-Output "runtimebroker-package:"
                & $adbOpenXrPath shell dumpsys package horizonos.openxr.runtimebroker |
                    Where-Object { $_ -match "versionCode=|versionName=|primaryCpuAbi=|codePath=|userId=" }

                Write-Output "openxr-service-props:"
                $props = @(
                    "ro.product.manufacturer",
                    "ro.product.model",
                    "ro.build.version.sdk",
                    "init.svc.xrservice",
                    "init.svc.xrspd",
                    "ovr.xrspd.status",
                    "ovr.xrspd.ffs.ready"
                )
                foreach ($prop in $props) {
                    $value = (& $adbOpenXrPath shell getprop $prop) -join ""
                    Write-Output "$prop=$value"
                }

                Write-Output "openxr-processes:"
                & $adbOpenXrPath shell ps -A |
                    Where-Object { $_ -match "xrservice|xrspd|openxr|runtimebroker" }

                Write-Output "androidOpenXrDiscovery=component presence only; no IMM OpenXR app session or frame submit validated"
            } `
            -BlockedPatterns @(
                "OPENXR_COMPONENT_BLOCKER",
                "device unauthorized",
                "no devices/emulators found"
            )
    }
} else {
    Add-Result "Android OpenXR runtime components" "SKIPPED" "" "-SkipAndroidOpenXrComponentProbe"
}

if (-not $SkipAndroidManifestChecks) {
    Invoke-Captured `
        -Name "Android manifest split" `
        -OutputName "android-manifest-split.txt" `
        -Command {
            $nonVrManifestPath = Join-Path $androidDir "appImmViewer\src\nonVr\AndroidManifest.xml"
            $vrManifestPath = Join-Path $androidDir "appImmViewer\src\vr\AndroidManifest.xml"
            if (-not (Test-Path $nonVrManifestPath)) {
                throw "Missing non-VR manifest: $nonVrManifestPath"
            }
            if (-not (Test-Path $vrManifestPath)) {
                throw "Missing VR manifest: $vrManifestPath"
            }

            [xml]$nonVrManifest = Get-Content -LiteralPath $nonVrManifestPath -Raw
            [xml]$vrManifest = Get-Content -LiteralPath $vrManifestPath -Raw
            $androidNs = "http://schemas.android.com/apk/res/android"

            function Get-AndroidAttr($Node, [string]$Name) {
                return $Node.GetAttribute($Name, $androidNs)
            }

            $nonVrText = Get-Content -LiteralPath $nonVrManifestPath -Raw
            $vrText = Get-Content -LiteralPath $vrManifestPath -Raw

            $nonVrForbidden = @(
                "com.oculus.intent.category.VR",
                "com.oculus.vr.application.mode",
                "android.hardware.vr.headtracking",
                "android.hardware.vr.high_performance",
                "android.software.vr.mode"
            )
            foreach ($token in $nonVrForbidden) {
                if ($nonVrText.Contains($token)) {
                    throw "Non-VR manifest contains VR-only token: $token"
                }
            }

            $vrRequired = @(
                "com.oculus.intent.category.VR",
                "com.oculus.vr.application.mode",
                "vr_only",
                "android.hardware.vr.headtracking",
                "android.hardware.vr.high_performance",
                "android.software.vr.mode"
            )
            foreach ($token in $vrRequired) {
                if (-not $vrText.Contains($token)) {
                    throw "VR manifest missing required VR token: $token"
                }
            }

            $nonVrActivity = $nonVrManifest.manifest.application.activity | Select-Object -First 1
            $vrActivity = $vrManifest.manifest.application.activity | Select-Object -First 1
            $nonVrActivityName = Get-AndroidAttr $nonVrActivity "name"
            $vrActivityName = Get-AndroidAttr $vrActivity "name"
            if ($nonVrActivityName -ne "org.linuxfoundation.imm.player.MainActivity") {
                throw "Unexpected non-VR activity name: $nonVrActivityName"
            }
            if ($vrActivityName -ne "org.linuxfoundation.imm.player.MainActivity") {
                throw "Unexpected VR activity name: $vrActivityName"
            }

            Write-Output "nonVrManifest=$nonVrManifestPath"
            Write-Output "vrManifest=$vrManifestPath"
            Write-Output "nonVrActivity=$nonVrActivityName"
            Write-Output "vrActivity=$vrActivityName"
            Write-Output "nonVrForbiddenAbsent=$($nonVrForbidden -join ',')"
            Write-Output "vrRequiredPresent=$($vrRequired -join ',')"
        }
} else {
    Add-Result "Android manifest split" "SKIPPED" "" "-SkipAndroidManifestChecks"
}

if (-not $SkipAndroidBuilds) {
    Invoke-Captured `
        -Name "Android Vulkan non-VR APK build" `
        -OutputName "android-vulkan-build.txt" `
        -Command {
            Push-Location $androidDir
            try {
                & .\gradlew.bat :libImmCore:assembleDebug :libImmImporter:assembleDebug :libImmPlayer:assembleDebug :appImmViewer:assembleDebug `
                    -PimmNonVr=ON `
                    -PimmBuildDir=build_vulkan `
                    -PimmRendererApi=Vulkan
            }
            finally {
                Pop-Location
            }
        }

    Invoke-Captured `
        -Name "Android GLES non-VR fallback APK build" `
        -OutputName "android-gles-build.txt" `
        -Command {
            Push-Location $androidDir
            try {
                & .\gradlew.bat :appImmViewer:assembleDebug `
                    -PimmNonVr=ON `
                    -PimmBuildDir=build_gles_fallback `
                    -PimmRendererApi=GLES
            }
            finally {
                Pop-Location
            }
        }

    Invoke-Captured `
        -Name "Android legacy VR/Oculus APK build" `
        -OutputName "android-legacy-vr-build.txt" `
        -Command {
            Push-Location $androidDir
            try {
                & .\gradlew.bat :appImmViewer:assembleDebug `
                    -PimmNonVr=OFF `
                    -PimmBuildDir=build_vr
            }
            finally {
                Pop-Location
            }
        }

    Invoke-Captured `
        -Name "Android OpenXR probe APK build" `
        -OutputName "android-openxr-probe-build.txt" `
        -Command {
            Push-Location $androidDir
            try {
                & .\gradlew.bat :appImmViewer:assembleDebug `
                    -PimmNonVr=ON `
                    -PimmBuildDir=build_openxr_probe `
                    -PimmRendererApi=Vulkan `
                    -PimmXrRuntime=OpenXR
            }
            finally {
                Pop-Location
            }
        }

    Invoke-Captured `
        -Name "Android OpenXR VR-manifest probe APK build" `
        -OutputName "android-openxr-vrmanifest-probe-build.txt" `
        -Command {
            Push-Location $androidDir
            try {
                & .\gradlew.bat :appImmViewer:assembleDebug `
                    -PimmNonVr=ON `
                    -PimmBuildDir=build_openxr_probe_vrmanifest `
                    -PimmRendererApi=Vulkan `
                    -PimmXrRuntime=OpenXR `
                    -PimmManifest=vr
            }
            finally {
                Pop-Location
            }
        }

    Invoke-Captured `
        -Name "Android OpenXR VR-manifest probe manifest" `
        -OutputName "android-openxr-vrmanifest-probe-manifest.txt" `
        -Command {
            $manifestPath = Join-Path $androidDir "appImmViewer\build_openxr_probe_vrmanifest\intermediates\merged_manifests\debug\processDebugManifest\AndroidManifest.xml"
            $apkPath = Join-Path $androidDir "appImmViewer\build_openxr_probe_vrmanifest\outputs\apk\debug\appImmViewer-debug.apk"
            if (-not (Test-Path $manifestPath)) {
                throw "VR-manifest OpenXR probe merged manifest was not found: $manifestPath"
            }
            if (-not (Test-Path $apkPath)) {
                throw "VR-manifest OpenXR probe APK was not found: $apkPath"
            }
            $manifestText = Get-Content -LiteralPath $manifestPath -Raw
            $requiredTokens = @(
                "com.oculus.intent.category.VR",
                "com.oculus.vr.application.mode",
                "vr_only",
                "android.hardware.vr.headtracking",
                "android.hardware.vr.high_performance",
                "android.software.vr.mode",
                "com.oculus.permission.HAND_TRACKING",
                "oculus.software.handtracking",
                "com.oculus.handtracking.version",
                "com.oculus.handtracking.frequency",
                "android.app.lib_name",
                "org.linuxfoundation.imm.player.MainActivity"
            )
            foreach ($token in $requiredTokens) {
                if (-not $manifestText.Contains($token)) {
                    throw "VR-manifest OpenXR probe merged manifest missing token: $token"
                }
            }

            Write-Output "apk=$apkPath"
            Write-Output "mergedManifest=$manifestPath"
            Write-Output "manifestTokens=$($requiredTokens -join ',')"
        }
} else {
    Add-Result "Android Vulkan non-VR APK build" "SKIPPED" "" "-SkipAndroidBuilds"
    Add-Result "Android GLES non-VR fallback APK build" "SKIPPED" "" "-SkipAndroidBuilds"
    Add-Result "Android legacy VR/Oculus APK build" "SKIPPED" "" "-SkipAndroidBuilds"
    Add-Result "Android OpenXR probe APK build" "SKIPPED" "" "-SkipAndroidBuilds"
    Add-Result "Android OpenXR VR-manifest probe APK build" "SKIPPED" "" "-SkipAndroidBuilds"
    Add-Result "Android OpenXR VR-manifest probe manifest" "SKIPPED" "" "-SkipAndroidBuilds"
}

if (-not $SkipAndroidRuntime) {
    $adbPath = Resolve-AdbForStatus $Adb
    if (-not $adbPath) {
        Add-Result "Android Vulkan runtime smoke" "BLOCKED" "" "adb not found"
        Add-Result "Android GLES runtime smoke" "BLOCKED" "" "adb not found"
        Add-Result "Android OpenXR probe runtime smoke" "BLOCKED" "" "adb not found"
        Add-Result "Android OpenXR VR-manifest probe runtime smoke" "BLOCKED" "" "adb not found"
    } else {
        Invoke-Captured `
            -Name "Android Vulkan runtime smoke" `
            -OutputName "android-vulkan-smoke.txt" `
            -Command {
                Push-Location $androidDir
                try {
                    & .\run-android-vulkan-smoke.ps1 `
                        -Adb $adbPath `
                        -WaitSeconds $AndroidWaitSeconds `
                        -LogDir "logs/android-vulkan-validation-status-$safeTimestamp" `
                        -SkipBuild:$SkipBuild
                }
                finally {
                    Pop-Location
                }
            } `
            -BlockedPatterns @(
                "Quest OS focus state",
                "Quest display is asleep/off",
                "No Android device or emulator",
                "APK not found"
            )

        Invoke-Captured `
            -Name "Android GLES runtime smoke" `
            -OutputName "android-gles-smoke.txt" `
            -Command {
                Push-Location $androidDir
                try {
                    & .\run-android-gles-smoke.ps1 `
                        -Adb $adbPath `
                        -WaitSeconds $AndroidWaitSeconds `
                        -LogDir "logs/android-gles-validation-status-$safeTimestamp" `
                        -SkipBuild:$SkipBuild
                }
                finally {
                    Pop-Location
                }
            } `
            -BlockedPatterns @(
                "Quest OS focus state",
                "Quest display is asleep/off",
                "No Android device or emulator",
                "APK not found"
            )

        Invoke-Captured `
            -Name "Android OpenXR probe runtime smoke" `
            -OutputName "android-openxr-probe-smoke.txt" `
            -Command {
                Push-Location $androidDir
                try {
                    & .\run-android-openxr-probe-smoke.ps1 `
                        -Adb $adbPath `
                        -WaitSeconds $AndroidWaitSeconds `
                        -LogDir "logs/android-openxr-probe-validation-status-$safeTimestamp" `
                        -SkipBuild:$SkipBuild
                }
                finally {
                    Pop-Location
                }
            } `
            -BlockedPatterns @(
                "Quest OS focus state",
                "reprojected Quest OS dialog",
                "Quest display is asleep/off",
                "controller-required",
                "No Android device or emulator",
                "APK not found"
            )

        Invoke-Captured `
            -Name "Android OpenXR VR-manifest probe runtime smoke" `
            -OutputName "android-openxr-vrmanifest-probe-smoke.txt" `
            -Command {
                Push-Location $androidDir
                try {
                    & .\run-android-openxr-probe-smoke.ps1 `
                        -Adb $adbPath `
                        -WaitSeconds $AndroidWaitSeconds `
                        -LogDir "logs/android-openxr-vrmanifest-probe-validation-status-$safeTimestamp" `
                        -VrManifest `
                        -AttemptQuestUnblock `
                        -SkipBuild:$SkipBuild
                }
                finally {
                    Pop-Location
                }
            } `
            -BlockedPatterns @(
                "Quest OS focus state",
                "reprojected Quest OS dialog",
                "Quest display is asleep/off",
                "controller-required",
                "No Android device or emulator",
                "APK not found"
            )
    }
} else {
    Add-Result "Android Vulkan runtime smoke" "SKIPPED" "" "-SkipAndroidRuntime"
    Add-Result "Android GLES runtime smoke" "SKIPPED" "" "-SkipAndroidRuntime"
    Add-Result "Android OpenXR probe runtime smoke" "SKIPPED" "" "-SkipAndroidRuntime"
    Add-Result "Android OpenXR VR-manifest probe runtime smoke" "SKIPPED" "" "-SkipAndroidRuntime"
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("IMM Vulkan/OpenXR validation status") | Out-Null
$lines.Add("Timestamp: $timestamp") | Out-Null
$lines.Add("Repository: $repoRoot") | Out-Null
$lines.Add("") | Out-Null
foreach ($result in $results) {
    $line = "{0}: {1}" -f $result.Status, $result.Name
    if ($result.Evidence) {
        $line += " [$($result.Evidence)]"
    }
    if ($result.Notes) {
        $line += " - $($result.Notes)"
    }
    $lines.Add($line) | Out-Null
}

$lines | Out-File -FilePath $reportPath -Encoding utf8
$lines | ForEach-Object { Write-Host $_ }
Write-Host ""
Write-Host "Validation report: $reportPath"

$hardFailures = @($results | Where-Object { $_.Status -eq "FAIL" })
if ($hardFailures.Count -gt 0) {
    exit 1
}
