#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include "libImmCore/src/libRender/metal/piMetal_Renderer.h"
#include "libImmCore/src/libBasics/piImage.h"
#include "libImmCore/src/libBasics/piTimer.h"
#include "libImmCore/src/libSound/piSound.h"
#include "appImmViewer/src/settings.h"
#include "appImmViewer/src/resolve.h"
#include "appImmViewer/src/viewer/viewer.h"
#include "libImmPlayer/src/blue_noise.h"

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <wchar.h>
#include <math.h>
#include <dirent.h>

using namespace ImmCore;

static wchar_t gSettingsPath[PATH_MAX] = L"code/appImmViewer/exe/settings.json";
static wchar_t gContentPath[PATH_MAX] = {};
static int gContentPathCount = 0;
static int gExitCode = 0;
static const NSInteger kOpenRecentMenuItemTag = 43001;
static NSString *const kRecentImmFilesDefaultsKey = @"IMMRecentDocumentURLs";

static bool iSetSettingsPath(const char *path)
{
    if (!path || !path[0])
    {
        return true;
    }
    const size_t len = mbstowcs(gSettingsPath, path, PATH_MAX - 1);
    if (len == (size_t)-1)
    {
        return false;
    }
    gSettingsPath[len] = 0;
    return true;
}

static void iUseBundledSettingsIfAvailable(void)
{
    NSString *resourcePath = [[NSBundle mainBundle] pathForResource:@"appImmViewerMetal-settings" ofType:@"json"];
    if (!resourcePath)
    {
        return;
    }
    const char *path = [resourcePath fileSystemRepresentation];
    if (path && path[0])
    {
        iSetSettingsPath(path);
    }
}

static bool iAddContentPath(const char *path)
{
    if (!path || !path[0])
    {
        return true;
    }
    if (gContentPathCount >= 1)
    {
        return false;
    }
    const size_t len = mbstowcs(gContentPath, path, PATH_MAX - 1);
    if (len == (size_t)-1)
    {
        return false;
    }
    gContentPath[len] = 0;
    ++gContentPathCount;
    return true;
}

static bool iHasExtension(const char *path, const char *extension)
{
    const char *dot = path ? strrchr(path, '.') : nullptr;
    return dot && strcasecmp(dot, extension) == 0;
}

static bool iIsValidationRequested(void)
{
    const char *frame = getenv("IMM_METAL_VALIDATE_FRAME");
    return frame && frame[0];
}

static bool iSetSingleLoadedFile(ExePlayer::Settings *settings, const wchar_t *path);

static NSString *iFirstImmFileInDirectory(NSString *directory)
{
    if (!directory)
    {
        return nil;
    }
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSArray<NSString *> *entries = [fileManager contentsOfDirectoryAtPath:directory error:nil];
    NSArray<NSString *> *sortedEntries = [entries sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
    for (NSString *entry in sortedEntries)
    {
        if ([[entry pathExtension] caseInsensitiveCompare:@"imm"] != NSOrderedSame)
        {
            continue;
        }
        NSString *candidate = [directory stringByAppendingPathComponent:entry];
        BOOL isDirectory = NO;
        if ([fileManager fileExistsAtPath:candidate isDirectory:&isDirectory] && !isDirectory)
        {
            return candidate;
        }
    }
    return nil;
}

static bool iApplyDefaultImmWhenNoLoadConfigured(ExePlayer::Settings *settings)
{
    if (!settings || settings->mFiles.mLoad.GetLength() > 0)
    {
        return true;
    }

    NSString *candidate = iFirstImmFileInDirectory([[NSBundle mainBundle] resourcePath]);
    if (!candidate)
    {
        NSString *executableDirectory = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
        candidate = iFirstImmFileInDirectory(executableDirectory);
    }
    if (!candidate)
    {
        return true;
    }

    const char *path = [candidate fileSystemRepresentation];
    wchar_t widePath[PATH_MAX] = {};
    const size_t len = mbstowcs(widePath, path, PATH_MAX - 1);
    if (len == (size_t)-1)
    {
        return false;
    }
    widePath[len] = 0;
    NSLog(@"IMM Metal player default document: %@", candidate);
    return iSetSingleLoadedFile(settings, widePath);
}

static bool iOverrideLoadedFiles(ExePlayer::Settings *settings)
{
    if (!settings || gContentPathCount == 0)
    {
        return true;
    }
    return iSetSingleLoadedFile(settings, gContentPath);
}

static bool iSetSingleLoadedFile(ExePlayer::Settings *settings, const wchar_t *path)
{
    if (!settings || !path || !path[0])
    {
        return false;
    }
    for (ImmCore::piString &load : settings->mFiles.mLoad)
    {
        load.End();
    }
    settings->mFiles.mLoad.SetLength(0);

    ImmCore::piString *fileToLoad = settings->mFiles.mLoad.GetAddress(0);
    new (fileToLoad) ImmCore::piString();
    settings->mFiles.mLoad.SetLength(1);
    return fileToLoad && fileToLoad->InitCopyW(path);
}

static NSString *iWindowTitleForSettings(const ExePlayer::Settings &settings)
{
    if (settings.mFiles.mLoad.GetLength() < 1)
    {
        return @"IMM Metal Standalone Player";
    }

    const ImmCore::piString &path = settings.mFiles.mLoad[0];
    const wchar_t *pathW = path.GetS();
    if (!pathW || !pathW[0])
    {
        return @"IMM Metal Standalone Player";
    }

    char pathUtf8[PATH_MAX] = {};
    const size_t pathLen = wcstombs(pathUtf8, pathW, sizeof(pathUtf8) - 1);
    if (pathLen == (size_t)-1)
    {
        return @"IMM Metal Standalone Player";
    }
    pathUtf8[pathLen] = 0;
    NSString *pathString = [NSString stringWithUTF8String:pathUtf8];
    if (!pathString)
    {
        return @"IMM Metal Standalone Player";
    }
    NSString *fileName = [pathString lastPathComponent];
    if (![fileName length])
    {
        return @"IMM Metal Standalone Player";
    }
    return [NSString stringWithFormat:@"%@ - IMM Metal Player", fileName];
}

static float iClamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static void iInstallApplicationMenu(void)
{
    NSString *appName = [[NSProcessInfo processInfo] processName];
    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@""];
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:appMenuItem];

    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:appName];
    NSString *aboutTitle = [NSString stringWithFormat:@"About %@", appName];
    [appMenu addItemWithTitle:aboutTitle action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];

    NSMenu *servicesMenu = [[NSMenu alloc] initWithTitle:@"Services"];
    NSMenuItem *servicesItem = [[NSMenuItem alloc] initWithTitle:@"Services" action:nil keyEquivalent:@""];
    [servicesItem setSubmenu:servicesMenu];
    [appMenu addItem:servicesItem];
    [NSApp setServicesMenu:servicesMenu];
    [appMenu addItem:[NSMenuItem separatorItem]];

    NSString *hideTitle = [NSString stringWithFormat:@"Hide %@", appName];
    [appMenu addItemWithTitle:hideTitle action:@selector(hide:) keyEquivalent:@"h"];
    NSMenuItem *hideOthers = [appMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    [hideOthers setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
    [appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];

    NSString *quitTitle = [NSString stringWithFormat:@"Quit %@", appName];
    [appMenu addItemWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];
    [appMenuItem setSubmenu:appMenu];

    NSMenuItem *fileMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:fileMenuItem];
    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    [fileMenu addItemWithTitle:@"Open..." action:@selector(openDocument:) keyEquivalent:@"o"];
    NSMenuItem *openRecentItem = [[NSMenuItem alloc] initWithTitle:@"Open Recent" action:nil keyEquivalent:@""];
    [openRecentItem setTag:kOpenRecentMenuItemTag];
    NSMenu *openRecentMenu = [[NSMenu alloc] initWithTitle:@"Open Recent"];
    [openRecentItem setSubmenu:openRecentMenu];
    [fileMenu addItem:openRecentItem];
    [fileMenuItem setSubmenu:fileMenu];

    NSMenuItem *playbackMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:playbackMenuItem];
    NSMenu *playbackMenu = [[NSMenu alloc] initWithTitle:@"Playback"];
    [playbackMenu addItemWithTitle:@"Play/Pause" action:@selector(togglePlayback:) keyEquivalent:@" "];
    [playbackMenu addItemWithTitle:@"Restart" action:@selector(restartPlayback:) keyEquivalent:@"c"];
    [playbackMenu addItem:[NSMenuItem separatorItem]];
    [playbackMenu addItemWithTitle:@"Previous" action:@selector(previousChapter:) keyEquivalent:@"z"];
    [playbackMenu addItemWithTitle:@"Next" action:@selector(nextChapter:) keyEquivalent:@"x"];
    [playbackMenuItem setSubmenu:playbackMenu];

    NSMenuItem *audioMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:audioMenuItem];
    NSMenu *audioMenu = [[NSMenu alloc] initWithTitle:@"Audio"];
    [audioMenu addItemWithTitle:@"Mute" action:@selector(toggleMute:) keyEquivalent:@"m"];
    [audioMenu addItemWithTitle:@"Volume Up" action:@selector(volumeUp:) keyEquivalent:@"="];
    [audioMenu addItemWithTitle:@"Volume Down" action:@selector(volumeDown:) keyEquivalent:@"-"];
    [audioMenuItem setSubmenu:audioMenu];

    [NSApp setMainMenu:mainMenu];
}

static NSMenuItem *iFindMenuItemWithTag(NSMenu *menu, NSInteger tag)
{
    for (NSMenuItem *item in [menu itemArray])
    {
        if ([item tag] == tag)
        {
            return item;
        }
        NSMenuItem *subitem = iFindMenuItemWithTag([item submenu], tag);
        if (subitem)
        {
            return subitem;
        }
    }
    return nil;
}

static float iDecodeUnsignedFloat(uint32_t bits, int mantissaBits)
{
    const uint32_t mantissaMask = (1u << mantissaBits) - 1u;
    const uint32_t mantissa = bits & mantissaMask;
    const uint32_t exponent = (bits >> mantissaBits) & 0x1fu;
    if (exponent == 0)
    {
        return ldexpf((float)mantissa / (float)(1u << mantissaBits), -14);
    }
    if (exponent == 31)
    {
        return 1.0f;
    }
    return ldexpf(1.0f + (float)mantissa / (float)(1u << mantissaBits), (int)exponent - 15);
}

static uint8_t iFloatToByte(float value)
{
    if (value <= 0.0f)
    {
        return 0;
    }
    if (value >= 1.0f)
    {
        return 255;
    }
    return (uint8_t)(value * 255.0f + 0.5f);
}

static bool iWriteRG11B10PPM(const char *path, const uint32_t *pixels, int width, int height)
{
    if (!path || !path[0] || !pixels || width <= 0 || height <= 0)
    {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file)
    {
        return false;
    }

    fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (int y = 0; y < height; ++y)
    {
        const uint32_t *row = pixels + (size_t)y * (size_t)width;
        for (int x = 0; x < width; ++x)
        {
            const uint32_t pixel = row[x];
            const float red = iDecodeUnsignedFloat(pixel & 0x7ffu, 6);
            const float green = iDecodeUnsignedFloat((pixel >> 11) & 0x7ffu, 6);
            const float blue = iDecodeUnsignedFloat((pixel >> 22) & 0x3ffu, 5);
            const uint8_t out[3] = { iFloatToByte(red), iFloatToByte(green), iFloatToByte(blue) };
            if (fwrite(out, 1, sizeof(out), file) != sizeof(out))
            {
                fclose(file);
                return false;
            }
        }
    }

    return fclose(file) == 0;
}

static void iDecodeRG11B10Pixels(uint8_t *dstRGB, const uint32_t *pixels, int width, int height)
{
    for (int y = 0; y < height; ++y)
    {
        const uint32_t *srcRow = pixels + (size_t)y * (size_t)width;
        uint8_t *dstRow = dstRGB + (size_t)y * (size_t)width * 3u;
        for (int x = 0; x < width; ++x)
        {
            const uint32_t pixel = srcRow[x];
            dstRow[3 * x + 0] = iFloatToByte(iDecodeUnsignedFloat(pixel & 0x7ffu, 6));
            dstRow[3 * x + 1] = iFloatToByte(iDecodeUnsignedFloat((pixel >> 11u) & 0x7ffu, 6));
            dstRow[3 * x + 2] = iFloatToByte(iDecodeUnsignedFloat((pixel >> 22u) & 0x3ffu, 5));
        }
    }
}

static bool iWriteRG11B10PNG(const char *path, const uint32_t *pixels, int width, int height)
{
    if (!path || !path[0] || !pixels || width <= 0 || height <= 0)
    {
        return false;
    }

    uint8_t *rgb = (uint8_t *)malloc((size_t)width * (size_t)height * 3u);
    if (!rgb)
    {
        return false;
    }
    iDecodeRG11B10Pixels(rgb, pixels, width, height);

    wchar_t widePath[PATH_MAX] = {};
    const size_t widePathLen = mbstowcs(widePath, path, PATH_MAX - 1);
    if (widePathLen == (size_t)-1)
    {
        free(rgb);
        return false;
    }
    widePath[widePathLen] = 0;

    piImage image;
    image.InitWrap(piImage::TYPE_2D, width, height, 1, piImage::FORMAT_I_RGB, rgb);
    const bool ok = image.WriteToDisk(widePath, 0, L"png");
    image.Free();
    free(rgb);
    return ok;
}

static bool iWriteRG11B10Capture(const char *path, const uint32_t *pixels, int width, int height)
{
    if (iHasExtension(path, ".png"))
    {
        return iWriteRG11B10PNG(path, pixels, width, height);
    }
    return iWriteRG11B10PPM(path, pixels, width, height);
}

class ImmMetalReporter final : public ImmCore::piRenderer::piReporter
{
public:
    void Info(const char *str) override { NSLog(@"IMM Metal: %s", str); }
    void Error(const char *str, int) override { NSLog(@"IMM Metal error: %s", str); }
    void Begin(uint64_t, uint64_t, int, int) override {}
    void Texture(const wchar_t *, uint64_t, ImmCore::piRenderer::Format, bool, int, int, int) override {}
    void End(void) override {}
};

@interface ImmMetalPlayerDelegate : NSObject <NSApplicationDelegate, MTKViewDelegate>
{
    NSWindow *_window;
    MTKView *_view;
    ImmCore::piWindowEvents _events;
    ImmCore::piRendererMetal *_renderer;
    ImmCore::piShader _shader;
    ImmCore::piShader _pipelineSanityShader;
    ImmCore::piShader _smokePictureShader;
    ImmCore::piShader _smokeCubemapShader;
    ImmCore::piBuffer _vertexBuffer;
    ImmCore::piBuffer _pipelineSanityIndexBuffer;
    ImmCore::piBuffer _smokeFrameBuffer;
    ImmCore::piBuffer _smokeLayerBuffer;
    ImmCore::piBuffer _smokeDisplayBuffer;
    ImmCore::piVertexArray _pipelineSanityIndexedVertexArray;
    ImmCore::piVertexArray _vertexArray;
    ImmCore::piTexture _smokePictureTexture;
    ImmCore::piTexture _smokeCubemapTexture;
    ImmCore::piTexture _smokeBlueNoiseTexture;
    ImmCore::piSampler _smokePictureSampler;
    ImmMetalReporter _reporter;
    ImmCore::piLog _log;
    ImmCore::piTimer _timer;
    ExePlayer::Settings _settings;
    ExePlayer::Viewer _viewer;
    ExePlayer::Resolve _resolve;
    ImmCore::piSoundEngineBackend *_soundBackend;
    ImmCore::piTexture _colorTexture;
    ImmCore::piTexture _depthTexture;
    ImmCore::piRTarget _renderTarget;
    ImmCore::ivec2 _renderSize;
    double _timeBase;
    double _lastTime;
    double _smokeExitAfterSeconds;
    uint64_t _frameIndex;
    int _nativeFrameFailureCount;
    uint64_t _validationFrame;
    uint64_t _validationMaxFrame;
    uint64_t _validationMinNonZeroPixels;
    uint64_t _validationMinDrawCalls;
    uint64_t _validationMinPictureDrawCalls;
    uint64_t _validationMinPicture2DDrawCalls;
    uint64_t _validationMinPicture360DrawCalls;
    uint64_t _validationMinPicture360EquirectDrawCalls;
    uint64_t _validationMinPicture360CubemapDrawCalls;
    uint64_t _validationMinTriangles;
    uint64_t _validationResizeFrame;
    uint64_t _validationReloadFrame;
    int _validationResizeWidth;
    int _validationResizeHeight;
    char _validationCapturePath[PATH_MAX];
    char _validationReloadPath[PATH_MAX];
    bool _viewerReady;
    bool _firstFrame;
    bool _validationEnabled;
    bool _validationDone;
    bool _exitAfterValidation;
    bool _validationResizeDone;
    bool _validationReloadDone;
    bool _validationReloadPending;
    bool _validationHelperDraws;
    bool _validationHelperDrawsDone;
    bool _validationPipelineSanityDone;
    bool _validationForceNativeFrameFailure;
    bool _didCleanup;
    bool _isStopping;
    bool _muted;
    bool _volumeSmokeDone;
    bool _recentDocumentSmokeDone;
    bool _playbackControlSmokeDone;
    bool _openFailureRestoreSmokeDone;
    float _documentVolume;
    float _volumeBeforeMute;
}
- (void)handleKeyEvent:(NSEvent *)event down:(BOOL)down;
- (void)handleFlagsChanged:(NSEvent *)event;
- (void)handleMouseEvent:(NSEvent *)event button:(int)button down:(BOOL)down dragged:(BOOL)dragged;
- (void)handleScrollEvent:(NSEvent *)event;
- (CGSize)initialDrawableSizeForView:(MTKView *)view fallbackFrame:(NSRect)frame;
- (void)handleNativeFrameFailure:(const char *)reason;
- (BOOL)runValidationPipelineSanity;
- (BOOL)runValidationShaderPathSanity;
- (IBAction)openDocument:(id)sender;
- (IBAction)openRecentDocumentFromMenu:(id)sender;
- (IBAction)clearRecentDocumentsFromMenu:(id)sender;
- (IBAction)togglePlayback:(id)sender;
- (IBAction)restartPlayback:(id)sender;
- (IBAction)previousChapter:(id)sender;
- (IBAction)nextChapter:(id)sender;
- (IBAction)toggleMute:(id)sender;
- (IBAction)volumeUp:(id)sender;
- (IBAction)volumeDown:(id)sender;
- (BOOL)loadContentFile:(NSString *)filename;
- (BOOL)loadContentFile:(NSString *)filename allowValidationReload:(BOOL)allowValidationReload;
- (BOOL)initializeSoundBackend;
- (void)presentOpenFailureForPath:(NSString *)path message:(NSString *)message;
- (void)setDocumentVolume:(float)volume muted:(BOOL)muted reason:(NSString *)reason;
- (void)runVolumeControlSmokeIfRequested;
- (void)runPlaybackControlSmokeIfRequested;
- (void)runOpenFailureRestoreSmokeIfRequested;
- (void)runRecentDocumentSmokeIfRequested;
- (void)updateRecentDocumentsMenu;
- (NSArray<NSURL *> *)recentImmDocumentURLs;
- (void)noteCurrentDocumentAsRecentWithReason:(NSString *)reason;
- (void)applyValidationReloadIfRequested;
- (void)validationReloadContent;
- (void)performCleanup;
- (void)terminateWithExitCode:(int)exitCode;
- (void)validationDrawTick;
- (void)interactiveSmokeDrawTick;
@end

@interface ImmMetalView : MTKView
@property(nonatomic, assign) ImmMetalPlayerDelegate *eventSink;
@end

@implementation ImmMetalView

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [[self window] makeFirstResponder:self];
    [self registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
    NSPasteboard *pasteboard = [sender draggingPasteboard];
    NSArray<NSURL *> *urls = [pasteboard readObjectsForClasses:@[[NSURL class]]
                                                       options:@{NSPasteboardURLReadingFileURLsOnlyKey: @YES}];
    for (NSURL *url in urls)
    {
        if ([[[url pathExtension] lowercaseString] isEqualToString:@"imm"])
        {
            return NSDragOperationCopy;
        }
    }
    return NSDragOperationNone;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    NSPasteboard *pasteboard = [sender draggingPasteboard];
    NSArray<NSURL *> *urls = [pasteboard readObjectsForClasses:@[[NSURL class]]
                                                       options:@{NSPasteboardURLReadingFileURLsOnlyKey: @YES}];
    for (NSURL *url in urls)
    {
        if ([[[url pathExtension] lowercaseString] isEqualToString:@"imm"])
        {
            return [_eventSink loadContentFile:[url path]];
        }
    }
    return NO;
}

- (void)keyDown:(NSEvent *)event
{
    [_eventSink handleKeyEvent:event down:YES];
}

- (void)keyUp:(NSEvent *)event
{
    [_eventSink handleKeyEvent:event down:NO];
}

- (void)flagsChanged:(NSEvent *)event
{
    [_eventSink handleFlagsChanged:event];
}

- (void)mouseDown:(NSEvent *)event
{
    [_eventSink handleMouseEvent:event button:0 down:YES dragged:NO];
}

- (void)mouseUp:(NSEvent *)event
{
    [_eventSink handleMouseEvent:event button:0 down:NO dragged:NO];
}

- (void)rightMouseDown:(NSEvent *)event
{
    [_eventSink handleMouseEvent:event button:1 down:YES dragged:NO];
}

- (void)rightMouseUp:(NSEvent *)event
{
    [_eventSink handleMouseEvent:event button:1 down:NO dragged:NO];
}

- (void)mouseDragged:(NSEvent *)event
{
    [_eventSink handleMouseEvent:event button:0 down:YES dragged:YES];
}

- (void)rightMouseDragged:(NSEvent *)event
{
    [_eventSink handleMouseEvent:event button:1 down:YES dragged:YES];
}

- (void)scrollWheel:(NSEvent *)event
{
    [_eventSink handleScrollEvent:event];
}

@end

struct ImmMetalDebugVertex
{
    float position[2];
    float color[4];
};

struct ImmMetalSmokeFrameState
{
    float mTime;
    int mFrame;
    int mDummy1;
    int mDummy2;
};

struct ImmMetalSmokeLayerState
{
    float mLayerToViewer[16];
    float mLayerToViewerScale;
    float mOpacity;
    float mFlipSign;
    float mDrawInTime;
    float mAnimParams[4];
    float mKeepAlive[8];
    uint32_t mID;
};

struct ImmMetalSmokeDisplayState
{
    struct
    {
        float mViewerToEyePrj[16];
    } mEye[2];
    float mResolution[2];
    uint32_t mEyeIndex;
};

static void iSetIdentity4x4(float *matrix)
{
    memset(matrix, 0, sizeof(float) * 16);
    matrix[0] = 1.0f;
    matrix[5] = 1.0f;
    matrix[10] = 1.0f;
    matrix[15] = 1.0f;
}

static uint64_t iCountNonZeroPixels(const uint32_t *pixels, size_t pixelCount)
{
    uint64_t nonZeroPixels = 0;
    for (size_t i = 0; i < pixelCount; ++i)
    {
        if (pixels[i] != 0)
        {
            ++nonZeroPixels;
        }
    }
    return nonZeroPixels;
}

@implementation ImmMetalPlayerDelegate

- (int)keyCodeForEvent:(NSEvent *)event
{
    NSString *chars = [event charactersIgnoringModifiers];
    if ([chars length] > 0)
    {
        unichar ch = [chars characterAtIndex:0];
        if (ch < 128)
        {
            if (ch >= 'a' && ch <= 'z')
            {
                ch = (unichar)toupper((int)ch);
            }
            if (ch >= 0 && ch < 256)
            {
                return (int)ch;
            }
        }
    }

    switch ([event keyCode])
    {
        case 36: return KEY_ENTER;
        case 48: return KEY_TAB;
        case 49: return KEY_SPACE;
        case 51: return KEY_BACK;
        case 123: return KEY_LEFT;
        case 124: return KEY_RIGHT;
        case 125: return KEY_DOWN;
        case 126: return KEY_UP;
        default: return -1;
    }
}

- (void)appendKeyToQueue:(int)key
{
    if (_events.keyb.queueLen < 1024)
    {
        _events.keyb.queue[_events.keyb.queueLen++] = key;
    }
}

- (void)handleKeyEvent:(NSEvent *)event down:(BOOL)down
{
    const int key = [self keyCodeForEvent:event];
    if (down && !_validationEnabled)
    {
        if (key == 'M')
        {
            [self toggleMute:nil];
            [self handleFlagsChanged:event];
            return;
        }
        if (key == KEY_SPACE)
        {
            [self togglePlayback:nil];
            [self handleFlagsChanged:event];
            return;
        }
        if (key == '-' || key == '_')
        {
            [self volumeDown:nil];
            [self handleFlagsChanged:event];
            return;
        }
        if (key == '=' || key == '+')
        {
            [self volumeUp:nil];
            [self handleFlagsChanged:event];
            return;
        }
    }
    if (key >= 0 && key < 256)
    {
        _events.keyb.state[key] = down ? 1 : 0;
        if (down)
        {
            _events.keyb.key[key] = 1;
            [self appendKeyToQueue:key];
        }
    }
    [self handleFlagsChanged:event];
}

- (void)handleFlagsChanged:(NSEvent *)event
{
    const NSEventModifierFlags flags = [event modifierFlags];
    const bool shift = (flags & NSEventModifierFlagShift) != 0;
    const bool control = (flags & NSEventModifierFlagControl) != 0;
    const bool option = (flags & NSEventModifierFlagOption) != 0;

    _events.keyb.state[KEY_SHIFT] = shift ? 1 : 0;
    _events.keyb.state[KEY_LSHIFT] = shift ? 1 : 0;
    _events.keyb.state[KEY_RSHIFT] = shift ? 1 : 0;
    _events.keyb.state[KEY_CONTROL] = control ? 1 : 0;
    _events.keyb.state[KEY_LCONTROL] = control ? 1 : 0;
    _events.keyb.state[KEY_RCONTROL] = control ? 1 : 0;
    _events.keyb.state[KEY_ALT] = option ? 1 : 0;
    _events.keyb.state[KEY_LALT] = option ? 1 : 0;
    _events.keyb.state[KEY_RALT] = option ? 1 : 0;
}

- (void)updateMousePositionFromEvent:(NSEvent *)event
{
    NSPoint point = [_view convertPoint:[event locationInWindow] fromView:nil];
    const int x = (int)point.x;
    const int y = (int)(_view.bounds.size.height - point.y);

    _events.mouse.ox = _events.mouse.x;
    _events.mouse.oy = _events.mouse.y;
    _events.mouse.x = x;
    _events.mouse.y = y;
    _events.mouse.dx += x - _events.mouse.ox;
    _events.mouse.dy += y - _events.mouse.oy;
}

- (void)handleMouseEvent:(NSEvent *)event button:(int)button down:(BOOL)down dragged:(BOOL)dragged
{
    (void)dragged;
    [self updateMousePositionFromEvent:event];
    if (button == 0)
    {
        _events.mouse.lb_isDown = down ? 1 : 0;
    }
    else if (button == 1)
    {
        _events.mouse.rb_isDown = down ? 1 : 0;
    }
}

- (void)handleScrollEvent:(NSEvent *)event
{
    [self updateMousePositionFromEvent:event];
    const int delta = (int)[event scrollingDeltaY];
    _events.mouse.dz += delta;
    _events.mouse.wheel += delta;
}

- (void)clearTransientInput
{
    memset(_events.keyb.key, 0, sizeof(_events.keyb.key));
    _events.keyb.queueLen = 0;
    _events.mouse.dx = 0;
    _events.mouse.dy = 0;
    _events.mouse.dz = 0;
    _events.mouse.wheel = 0;
}

- (void)terminateWithExitCode:(int)exitCode
{
    if (exitCode != 0 && gExitCode == 0)
    {
        gExitCode = exitCode;
    }
    if (_isStopping)
    {
        return;
    }
    _isStopping = true;
    [self performCleanup];
    [NSApp stop:nil];
    NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSZeroPoint
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0];
    [NSApp postEvent:event atStart:NO];
}

- (void)configureValidationFromEnvironment
{
    const char *frame = getenv("IMM_METAL_VALIDATE_FRAME");
    _validationEnabled = frame && frame[0];
    _validationFrame = _validationEnabled ? strtoull(frame, nullptr, 10) : 0;
    _validationMaxFrame = _validationFrame + 300;
    _validationMinNonZeroPixels = 16;
    _validationMinDrawCalls = 0;
    _validationMinPictureDrawCalls = 0;
    _validationMinPicture2DDrawCalls = 0;
    _validationMinPicture360DrawCalls = 0;
    _validationMinPicture360EquirectDrawCalls = 0;
    _validationMinPicture360CubemapDrawCalls = 0;
    _validationMinTriangles = 0;
    _validationResizeFrame = 0;
    _validationReloadFrame = 0;
    _validationResizeWidth = 0;
    _validationResizeHeight = 0;
    _validationCapturePath[0] = 0;
    _validationReloadPath[0] = 0;
    _exitAfterValidation = false;
    _validationDone = false;
    _validationResizeDone = false;
    _validationReloadDone = false;
    _validationReloadPending = false;
    const char *helperDraws = getenv("IMM_METAL_VALIDATE_HELPER_DRAWS");
    _validationHelperDraws = helperDraws && helperDraws[0] && strcmp(helperDraws, "0") != 0;
    _validationHelperDrawsDone = false;
    _validationPipelineSanityDone = false;
    const char *forceNativeFrameFailure = getenv("IMM_METAL_VALIDATE_FORCE_NATIVE_FRAME_FAILURE");
    _validationForceNativeFrameFailure = forceNativeFrameFailure &&
                                         forceNativeFrameFailure[0] &&
                                         strcmp(forceNativeFrameFailure, "0") != 0;
    _smokeExitAfterSeconds = 0.0;
    const char *smokeExitAfterSeconds = getenv("IMM_METAL_INTERACTIVE_SMOKE_EXIT_AFTER_SECONDS");
    if (smokeExitAfterSeconds && smokeExitAfterSeconds[0])
    {
        _smokeExitAfterSeconds = strtod(smokeExitAfterSeconds, nullptr);
        if (_smokeExitAfterSeconds < 0.0)
        {
            _smokeExitAfterSeconds = 0.0;
        }
    }

    const char *threshold = getenv("IMM_METAL_VALIDATE_MIN_NONZERO");
    if (threshold && threshold[0])
    {
        _validationMinNonZeroPixels = strtoull(threshold, nullptr, 10);
    }
    const char *maxFrame = getenv("IMM_METAL_VALIDATE_MAX_FRAME");
    if (maxFrame && maxFrame[0])
    {
        _validationMaxFrame = strtoull(maxFrame, nullptr, 10);
    }
    const char *minDrawCalls = getenv("IMM_METAL_VALIDATE_MIN_DRAWCALLS");
    if (minDrawCalls && minDrawCalls[0])
    {
        _validationMinDrawCalls = strtoull(minDrawCalls, nullptr, 10);
    }
    const char *minPictureDrawCalls = getenv("IMM_METAL_VALIDATE_MIN_PICTURE_DRAWCALLS");
    if (minPictureDrawCalls && minPictureDrawCalls[0])
    {
        _validationMinPictureDrawCalls = strtoull(minPictureDrawCalls, nullptr, 10);
    }
    const char *minPicture2DDrawCalls = getenv("IMM_METAL_VALIDATE_MIN_PICTURE2D_DRAWCALLS");
    if (minPicture2DDrawCalls && minPicture2DDrawCalls[0])
    {
        _validationMinPicture2DDrawCalls = strtoull(minPicture2DDrawCalls, nullptr, 10);
    }
    const char *minPicture360DrawCalls = getenv("IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS");
    if (minPicture360DrawCalls && minPicture360DrawCalls[0])
    {
        _validationMinPicture360DrawCalls = strtoull(minPicture360DrawCalls, nullptr, 10);
    }
    const char *minPicture360EquirectDrawCalls = getenv("IMM_METAL_VALIDATE_MIN_PICTURE360_EQUIRECT_DRAWCALLS");
    if (minPicture360EquirectDrawCalls && minPicture360EquirectDrawCalls[0])
    {
        _validationMinPicture360EquirectDrawCalls = strtoull(minPicture360EquirectDrawCalls, nullptr, 10);
    }
    const char *minPicture360CubemapDrawCalls = getenv("IMM_METAL_VALIDATE_MIN_PICTURE360_CUBEMAP_DRAWCALLS");
    if (minPicture360CubemapDrawCalls && minPicture360CubemapDrawCalls[0])
    {
        _validationMinPicture360CubemapDrawCalls = strtoull(minPicture360CubemapDrawCalls, nullptr, 10);
    }
    const char *minTriangles = getenv("IMM_METAL_VALIDATE_MIN_TRIANGLES");
    if (minTriangles && minTriangles[0])
    {
        _validationMinTriangles = strtoull(minTriangles, nullptr, 10);
    }
    const char *resizeFrame = getenv("IMM_METAL_VALIDATE_RESIZE_FRAME");
    if (resizeFrame && resizeFrame[0])
    {
        _validationResizeFrame = strtoull(resizeFrame, nullptr, 10);
    }
    const char *reloadFrame = getenv("IMM_METAL_VALIDATE_RELOAD_FRAME");
    if (reloadFrame && reloadFrame[0])
    {
        _validationReloadFrame = strtoull(reloadFrame, nullptr, 10);
    }
    const char *resizeWidth = getenv("IMM_METAL_VALIDATE_RESIZE_WIDTH");
    if (resizeWidth && resizeWidth[0])
    {
        _validationResizeWidth = atoi(resizeWidth);
    }
    const char *resizeHeight = getenv("IMM_METAL_VALIDATE_RESIZE_HEIGHT");
    if (resizeHeight && resizeHeight[0])
    {
        _validationResizeHeight = atoi(resizeHeight);
    }
    const char *exitAfter = getenv("IMM_METAL_EXIT_AFTER_VALIDATE");
    _exitAfterValidation = exitAfter && exitAfter[0] && strcmp(exitAfter, "0") != 0;
    const char *capturePath = getenv("IMM_METAL_VALIDATE_CAPTURE_PATH");
    if (capturePath && capturePath[0])
    {
        strncpy(_validationCapturePath, capturePath, sizeof(_validationCapturePath) - 1);
        _validationCapturePath[sizeof(_validationCapturePath) - 1] = 0;
    }
    const char *reloadPath = getenv("IMM_METAL_VALIDATE_RELOAD_PATH");
    if (reloadPath && reloadPath[0])
    {
        strncpy(_validationReloadPath, reloadPath, sizeof(_validationReloadPath) - 1);
        _validationReloadPath[sizeof(_validationReloadPath) - 1] = 0;
    }
}

- (CGSize)initialDrawableSizeForView:(MTKView *)view fallbackFrame:(NSRect)frame
{
    if (_validationEnabled)
    {
        return CGSizeMake((int)frame.size.width > 0 ? (int)frame.size.width : 1,
                          (int)frame.size.height > 0 ? (int)frame.size.height : 1);
    }

    NSSize pointSize = view.bounds.size;
    NSSize backingSize = [view convertSizeToBacking:pointSize];
    const int width = (int)backingSize.width > 0 ? (int)backingSize.width : 1;
    const int height = (int)backingSize.height > 0 ? (int)backingSize.height : 1;
    return CGSizeMake(width, height);
}

- (void)handleNativeFrameFailure:(const char *)reason
{
    ++_nativeFrameFailureCount;
    if (_validationEnabled && _nativeFrameFailureCount >= 8)
    {
        NSLog(@"IMM Metal validation failed: native frame setup failed repeatedly: %s", reason ? reason : "unknown");
        _validationDone = true;
        if (_exitAfterValidation)
        {
            [self terminateWithExitCode:2];
        }
    }
}

- (BOOL)runValidationPipelineSanity
{
    if (!_renderer || !_renderTarget || !_colorTexture || !_pipelineSanityShader || _renderSize.x <= 0 || _renderSize.y <= 0)
    {
        NSLog(@"IMM Metal validation failed: pipeline sanity missing renderer resources");
        return NO;
    }

    const int viewport[4] = { 0, 0, _renderSize.x, _renderSize.y };
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    _renderer->SetRenderTarget(_renderTarget);
    _renderer->SetViewport(0, viewport);
    _renderer->SetWriteMask(true, false, false, false, true);
    _renderer->SetState(ImmCore::piSTATE_DEPTH_TEST, false);
    _renderer->SetState(ImmCore::piSTATE_CULL_FACE, false);
    _renderer->Clear(clearColor, nullptr, nullptr, nullptr, true);
    _renderer->AttachShader(_pipelineSanityShader);
    _renderer->DrawPrimitiveNotIndexed(ImmCore::piRenderer::PrimitiveType::Triangle, 0, 3, 1);
    _renderer->DettachShader();

    const size_t pixelCount = (size_t)_renderSize.x * (size_t)_renderSize.y;
    uint32_t *pixels = (uint32_t *)malloc(pixelCount * sizeof(uint32_t));
    if (!pixels)
    {
        NSLog(@"IMM Metal validation failed: pipeline sanity could not allocate readback buffer");
        return NO;
    }

    memset(pixels, 0, pixelCount * sizeof(uint32_t));
    _renderer->GetTextureContent(_colorTexture, pixels, ImmCore::piRenderer::Format::C3_11_11_10_FLOAT);

    const size_t centerIndex = (size_t)(_renderSize.y / 2) * (size_t)_renderSize.x + (size_t)(_renderSize.x / 2);
    const size_t cornerIndex = 4u * (size_t)_renderSize.x + 4u;
    const size_t oppositeCornerIndex = ((size_t)_renderSize.y - 5u) * (size_t)_renderSize.x + ((size_t)_renderSize.x - 5u);
    uint64_t nonZeroPixels = 0;
    for (size_t i = 0; i < pixelCount; ++i)
    {
        if (pixels[i] != 0)
        {
            ++nonZeroPixels;
        }
    }

    const uint32_t center = pixels[centerIndex];
    const uint32_t corner = pixels[cornerIndex];
    const uint32_t oppositeCorner = pixels[oppositeCornerIndex];
    free(pixels);

    if (center == 0 || corner != 0 || oppositeCorner != 0 || nonZeroPixels < (pixelCount / 20) || nonZeroPixels > (pixelCount / 2))
    {
        NSLog(@"IMM Metal validation failed: pipeline sanity unexpected triangle readback center=%u corner=%u oppositeCorner=%u nonZero=%llu pixels=%zu",
              center,
              corner,
              oppositeCorner,
              (unsigned long long)nonZeroPixels,
              pixelCount);
        return NO;
    }

    _validationPipelineSanityDone = true;
    NSLog(@"IMM Metal pipeline sanity: singleTriangle=1 center=%u corner=%u oppositeCorner=%u nonZero=%llu pixels=%zu",
          center,
          corner,
          oppositeCorner,
          (unsigned long long)nonZeroPixels,
          pixelCount);

    if (_pipelineSanityIndexBuffer && _pipelineSanityIndexedVertexArray)
    {
        _renderer->SetRenderTarget(_renderTarget);
        _renderer->SetViewport(0, viewport);
        _renderer->SetWriteMask(true, false, false, false, true);
        _renderer->SetState(ImmCore::piSTATE_DEPTH_TEST, false);
        _renderer->SetState(ImmCore::piSTATE_CULL_FACE, false);
        _renderer->Clear(clearColor, nullptr, nullptr, nullptr, true);
        _renderer->AttachShader(_pipelineSanityShader);
        _renderer->AttachVertexArray(_pipelineSanityIndexedVertexArray);
        _renderer->DrawPrimitiveIndexed(ImmCore::piRenderer::PrimitiveType::Triangle, 3, 1, 3, 0, 0);
        _renderer->DettachVertexArray();
        _renderer->DettachShader();

        pixels = (uint32_t *)malloc(pixelCount * sizeof(uint32_t));
        if (!pixels)
        {
            NSLog(@"IMM Metal validation failed: indexed pipeline sanity could not allocate readback buffer");
            return NO;
        }
        memset(pixels, 0, pixelCount * sizeof(uint32_t));
        _renderer->GetTextureContent(_colorTexture, pixels, ImmCore::piRenderer::Format::C3_11_11_10_FLOAT);

        const size_t indexedCenterIndex = centerIndex;
        const size_t indexedProbeA = (size_t)(_renderSize.y / 8) * (size_t)_renderSize.x + (size_t)(_renderSize.x / 8);
        const size_t indexedProbeB = (size_t)((_renderSize.y * 7) / 8) * (size_t)_renderSize.x + (size_t)(_renderSize.x / 8);
        const uint32_t indexedCenter = pixels[indexedCenterIndex];
        const uint32_t indexedA = pixels[indexedProbeA];
        const uint32_t indexedB = pixels[indexedProbeB];
        uint64_t indexedNonZeroPixels = 0;
        for (size_t i = 0; i < pixelCount; ++i)
        {
            if (pixels[i] != 0)
            {
                ++indexedNonZeroPixels;
            }
        }
        free(pixels);

        if (indexedCenter != 0 || (indexedA == 0 && indexedB == 0) || indexedNonZeroPixels < (pixelCount / 200) || indexedNonZeroPixels > (pixelCount / 8))
        {
            NSLog(@"IMM Metal validation failed: indexed base-vertex pipeline sanity unexpected readback center=%u probeA=%u probeB=%u nonZero=%llu pixels=%zu",
                  indexedCenter,
                  indexedA,
                  indexedB,
                  (unsigned long long)indexedNonZeroPixels,
                  pixelCount);
            return NO;
        }

        NSLog(@"IMM Metal pipeline sanity: indexedBaseVertexTriangle=1 center=%u probeA=%u probeB=%u nonZero=%llu pixels=%zu",
              indexedCenter,
              indexedA,
              indexedB,
              (unsigned long long)indexedNonZeroPixels,
              pixelCount);
    }
    return YES;
}

- (BOOL)runValidationShaderPathSanity
{
    if (!_renderer || !_renderTarget || !_colorTexture || _renderSize.x <= 0 || _renderSize.y <= 0)
    {
        NSLog(@"IMM Metal validation failed: shader-path sanity missing renderer resources");
        return NO;
    }

    char shaderError[2048] = {};
    ImmCore::piShaderOptions pictureOptions = {};
    pictureOptions.mNum = 3;
    snprintf(pictureOptions.mOption[0].mName, sizeof(pictureOptions.mOption[0].mName), "%s", "COLOR_SPACE");
    snprintf(pictureOptions.mOption[1].mName, sizeof(pictureOptions.mOption[1].mName), "%s", "STEREOMODE");
    snprintf(pictureOptions.mOption[2].mName, sizeof(pictureOptions.mOption[2].mName), "%s", "DEBUG_RENDER_MODE");
    pictureOptions.mOption[0].mValue = 0;
    pictureOptions.mOption[1].mValue = 0;
    pictureOptions.mOption[2].mValue = 0;

    if (!_smokePictureShader)
    {
        _smokePictureShader = _renderer->CreateShader(&pictureOptions, nullptr, nullptr, nullptr, nullptr, nullptr, shaderError);
    }
    if (!_smokePictureShader)
    {
        NSLog(@"IMM Metal validation failed: picture2D shader-path sanity shader creation failed: %s", shaderError);
        return NO;
    }

    ImmCore::piShaderOptions cubemapOptions = {};
    cubemapOptions.mNum = 3;
    snprintf(cubemapOptions.mOption[0].mName, sizeof(cubemapOptions.mOption[0].mName), "%s", "COLOR_SPACE");
    snprintf(cubemapOptions.mOption[1].mName, sizeof(cubemapOptions.mOption[1].mName), "%s", "STEREOMODE");
    snprintf(cubemapOptions.mOption[2].mName, sizeof(cubemapOptions.mOption[2].mName), "%s", "CUBEMAP");
    cubemapOptions.mOption[0].mValue = 0;
    cubemapOptions.mOption[1].mValue = 0;
    cubemapOptions.mOption[2].mValue = 1;

    if (!_smokeCubemapShader)
    {
        shaderError[0] = 0;
        _smokeCubemapShader = _renderer->CreateShader(&cubemapOptions, nullptr, nullptr, nullptr, nullptr, nullptr, shaderError);
    }
    if (!_smokeCubemapShader)
    {
        NSLog(@"IMM Metal validation failed: picture360 cubemap shader-path sanity shader creation failed: %s", shaderError);
        return NO;
    }

    ImmMetalSmokeFrameState frameState = { 0.0f, 17, 0, 0 };
    ImmMetalSmokeLayerState layerState = {};
    iSetIdentity4x4(layerState.mLayerToViewer);
    layerState.mLayerToViewerScale = 1.0f;
    layerState.mOpacity = 1.0f;
    layerState.mFlipSign = 1.0f;
    layerState.mDrawInTime = 1.0f;

    ImmMetalSmokeDisplayState displayState = {};
    iSetIdentity4x4(displayState.mEye[0].mViewerToEyePrj);
    iSetIdentity4x4(displayState.mEye[1].mViewerToEyePrj);
    displayState.mResolution[0] = (float)_renderSize.x;
    displayState.mResolution[1] = (float)_renderSize.y;
    displayState.mEyeIndex = 0;

    if (!_smokeFrameBuffer)
    {
        _smokeFrameBuffer = _renderer->CreateBuffer(&frameState, sizeof(frameState), ImmCore::piRenderer::BufferType::Static, ImmCore::piRenderer::BufferUse::Constant);
    }
    if (!_smokeLayerBuffer)
    {
        _smokeLayerBuffer = _renderer->CreateBuffer(&layerState, sizeof(layerState), ImmCore::piRenderer::BufferType::Static, ImmCore::piRenderer::BufferUse::Constant);
    }
    if (!_smokeDisplayBuffer)
    {
        _smokeDisplayBuffer = _renderer->CreateBuffer(&displayState, sizeof(displayState), ImmCore::piRenderer::BufferType::Static, ImmCore::piRenderer::BufferUse::Constant);
    }

    const uint8_t picturePixels[16] = {
        255, 64, 64, 255,
        64, 255, 64, 255,
        64, 64, 255, 255,
        255, 255, 64, 255,
    };
    const ImmCore::piRenderer::TextureInfo pictureInfo = {
        ImmCore::piRenderer::TextureType::T2D,
        ImmCore::piRenderer::Format::C4_8_UNORM,
        2,
        2,
        1,
        1,
        1,
        0
    };
    if (!_smokePictureTexture)
    {
        _smokePictureTexture = _renderer->CreateTexture(nullptr, &pictureInfo, false, ImmCore::piRenderer::TextureFilter::NONE, ImmCore::piRenderer::TextureWrap::CLAMP, 1.0f, picturePixels);
    }

    const uint8_t cubemapPixels[96] = {
        255, 0, 0, 255, 255, 32, 32, 255, 255, 64, 64, 255, 255, 96, 96, 255,
        0, 255, 0, 255, 32, 255, 32, 255, 64, 255, 64, 255, 96, 255, 96, 255,
        0, 0, 255, 255, 32, 32, 255, 255, 64, 64, 255, 255, 96, 96, 255, 255,
        255, 255, 0, 255, 255, 255, 32, 255, 255, 255, 64, 255, 255, 255, 96, 255,
        255, 0, 255, 255, 255, 32, 255, 255, 255, 64, 255, 255, 255, 96, 255, 255,
        0, 255, 255, 255, 32, 255, 255, 255, 64, 255, 255, 255, 96, 255, 255, 255,
    };
    const ImmCore::piRenderer::TextureInfo cubemapInfo = {
        ImmCore::piRenderer::TextureType::TCUBE,
        ImmCore::piRenderer::Format::C4_8_UNORM,
        2,
        2,
        1,
        1,
        1,
        0
    };
    if (!_smokeCubemapTexture)
    {
        _smokeCubemapTexture = _renderer->CreateTexture(nullptr, &cubemapInfo, false, ImmCore::piRenderer::TextureFilter::NONE, ImmCore::piRenderer::TextureWrap::CLAMP, 1.0f, cubemapPixels);
    }

    const ImmCore::piRenderer::TextureInfo blueNoiseInfo = {
        ImmCore::piRenderer::TextureType::T2D_ARRAY,
        ImmCore::piRenderer::Format::C1_8_UNORM,
        64,
        64,
        64,
        1,
        1,
        0
    };
    if (!_smokeBlueNoiseTexture)
    {
        _smokeBlueNoiseTexture = _renderer->CreateTexture(nullptr, &blueNoiseInfo, false, ImmCore::piRenderer::TextureFilter::NONE, ImmCore::piRenderer::TextureWrap::REPEAT, 1.0f, GetBlueNoise_64x64x64());
    }
    if (!_smokePictureSampler)
    {
        _smokePictureSampler = _renderer->CreateSampler(ImmCore::piRenderer::TextureFilter::NONE, ImmCore::piRenderer::TextureWrap::CLAMP, 1.0f);
    }

    BOOL ok = YES;
    if (!_smokeFrameBuffer || !_smokeLayerBuffer || !_smokeDisplayBuffer || !_smokePictureTexture || !_smokeCubemapTexture || !_smokeBlueNoiseTexture || !_smokePictureSampler)
    {
        NSLog(@"IMM Metal validation failed: shader-path sanity resource creation failed frame=%p layer=%p display=%p picture=%p cubemap=%p blueNoise=%p sampler=%p",
              _smokeFrameBuffer, _smokeLayerBuffer, _smokeDisplayBuffer, _smokePictureTexture, _smokeCubemapTexture, _smokeBlueNoiseTexture, _smokePictureSampler);
        ok = NO;
    }

    const size_t pixelCount = (size_t)_renderSize.x * (size_t)_renderSize.y;
    uint32_t *pixels = ok ? (uint32_t *)malloc(pixelCount * sizeof(uint32_t)) : nullptr;
    if (ok && !pixels)
    {
        NSLog(@"IMM Metal validation failed: shader-path sanity could not allocate readback buffer");
        ok = NO;
    }

    const int viewport[4] = { 0, 0, _renderSize.x, _renderSize.y };
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    const size_t centerIndex = (size_t)(_renderSize.y / 2) * (size_t)_renderSize.x + (size_t)(_renderSize.x / 2);

    if (ok)
    {
        const float pictureSize[4] = { 0.8f, 0.8f, 0.0f, 0.0f };
        _renderer->SetRenderTarget(_renderTarget);
        _renderer->SetViewport(0, viewport);
        _renderer->SetWriteMask(true, false, false, false, true);
        _renderer->SetState(ImmCore::piSTATE_DEPTH_TEST, false);
        _renderer->SetState(ImmCore::piSTATE_CULL_FACE, false);
        _renderer->SetBlending(0,
                               ImmCore::piRenderer::BlendEquation::piBLEND_ADD,
                               ImmCore::piRenderer::BlendOperations::piBLEND_ONE,
                               ImmCore::piRenderer::BlendOperations::piBLEND_ZERO,
                               ImmCore::piRenderer::BlendEquation::piBLEND_ADD,
                               ImmCore::piRenderer::BlendOperations::piBLEND_ONE,
                               ImmCore::piRenderer::BlendOperations::piBLEND_ZERO);
        _renderer->Clear(clearColor, nullptr, nullptr, nullptr, true);
        _renderer->AttachShaderConstants(_smokeFrameBuffer, 0);
        _renderer->AttachShaderConstants(_smokeLayerBuffer, 3);
        _renderer->AttachShaderConstants(_smokeDisplayBuffer, 4);
        _renderer->SetShaderConstant4F(0, pictureSize, 1);
        _renderer->AttachTextures(1, _smokePictureTexture);
        _renderer->AttachTextures(1, &_smokeBlueNoiseTexture, 7);
        _renderer->AttachSamplers(1, _smokePictureSampler);
        _renderer->AttachShader(_smokePictureShader);
        _renderer->DrawUnitQuad_XY(1);
        _renderer->DettachShader();
        _renderer->DettachTextures();

        memset(pixels, 0, pixelCount * sizeof(uint32_t));
        _renderer->GetTextureContent(_colorTexture, pixels, ImmCore::piRenderer::Format::C3_11_11_10_FLOAT);
        const uint32_t center = pixels[centerIndex];
        const uint64_t nonZeroPixels = iCountNonZeroPixels(pixels, pixelCount);
        if (center == 0 || nonZeroPixels < (pixelCount / 12))
        {
            NSLog(@"IMM Metal validation failed: picture2D shader-path sanity unexpected readback center=%u nonZero=%llu pixels=%zu",
                  center,
                  (unsigned long long)nonZeroPixels,
                  pixelCount);
            ok = NO;
        }
        else
        {
            NSLog(@"IMM Metal pipeline sanity: picture2DShader=1 center=%u nonZero=%llu pixels=%zu",
                  center,
                  (unsigned long long)nonZeroPixels,
                  pixelCount);
        }

        _renderer->SetWriteMask(true, false, false, false, true);
        _renderer->SetState(ImmCore::piSTATE_DEPTH_TEST, false);
        _renderer->SetState(ImmCore::piSTATE_CULL_FACE, false);
        _renderer->Clear(clearColor, nullptr, nullptr, nullptr, true);
        _renderer->AttachShaderConstants(_smokeFrameBuffer, 0);
        _renderer->AttachShaderConstants(_smokeLayerBuffer, 3);
        _renderer->AttachShaderConstants(_smokeDisplayBuffer, 4);
        _renderer->AttachTextures(1, _smokeCubemapTexture);
        _renderer->AttachTextures(1, &_smokeBlueNoiseTexture, 7);
        _renderer->AttachSamplers(1, _smokePictureSampler);
        _renderer->AttachShader(_smokeCubemapShader);
        _renderer->DrawUnitCube_XYZ_NOR(1);
        _renderer->DettachShader();
        _renderer->DettachTextures();

        memset(pixels, 0, pixelCount * sizeof(uint32_t));
        _renderer->GetTextureContent(_colorTexture, pixels, ImmCore::piRenderer::Format::C3_11_11_10_FLOAT);
        const uint32_t cubemapCenter = pixels[centerIndex];
        const uint64_t cubemapNonZeroPixels = iCountNonZeroPixels(pixels, pixelCount);
        if (cubemapCenter == 0 || cubemapNonZeroPixels < (pixelCount / 20))
        {
            NSLog(@"IMM Metal validation failed: picture360 cubemap shader-path sanity unexpected readback center=%u nonZero=%llu pixels=%zu",
                  cubemapCenter,
                  (unsigned long long)cubemapNonZeroPixels,
                  pixelCount);
            ok = NO;
        }
        else
        {
            NSLog(@"IMM Metal pipeline sanity: picture360CubemapShader=1 center=%u nonZero=%llu pixels=%zu",
                  cubemapCenter,
                  (unsigned long long)cubemapNonZeroPixels,
                  pixelCount);
        }
    }

    if (pixels)
    {
        free(pixels);
    }
    return ok;
}

- (void)applyValidationResizeIfRequested
{
    if (!_validationEnabled ||
        _validationResizeDone ||
        _validationResizeFrame == 0 ||
        _frameIndex < _validationResizeFrame ||
        _validationResizeWidth <= 0 ||
        _validationResizeHeight <= 0)
    {
        return;
    }

    const NSSize contentSize = NSMakeSize(_validationResizeWidth, _validationResizeHeight);
    [_window setContentSize:contentSize];
    _view.drawableSize = CGSizeMake(_validationResizeWidth, _validationResizeHeight);
    _validationResizeDone = true;
    NSLog(@"IMM Metal validation resize: frame=%llu width=%d height=%d",
          (unsigned long long)_frameIndex,
          _validationResizeWidth,
          _validationResizeHeight);
}

- (void)validateOffscreenFrameIfRequested
{
    if (!_validationEnabled || _validationDone || _frameIndex < _validationFrame || !_colorTexture || _renderSize.x <= 0 || _renderSize.y <= 0)
    {
        return;
    }

    if (_validationReloadFrame != 0 && !_validationReloadDone)
    {
        return;
    }

    const size_t pixelCount = (size_t)_renderSize.x * (size_t)_renderSize.y;
    uint32_t *pixels = (uint32_t *)malloc(pixelCount * sizeof(uint32_t));
    if (!pixels)
    {
        NSLog(@"IMM Metal validation failed: could not allocate readback buffer");
        if (_exitAfterValidation)
        {
            gExitCode = 2;
            _validationDone = true;
        }
        return;
    }

    memset(pixels, 0, pixelCount * sizeof(uint32_t));
    _renderer->GetTextureContent(_colorTexture, pixels, ImmCore::piRenderer::Format::C3_11_11_10_FLOAT);

    uint64_t nonZeroPixels = 0;
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < pixelCount; ++i)
    {
        if (pixels[i] != 0)
        {
            ++nonZeroPixels;
        }
        hash ^= pixels[i];
        hash *= 1099511628211ull;
    }
    const ImmPlayer::Player::PerformanceInfo &perf = _viewer.GetPerformanceInfoForFrame();
    const bool passed =
        nonZeroPixels >= _validationMinNonZeroPixels &&
        perf.numDrawCalls >= 0 &&
        perf.numTriangles >= 0 &&
        (uint64_t)perf.numDrawCalls >= _validationMinDrawCalls &&
        (uint64_t)perf.numPictureDrawCalls >= _validationMinPictureDrawCalls &&
        (uint64_t)perf.numPicture2DDrawCalls >= _validationMinPicture2DDrawCalls &&
        (uint64_t)perf.numPicture360DrawCalls >= _validationMinPicture360DrawCalls &&
        (uint64_t)perf.numPicture360EquirectDrawCalls >= _validationMinPicture360EquirectDrawCalls &&
        (uint64_t)perf.numPicture360CubemapDrawCalls >= _validationMinPicture360CubemapDrawCalls &&
        (uint64_t)perf.numTriangles >= _validationMinTriangles;

    if (!passed)
    {
        if (_frameIndex < _validationMaxFrame)
        {
            free(pixels);
            return;
        }
        _validationDone = true;
        NSLog(@"IMM Metal validation failed: frame=%llu pixels=%zu nonZero=%llu minNonZero=%llu hash=%llu drawCalls=%d minDrawCalls=%llu paintDrawCalls=%d pictureDrawCalls=%d minPictureDrawCalls=%llu picture2DDrawCalls=%d minPicture2DDrawCalls=%llu picture360DrawCalls=%d minPicture360DrawCalls=%llu picture360EquirectDrawCalls=%d minPicture360EquirectDrawCalls=%llu picture360CubemapDrawCalls=%d minPicture360CubemapDrawCalls=%llu modelDrawCalls=%d triangles=%d minTriangles=%llu culledCalls=%d",
              (unsigned long long)_frameIndex,
              pixelCount,
              (unsigned long long)nonZeroPixels,
              (unsigned long long)_validationMinNonZeroPixels,
              (unsigned long long)hash,
              perf.numDrawCalls,
              (unsigned long long)_validationMinDrawCalls,
              perf.numPaintDrawCalls,
              perf.numPictureDrawCalls,
              (unsigned long long)_validationMinPictureDrawCalls,
              perf.numPicture2DDrawCalls,
              (unsigned long long)_validationMinPicture2DDrawCalls,
              perf.numPicture360DrawCalls,
              (unsigned long long)_validationMinPicture360DrawCalls,
              perf.numPicture360EquirectDrawCalls,
              (unsigned long long)_validationMinPicture360EquirectDrawCalls,
              perf.numPicture360CubemapDrawCalls,
              (unsigned long long)_validationMinPicture360CubemapDrawCalls,
              perf.numModelDrawCalls,
              perf.numTriangles,
              (unsigned long long)_validationMinTriangles,
              perf.numDrawCallsCulled);
        if (_exitAfterValidation)
        {
            gExitCode = 2;
        }
        free(pixels);
        return;
    }

    _validationDone = true;
    NSLog(@"IMM Metal validation: frame=%llu pixels=%zu nonZero=%llu hash=%llu drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d picture2DDrawCalls=%d picture360DrawCalls=%d picture360EquirectDrawCalls=%d picture360CubemapDrawCalls=%d modelDrawCalls=%d triangles=%d culledCalls=%d",
          (unsigned long long)_frameIndex,
          pixelCount,
          (unsigned long long)nonZeroPixels,
          (unsigned long long)hash,
          perf.numDrawCalls,
          perf.numPaintDrawCalls,
          perf.numPictureDrawCalls,
          perf.numPicture2DDrawCalls,
          perf.numPicture360DrawCalls,
          perf.numPicture360EquirectDrawCalls,
          perf.numPicture360CubemapDrawCalls,
          perf.numModelDrawCalls,
          perf.numTriangles,
          perf.numDrawCallsCulled);

    if (_validationCapturePath[0])
    {
        if (iWriteRG11B10Capture(_validationCapturePath, pixels, _renderSize.x, _renderSize.y))
        {
            NSLog(@"IMM Metal validation capture: %s", _validationCapturePath);
        }
        else
        {
            NSLog(@"IMM Metal validation capture failed: %s", _validationCapturePath);
            if (_exitAfterValidation)
            {
                gExitCode = 2;
            }
        }
    }

    free(pixels);
}

- (void)destroyRenderTargetResources
{
    if (!_renderer)
    {
        return;
    }
    if (_renderTarget)
    {
        _renderer->DestroyRenderTarget(_renderTarget);
        _renderTarget = nullptr;
    }
    if (_depthTexture)
    {
        _renderer->DestroyTexture(_depthTexture);
        _depthTexture = nullptr;
    }
    if (_colorTexture)
    {
        _renderer->DestroyTexture(_colorTexture);
        _colorTexture = nullptr;
    }
    _renderSize = ImmCore::ivec2(0, 0);
}

- (BOOL)ensureRenderTargetForSize:(CGSize)size
{
    const int width = (int)size.width > 0 ? (int)size.width : 1;
    const int height = (int)size.height > 0 ? (int)size.height : 1;
    if (_renderTarget && _renderSize.x == width && _renderSize.y == height)
    {
        return YES;
    }

    [self destroyRenderTargetResources];
    _renderSize = ImmCore::ivec2(width, height);

    const ImmCore::piRenderer::TextureInfo colorInfo = {
        ImmCore::piRenderer::TextureType::T2D,
        ImmCore::piRenderer::Format::C3_11_11_10_FLOAT,
        _renderSize.x,
        _renderSize.y,
        1,
        1,
        1,
        0
    };
    const ImmCore::piRenderer::TextureInfo depthInfo = {
        ImmCore::piRenderer::TextureType::T2D,
        ImmCore::piRenderer::Format::D1_32_FLOAT,
        _renderSize.x,
        _renderSize.y,
        1,
        1,
        1,
        0
    };
    _colorTexture = _renderer->CreateTexture(nullptr, &colorInfo, false, ImmCore::piRenderer::TextureFilter::NONE, ImmCore::piRenderer::TextureWrap::CLAMP, 1.0f, nullptr);
    _depthTexture = _renderer->CreateTexture(nullptr, &depthInfo, false, ImmCore::piRenderer::TextureFilter::NONE, ImmCore::piRenderer::TextureWrap::CLAMP, 1.0f, nullptr);
    _renderTarget = _renderer->CreateRenderTarget(_colorTexture, nullptr, nullptr, nullptr, _depthTexture);
    return _colorTexture && _depthTexture && _renderTarget;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    (void)notification;
    [self updateRecentDocumentsMenu];

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device)
    {
        NSLog(@"IMM Metal player: no Metal device available");
        [self terminateWithExitCode:1];
        return;
    }

    _renderer = static_cast<ImmCore::piRendererMetal *>(ImmCore::piRenderer::Create(ImmCore::piRenderer::API::Metal));
    if (!_renderer || !_renderer->Initialize(0, nullptr, 0, true, false, &_reporter, false, (__bridge void *)device))
    {
        NSLog(@"IMM Metal player: piRendererMetal initialization failed");
        [self terminateWithExitCode:1];
        return;
    }

    wchar_t logPath[PATH_MAX] = L"metal_player_debug.txt";
    const char *logPathEnv = getenv("IMM_METAL_LOG_PATH");
    if (logPathEnv && logPathEnv[0])
    {
        const size_t logPathLen = mbstowcs(logPath, logPathEnv, PATH_MAX - 1);
        if (logPathLen == (size_t)-1)
        {
            NSLog(@"IMM Metal player: invalid log path");
            [self terminateWithExitCode:1];
            return;
        }
        logPath[logPathLen] = 0;
    }

    if (!_log.Init(logPath, PILOG_TXT + PILOG_CNS))
    {
        NSLog(@"IMM Metal player: log initialization failed");
        [self terminateWithExitCode:1];
        return;
    }

    if (!_timer.Init())
    {
        NSLog(@"IMM Metal player: timer initialization failed");
        [self terminateWithExitCode:1];
        return;
    }

    if (!_settings.Init(gSettingsPath, &_log))
    {
        NSLog(@"IMM Metal player: settings initialization failed");
        [self terminateWithExitCode:1];
        return;
    }
    _settings.mRendering.mRenderingAPI = ExePlayer::Settings::Rendering::API::Metal;
    _settings.mRendering.mEnableVR = false;
    if (!iOverrideLoadedFiles(&_settings))
    {
        NSLog(@"IMM Metal player: could not apply content paths");
        [self terminateWithExitCode:1];
        return;
    }
    if (!iApplyDefaultImmWhenNoLoadConfigured(&_settings))
    {
        NSLog(@"IMM Metal player: could not apply default IMM file");
        [self terminateWithExitCode:1];
        return;
    }
    [self configureValidationFromEnvironment];

    char shaderError[2048] = {};
    _shader = _renderer->CreateShader(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, shaderError);
    if (!_shader)
    {
        NSLog(@"IMM Metal player: shader creation failed: %s", shaderError);
        [self terminateWithExitCode:1];
        return;
    }

    _pipelineSanityShader = _renderer->CreateShaderBinary(nullptr,
                                                          nullptr,
                                                          0,
                                                          nullptr,
                                                          0,
                                                          nullptr,
                                                          0,
                                                          nullptr,
                                                          0,
                                                          nullptr,
                                                          0,
                                                          shaderError);
    if (!_pipelineSanityShader)
    {
        NSLog(@"IMM Metal player: pipeline sanity shader creation failed: %s", shaderError);
        [self terminateWithExitCode:1];
        return;
    }

    const ImmMetalDebugVertex vertices[3] = {
        {{-0.65f, -0.55f}, {0.95f, 0.25f, 0.18f, 1.0f}},
        {{ 0.65f, -0.55f}, {0.18f, 0.75f, 0.95f, 1.0f}},
        {{ 0.00f,  0.65f}, {0.95f, 0.85f, 0.25f, 1.0f}},
    };
    _vertexBuffer = _renderer->CreateBuffer(vertices, sizeof(vertices), ImmCore::piRenderer::BufferType::Static, ImmCore::piRenderer::BufferUse::Vertex);
    if (!_vertexBuffer)
    {
        NSLog(@"IMM Metal player: vertex buffer creation failed");
        [self terminateWithExitCode:1];
        return;
    }

    ImmCore::piRArrayLayout layout = {};
    layout.mStride = sizeof(ImmMetalDebugVertex);
    layout.mNumElements = 2;
    layout.mEntry[0].mNumComponents = 2;
    layout.mEntry[0].mType = ImmCore::piRArrayType_Float;
    layout.mEntry[0].mNormalize = false;
    layout.mEntry[1].mNumComponents = 4;
    layout.mEntry[1].mType = ImmCore::piRArrayType_Float;
    layout.mEntry[1].mNormalize = false;

    _vertexArray = _renderer->CreateVertexArray(1, _vertexBuffer, &layout, nullptr, nullptr, nullptr, ImmCore::piRenderer::IndexArrayFormat::UINT_32);
    if (!_vertexArray)
    {
        NSLog(@"IMM Metal player: vertex array creation failed");
        [self terminateWithExitCode:1];
        return;
    }

    const uint16_t sanityIndices[3] = { 0, 1, 2 };
    _pipelineSanityIndexBuffer = _renderer->CreateBuffer(sanityIndices, sizeof(sanityIndices), ImmCore::piRenderer::BufferType::Static, ImmCore::piRenderer::BufferUse::Index);
    if (!_pipelineSanityIndexBuffer)
    {
        NSLog(@"IMM Metal player: indexed pipeline sanity index buffer creation failed");
        [self terminateWithExitCode:1];
        return;
    }
    _pipelineSanityIndexedVertexArray = _renderer->CreateVertexArray(0, nullptr, nullptr, nullptr, nullptr, _pipelineSanityIndexBuffer, ImmCore::piRenderer::IndexArrayFormat::UINT_16);
    if (!_pipelineSanityIndexedVertexArray)
    {
        NSLog(@"IMM Metal player: indexed pipeline sanity vertex array creation failed");
        [self terminateWithExitCode:1];
        return;
    }

    NSRect frame = NSMakeRect(_settings.mWindow.mPositionX,
                              _settings.mWindow.mPositionY,
                              _settings.mWindow.mWidth,
                              _settings.mWindow.mHeight);
    const bool fullScreen = _settings.mWindow.mFullScreen && !_validationEnabled;
    if (fullScreen)
    {
        NSScreen *screen = [NSScreen mainScreen];
        if (screen)
        {
            frame = [screen frame];
        }
    }
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:(NSWindowStyleMaskTitled |
                                                     NSWindowStyleMaskClosable |
                                                     NSWindowStyleMaskResizable |
                                                     NSWindowStyleMaskMiniaturizable)
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    NSString *windowTitle = iWindowTitleForSettings(_settings);
    [_window setTitle:windowTitle];
    if (_validationEnabled)
    {
        NSLog(@"IMM Metal validation window title: %@", windowTitle);
    }

    ImmMetalView *metalView = [[ImmMetalView alloc] initWithFrame:frame device:device];
    [metalView setEventSink:self];
    _view = metalView;
    [_view setColorPixelFormat:MTLPixelFormatBGRA8Unorm];
    [_view setDepthStencilPixelFormat:MTLPixelFormatDepth32Float];
    [_view setClearColor:MTLClearColorMake(0.08, 0.10, 0.13, 1.0)];
    [_view setPreferredFramesPerSecond:60];
    [_view setPaused:NO];
    [_view setEnableSetNeedsDisplay:NO];
    [_view setDelegate:self];
    _view.autoResizeDrawable = !_validationEnabled;
    _view.drawableSize = [self initialDrawableSizeForView:_view fallbackFrame:frame];

    if (![self ensureRenderTargetForSize:_view.drawableSize])
    {
        NSLog(@"IMM Metal player: render texture setup failed");
        [self terminateWithExitCode:1];
        return;
    }

    if (!_resolve.Init(_renderer, 1, 1))
    {
        NSLog(@"IMM Metal player: resolve initialization failed");
        [self terminateWithExitCode:1];
        return;
    }

    if (![self initializeSoundBackend])
    {
        NSLog(@"IMM Metal player: sound backend initialization failed");
        [self terminateWithExitCode:1];
        return;
    }

    if (!_viewer.Init(nullptr, _renderer, _soundBackend->GetEngine(), &_log, &_timer, ImmPlayer::StereoMode::None, &_settings))
    {
        NSLog(@"IMM Metal player: viewer initialization failed");
        [self terminateWithExitCode:1];
        return;
    }
    _viewerReady = true;
    _firstFrame = true;
    _timeBase = _timer.GetTime();
    _lastTime = 0.0;
    _frameIndex = 0;
    _nativeFrameFailureCount = 0;
    _documentVolume = _viewer.GetVolume(0);
    if (_documentVolume <= 0.0f)
    {
        _documentVolume = 1.0f;
    }
    _volumeBeforeMute = _documentVolume;
    _muted = false;
    _volumeSmokeDone = false;
    _recentDocumentSmokeDone = false;
    _playbackControlSmokeDone = false;
    _openFailureRestoreSmokeDone = false;
    [self runVolumeControlSmokeIfRequested];
    [self noteCurrentDocumentAsRecentWithReason:@"initial-load"];
    [self runRecentDocumentSmokeIfRequested];

    [_window setContentView:_view];
    [_window makeKeyAndOrderFront:nil];
    [_window makeFirstResponder:_view];
    if (!_validationEnabled)
    {
        [NSApp activateIgnoringOtherApps:YES];
    }
    if (fullScreen)
    {
        [_window toggleFullScreen:nil];
    }

    if (_validationEnabled)
    {
        [_view setPaused:YES];
        [_view setEnableSetNeedsDisplay:YES];
        [self performSelector:@selector(validationDrawTick) withObject:nil afterDelay:0.0];
    }
    else if (_smokeExitAfterSeconds > 0.0)
    {
        [_view setPaused:YES];
        [_view setEnableSetNeedsDisplay:YES];
        [self performSelector:@selector(interactiveSmokeDrawTick) withObject:nil afterDelay:0.0];
    }
}

- (BOOL)application:(NSApplication *)sender openFile:(NSString *)filename
{
    (void)sender;
    if (_viewerReady)
    {
        return [self loadContentFile:filename];
    }

    const char *path = [filename fileSystemRepresentation];
    if (iHasExtension(path, ".json"))
    {
        return iSetSettingsPath(path) ? YES : NO;
    }
    if (gContentPathCount >= 1)
    {
        return YES;
    }
    if (!iAddContentPath(path))
    {
        NSLog(@"IMM Metal player: could not accept opened IMM file");
        return NO;
    }
    return YES;
}

- (IBAction)openDocument:(id)sender
{
    (void)sender;
    if (_validationEnabled)
    {
        return;
    }

    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseDirectories:NO];
    [panel setCanChooseFiles:YES];
    [panel setAllowedFileTypes:@[@"imm"]];

    if ([panel runModal] != NSModalResponseOK)
    {
        return;
    }

    NSURL *url = [[panel URLs] firstObject];
    if (!url)
    {
        return;
    }

    if (_viewerReady)
    {
        [self loadContentFile:[url path]];
    }
    else
    {
        iAddContentPath([[url path] fileSystemRepresentation]);
    }
}

- (IBAction)openRecentDocumentFromMenu:(id)sender
{
    if (_validationEnabled)
    {
        return;
    }

    NSURL *url = nil;
    if ([sender respondsToSelector:@selector(representedObject)])
    {
        id represented = [sender representedObject];
        if ([represented isKindOfClass:[NSURL class]])
        {
            url = (NSURL *)represented;
        }
    }

    if (!url || ![url isFileURL])
    {
        return;
    }

    NSString *path = [url path];
    if (![path length])
    {
        return;
    }

    if (_viewerReady)
    {
        [self loadContentFile:path];
    }
    else
    {
        iAddContentPath([path fileSystemRepresentation]);
    }
}

- (IBAction)clearRecentDocumentsFromMenu:(id)sender
{
    (void)sender;
    [[NSDocumentController sharedDocumentController] clearRecentDocuments:nil];
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:kRecentImmFilesDefaultsKey];
    [self updateRecentDocumentsMenu];
}

- (IBAction)togglePlayback:(id)sender
{
    (void)sender;
    if (!_viewerReady || _validationEnabled)
    {
        return;
    }

    const bool wasPaused = _viewer.IsPaused(0);
    if (wasPaused)
    {
        _viewer.Resume(0);
    }
    else
    {
        _viewer.Pause(0);
    }
    NSLog(@"IMM Metal player playback control: action=%@ previousPaused=%d",
          wasPaused ? @"resume" : @"pause",
          wasPaused ? 1 : 0);
}

- (IBAction)restartPlayback:(id)sender
{
    (void)sender;
    if (!_viewerReady || _validationEnabled)
    {
        return;
    }

    _viewer.Restart(0);
    NSLog(@"IMM Metal player playback control: action=restart");
}

- (IBAction)previousChapter:(id)sender
{
    (void)sender;
    if (!_viewerReady || _validationEnabled)
    {
        return;
    }

    _viewer.Prev(0);
    NSLog(@"IMM Metal player playback control: action=previous");
}

- (IBAction)nextChapter:(id)sender
{
    (void)sender;
    if (!_viewerReady || _validationEnabled)
    {
        return;
    }

    _viewer.Next(0);
    NSLog(@"IMM Metal player playback control: action=next");
}

- (void)setDocumentVolume:(float)volume muted:(BOOL)muted reason:(NSString *)reason
{
    if (!_viewerReady)
    {
        return;
    }

    _documentVolume = iClamp01(volume);
    _muted = muted;
    if (!_muted && _documentVolume > 0.0f)
    {
        _volumeBeforeMute = _documentVolume;
    }
    _viewer.SetVolume(0, _muted ? 0.0f : _documentVolume);
    NSLog(@"IMM Metal player audio volume: reason=%@ volume=%.2f muted=%d applied=%.2f",
          reason ? reason : @"unknown",
          _documentVolume,
          _muted ? 1 : 0,
          _muted ? 0.0f : _documentVolume);
}

- (IBAction)toggleMute:(id)sender
{
    (void)sender;
    if (!_viewerReady || _validationEnabled)
    {
        return;
    }

    if (_muted)
    {
        [self setDocumentVolume:(_volumeBeforeMute > 0.0f ? _volumeBeforeMute : 1.0f) muted:NO reason:@"mute-off"];
    }
    else
    {
        _volumeBeforeMute = (_documentVolume > 0.0f ? _documentVolume : 1.0f);
        [self setDocumentVolume:_volumeBeforeMute muted:YES reason:@"mute-on"];
    }
}

- (IBAction)volumeUp:(id)sender
{
    (void)sender;
    if (!_viewerReady || _validationEnabled)
    {
        return;
    }

    [self setDocumentVolume:iClamp01((_muted ? _volumeBeforeMute : _documentVolume) + 0.1f) muted:NO reason:@"volume-up"];
}

- (IBAction)volumeDown:(id)sender
{
    (void)sender;
    if (!_viewerReady || _validationEnabled)
    {
        return;
    }

    [self setDocumentVolume:iClamp01((_muted ? _volumeBeforeMute : _documentVolume) - 0.1f) muted:NO reason:@"volume-down"];
}

- (void)runVolumeControlSmokeIfRequested
{
    if (_volumeSmokeDone)
    {
        return;
    }

    const char *smoke = getenv("IMM_METAL_VALIDATE_VOLUME_CONTROLS");
    if (!smoke || !smoke[0] || strcmp(smoke, "0") == 0)
    {
        return;
    }

    _volumeSmokeDone = true;
    [self setDocumentVolume:0.5f muted:NO reason:@"volume-smoke-set-half"];
    [self setDocumentVolume:0.5f muted:YES reason:@"volume-smoke-mute"];
    [self setDocumentVolume:0.75f muted:NO reason:@"volume-smoke-restore"];
    NSLog(@"IMM Metal player audio volume smoke: done=1 final=%.2f muted=%d",
          _documentVolume,
          _muted ? 1 : 0);
}

- (void)runPlaybackControlSmokeIfRequested
{
    if (_playbackControlSmokeDone)
    {
        return;
    }

    const char *smoke = getenv("IMM_METAL_VALIDATE_PLAYBACK_CONTROLS");
    if (!smoke || !smoke[0] || strcmp(smoke, "0") == 0)
    {
        return;
    }

    _playbackControlSmokeDone = true;
    _viewer.Pause(0);
    NSLog(@"IMM Metal player playback control: action=pause smoke=1");
    _viewer.Resume(0);
    NSLog(@"IMM Metal player playback control: action=resume smoke=1");
    _viewer.Restart(0);
    NSLog(@"IMM Metal player playback control: action=restart smoke=1");
    NSLog(@"IMM Metal player playback control smoke: done=1 pauseResume=1 restart=1");
}

- (void)runOpenFailureRestoreSmokeIfRequested
{
    if (_openFailureRestoreSmokeDone)
    {
        return;
    }

    const char *smoke = getenv("IMM_METAL_VALIDATE_OPEN_FAILURE_RESTORE");
    if (!smoke || !smoke[0] || strcmp(smoke, "0") == 0)
    {
        return;
    }

    _openFailureRestoreSmokeDone = true;
    NSString *badPath = [NSTemporaryDirectory() stringByAppendingPathComponent:@"imm-metal-open-failure-does-not-exist.imm"];
    setenv("IMM_METAL_FORCE_NEXT_OPEN_FAILURE", "1", 1);
    (void)[self loadContentFile:badPath];
    unsetenv("IMM_METAL_FORCE_NEXT_OPEN_FAILURE");
    NSLog(@"IMM Metal player open failure restore smoke: done=1 viewerReady=%d",
          _viewerReady ? 1 : 0);
}

- (void)updateRecentDocumentsMenu
{
    NSMenuItem *openRecentItem = iFindMenuItemWithTag([NSApp mainMenu], kOpenRecentMenuItemTag);
    NSMenu *menu = [openRecentItem submenu];
    if (!menu)
    {
        return;
    }

    [menu removeAllItems];

    NSArray<NSURL *> *urls = [self recentImmDocumentURLs];
    NSUInteger added = 0;
    for (NSURL *url in urls)
    {
        NSString *path = [url path];
        NSString *title = [[path lastPathComponent] length] ? [path lastPathComponent] : path;
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title
                                                      action:@selector(openRecentDocumentFromMenu:)
                                               keyEquivalent:@""];
        [item setTarget:self];
        [item setRepresentedObject:url];
        [menu addItem:item];
        ++added;
    }

    if (added == 0)
    {
        NSMenuItem *empty = [[NSMenuItem alloc] initWithTitle:@"No Recent IMM Files" action:nil keyEquivalent:@""];
        [empty setEnabled:NO];
        [menu addItem:empty];
    }

    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *clearItem = [[NSMenuItem alloc] initWithTitle:@"Clear Menu"
                                                       action:@selector(clearRecentDocumentsFromMenu:)
                                                keyEquivalent:@""];
    [clearItem setTarget:self];
    [clearItem setEnabled:(added > 0)];
    [menu addItem:clearItem];
}

- (NSArray<NSURL *> *)recentImmDocumentURLs
{
    NSMutableArray<NSURL *> *urls = [NSMutableArray array];
    NSMutableSet<NSString *> *seen = [NSMutableSet set];
    NSArray *storedPaths = [[NSUserDefaults standardUserDefaults] arrayForKey:kRecentImmFilesDefaultsKey];
    if ([storedPaths isKindOfClass:[NSArray class]])
    {
        for (id value in storedPaths)
        {
            if (![value isKindOfClass:[NSString class]])
            {
                continue;
            }
            NSString *path = (NSString *)value;
            if (![path length] ||
                ![[[path pathExtension] lowercaseString] isEqualToString:@"imm"] ||
                [seen containsObject:path])
            {
                continue;
            }
            NSURL *url = [NSURL fileURLWithPath:path];
            if (!url)
            {
                continue;
            }
            [urls addObject:url];
            [seen addObject:path];
        }
    }

    for (NSURL *url in [[NSDocumentController sharedDocumentController] recentDocumentURLs])
    {
        if (![url isFileURL])
        {
            continue;
        }
        NSString *path = [url path];
        if (![path length] ||
            ![[[path pathExtension] lowercaseString] isEqualToString:@"imm"] ||
            [seen containsObject:path])
        {
            continue;
        }
        [urls addObject:url];
        [seen addObject:path];
    }
    return urls;
}

- (void)runRecentDocumentSmokeIfRequested
{
    if (_recentDocumentSmokeDone)
    {
        return;
    }

    const char *smoke = getenv("IMM_METAL_VALIDATE_RECENT_DOCUMENTS");
    if (!smoke || !smoke[0] || strcmp(smoke, "0") == 0)
    {
        return;
    }

    _recentDocumentSmokeDone = true;
    [self noteCurrentDocumentAsRecentWithReason:@"recent-smoke"];

    NSMenuItem *openRecentItem = iFindMenuItemWithTag([NSApp mainMenu], kOpenRecentMenuItemTag);
    NSMenu *menu = [openRecentItem submenu];
    NSUInteger enabledRecentItems = 0;
    for (NSMenuItem *item in [menu itemArray])
    {
        if ([item action] == @selector(openRecentDocumentFromMenu:) && [item isEnabled])
        {
            ++enabledRecentItems;
        }
    }

    NSUInteger recentImmURLs = [[self recentImmDocumentURLs] count];

    NSLog(@"IMM Metal player recent document smoke: done=1 recentImmURLs=%lu menuItems=%lu",
          (unsigned long)recentImmURLs,
          (unsigned long)enabledRecentItems);

    [[NSDocumentController sharedDocumentController] clearRecentDocuments:nil];
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:kRecentImmFilesDefaultsKey];
    [self updateRecentDocumentsMenu];
}

- (void)noteCurrentDocumentAsRecentWithReason:(NSString *)reason
{
    const char *recentSmoke = getenv("IMM_METAL_VALIDATE_RECENT_DOCUMENTS");
    const bool allowSmokeRecent = recentSmoke && recentSmoke[0] && strcmp(recentSmoke, "0") != 0;
    if (_validationEnabled ||
        (_smokeExitAfterSeconds > 0.0 && !allowSmokeRecent) ||
        _settings.mFiles.mLoad.GetLength() < 1)
    {
        return;
    }

    const wchar_t *pathW = _settings.mFiles.mLoad[0].GetS();
    if (!pathW || !pathW[0])
    {
        return;
    }

    char pathUtf8[PATH_MAX] = {};
    const size_t pathLen = wcstombs(pathUtf8, pathW, sizeof(pathUtf8) - 1);
    if (pathLen == (size_t)-1)
    {
        return;
    }
    pathUtf8[pathLen] = 0;

    NSString *path = [NSString stringWithUTF8String:pathUtf8];
    if (![path length])
    {
        return;
    }

    NSURL *url = [NSURL fileURLWithPath:path];
    if (!url)
    {
        return;
    }

    NSMutableArray<NSString *> *paths = [NSMutableArray arrayWithObject:path];
    NSArray *storedPaths = [[NSUserDefaults standardUserDefaults] arrayForKey:kRecentImmFilesDefaultsKey];
    if ([storedPaths isKindOfClass:[NSArray class]])
    {
        for (id value in storedPaths)
        {
            if (![value isKindOfClass:[NSString class]])
            {
                continue;
            }
            NSString *storedPath = (NSString *)value;
            if (![storedPath length] ||
                [storedPath isEqualToString:path] ||
                ![[[storedPath pathExtension] lowercaseString] isEqualToString:@"imm"])
            {
                continue;
            }
            [paths addObject:storedPath];
            if ([paths count] >= 10)
            {
                break;
            }
        }
    }

    [[NSUserDefaults standardUserDefaults] setObject:paths forKey:kRecentImmFilesDefaultsKey];
    [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:url];
    [self updateRecentDocumentsMenu];
    NSLog(@"IMM Metal player recent document: reason=%@ path=%@",
          reason ? reason : @"unknown",
          path);
}

- (BOOL)loadContentFile:(NSString *)filename
{
    return [self loadContentFile:filename allowValidationReload:NO];
}

- (BOOL)initializeSoundBackend
{
    const char *disableAudioForValidation = getenv("IMM_VIEWER_VALIDATE_DISABLE_AUDIO");
    const bool useNullSoundBackend = (_validationEnabled && (!disableAudioForValidation || disableAudioForValidation[0] != '0')) ||
                                     (disableAudioForValidation && disableAudioForValidation[0] && disableAudioForValidation[0] != '0');
    NSLog(@"IMM Metal player sound backend: %s", useNullSoundBackend ? "Null" : "AVFoundation");
    _soundBackend = ImmCore::piCreateSoundEngineBackend(useNullSoundBackend ? ImmCore::piSoundEngineBackend::API::Null
                                                                            : ImmCore::piSoundEngineBackend::API::AVFoundation,
                                                        &_log);
    ImmCore::piSoundEngineBackend::Configuration soundConfig;
    soundConfig.mTempPath = NSTemporaryDirectory().UTF8String;
    return _soundBackend && _soundBackend->Init(nullptr, -1, &soundConfig);
}

- (void)presentOpenFailureForPath:(NSString *)path message:(NSString *)message
{
    if (_validationEnabled)
    {
        return;
    }
    const char *suppressAlert = getenv("IMM_METAL_SUPPRESS_OPEN_FAILURE_ALERT");
    if (suppressAlert && suppressAlert[0] && strcmp(suppressAlert, "0") != 0)
    {
        return;
    }

    NSAlert *alert = [[NSAlert alloc] init];
    [alert setAlertStyle:NSAlertStyleWarning];
    [alert setMessageText:@"Could not open IMM file"];
    [alert setInformativeText:[NSString stringWithFormat:@"%@\n\n%@", message ? message : @"The selected file could not be loaded.", path ? path : @""]];
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

- (BOOL)loadContentFile:(NSString *)filename allowValidationReload:(BOOL)allowValidationReload
{
    if (!filename || !_viewerReady || (_validationEnabled && !allowValidationReload))
    {
        return NO;
    }

    const char *path = [filename fileSystemRepresentation];
    if (!path || !path[0])
    {
        NSLog(@"IMM Metal player: invalid opened IMM file path");
        [self presentOpenFailureForPath:filename message:@"The selected file path is empty or invalid."];
        return NO;
    }

    wchar_t pathW[PATH_MAX] = {};
    const size_t pathLen = mbstowcs(pathW, path, PATH_MAX - 1);
    if (pathLen == (size_t)-1)
    {
        NSLog(@"IMM Metal player: invalid opened IMM file path encoding");
        [self presentOpenFailureForPath:filename message:@"The selected file path could not be converted to the player path encoding."];
        return NO;
    }
    pathW[pathLen] = 0;

    wchar_t previousPathW[PATH_MAX] = {};
    bool hasPreviousPath = false;
    if (_settings.mFiles.mLoad.GetLength() > 0)
    {
        const wchar_t *loadedPath = _settings.mFiles.mLoad[0].GetS();
        if (loadedPath && loadedPath[0])
        {
            wcsncpy(previousPathW, loadedPath, PATH_MAX - 1);
            previousPathW[PATH_MAX - 1] = 0;
            hasPreviousPath = true;
        }
    }

    auto failReload = [&](NSString *message) -> BOOL {
        NSLog(@"IMM Metal player: opened IMM file failed: %@ (%@)", filename, message);
        if (_validationEnabled)
        {
            [self terminateWithExitCode:1];
            return NO;
        }

        if (_soundBackend)
        {
            _soundBackend->Deinit();
            ImmCore::piDestroySoundEngineBackend(_soundBackend);
            _soundBackend = nullptr;
        }

        bool restored = false;
        if (hasPreviousPath && iSetSingleLoadedFile(&_settings, previousPathW) && [self initializeSoundBackend])
        {
            restored = _viewer.Init(nullptr, _renderer, _soundBackend->GetEngine(), &_log, &_timer, ImmPlayer::StereoMode::None, &_settings);
        }

        if (restored)
        {
            _viewerReady = true;
            _firstFrame = true;
            _timeBase = _timer.GetTime();
            _lastTime = 0.0;
            _frameIndex = 0;
            _nativeFrameFailureCount = 0;
            memset(&_events, 0, sizeof(_events));
            _documentVolume = _viewer.GetVolume(0);
            if (_documentVolume <= 0.0f)
            {
                _documentVolume = 1.0f;
            }
            _volumeBeforeMute = _documentVolume;
            _muted = false;
            [_window setTitle:iWindowTitleForSettings(_settings)];
            [_view setPaused:NO];
            [_view setEnableSetNeedsDisplay:NO];
            if (getenv("IMM_METAL_VALIDATE_OPEN_FAILURE_RESTORE"))
            {
                NSLog(@"IMM Metal player open failure restore: restored=1 failedPath=%@", filename);
            }
            [self presentOpenFailureForPath:filename message:[message stringByAppendingString:@"\n\nThe previous IMM file was restored."]];
        }
        else
        {
            if (getenv("IMM_METAL_VALIDATE_OPEN_FAILURE_RESTORE"))
            {
                NSLog(@"IMM Metal player open failure restore: restored=0 failedPath=%@", filename);
            }
            [self presentOpenFailureForPath:filename message:[message stringByAppendingString:@"\n\nThe previous IMM file could not be restored, so the player will close."]];
            [self terminateWithExitCode:1];
        }
        return NO;
    };

    [_view setPaused:YES];
    _viewer.Deinit();
    _viewerReady = false;
    if (_soundBackend)
    {
        _soundBackend->Deinit();
        ImmCore::piDestroySoundEngineBackend(_soundBackend);
        _soundBackend = nullptr;
    }

    const char *forceOpenFailure = getenv("IMM_METAL_FORCE_NEXT_OPEN_FAILURE");
    if (forceOpenFailure && forceOpenFailure[0] && strcmp(forceOpenFailure, "0") != 0)
    {
        unsetenv("IMM_METAL_FORCE_NEXT_OPEN_FAILURE");
        return failReload(@"Forced failed-open restore validation.");
    }

    if (!iSetSingleLoadedFile(&_settings, pathW))
    {
        NSLog(@"IMM Metal player: could not apply opened IMM file path");
        return failReload(@"The selected file path could not be applied to the player settings.");
    }

    if (![self initializeSoundBackend])
    {
        NSLog(@"IMM Metal player: sound backend reinitialization failed for opened IMM file");
        return failReload(@"The audio backend could not be initialized for the selected file.");
    }

    if (!_viewer.Init(nullptr, _renderer, _soundBackend->GetEngine(), &_log, &_timer, ImmPlayer::StereoMode::None, &_settings))
    {
        NSLog(@"IMM Metal player: viewer reinitialization failed for opened IMM file");
        return failReload(@"The selected IMM file could not be loaded.");
    }

    _viewerReady = true;
    _firstFrame = true;
    _timeBase = _timer.GetTime();
    _lastTime = 0.0;
    _frameIndex = 0;
    _nativeFrameFailureCount = 0;
    memset(&_events, 0, sizeof(_events));
    _documentVolume = _viewer.GetVolume(0);
    if (_documentVolume <= 0.0f)
    {
        _documentVolume = 1.0f;
    }
    _volumeBeforeMute = _documentVolume;
    _muted = false;
    _volumeSmokeDone = false;
    _recentDocumentSmokeDone = false;
    _playbackControlSmokeDone = false;
    _openFailureRestoreSmokeDone = false;
    [self runVolumeControlSmokeIfRequested];
    [self noteCurrentDocumentAsRecentWithReason:@"opened-file"];
    [self runRecentDocumentSmokeIfRequested];

    [_window setTitle:iWindowTitleForSettings(_settings)];
    [_window makeKeyAndOrderFront:nil];
    [_window makeFirstResponder:_view];
    if (!_validationEnabled)
    {
        [NSApp activateIgnoringOtherApps:YES];
    }
    [_view setPaused:_validationEnabled ? YES : NO];
    [_view setEnableSetNeedsDisplay:_validationEnabled ? YES : NO];

    NSLog(@"IMM Metal player: opened IMM file %@", filename);
    if (_validationEnabled)
    {
        NSLog(@"IMM Metal validation window title: %@", [_window title]);
    }
    return YES;
}

- (void)applyValidationReloadIfRequested
{
    if (!_validationEnabled ||
        _validationReloadDone ||
        _validationReloadPending ||
        _validationReloadFrame == 0 ||
        _frameIndex < _validationReloadFrame)
    {
        return;
    }

    if (!_viewerReady || !_viewer.IsDocumentLoaded(0))
    {
        return;
    }

    _validationReloadPending = true;
    [self performSelector:@selector(validationReloadContent) withObject:nil afterDelay:0.0];
    NSLog(@"IMM Metal validation reload scheduled: frame=%llu", (unsigned long long)_frameIndex);
}

- (void)validationReloadContent
{
    if (!_validationEnabled || _validationReloadDone)
    {
        return;
    }

    NSString *reloadPath = nil;
    if (_validationReloadPath[0])
    {
        reloadPath = [NSString stringWithUTF8String:_validationReloadPath];
    }

    if (!reloadPath && _settings.mFiles.mLoad.GetLength() < 1)
    {
        NSLog(@"IMM Metal validation failed: reload requested but no loaded IMM path is available");
        _validationDone = true;
        gExitCode = 2;
        return;
    }

    if (!reloadPath)
    {
        const wchar_t *pathW = _settings.mFiles.mLoad[0].GetS();
        if (!pathW || !pathW[0])
        {
            NSLog(@"IMM Metal validation failed: reload requested but loaded IMM path is empty");
            _validationDone = true;
            gExitCode = 2;
            return;
        }

        char pathUtf8[PATH_MAX] = {};
        const size_t pathLen = wcstombs(pathUtf8, pathW, sizeof(pathUtf8) - 1);
        if (pathLen == (size_t)-1)
        {
            NSLog(@"IMM Metal validation failed: reload requested but loaded IMM path is not UTF-8 encodable");
            _validationDone = true;
            gExitCode = 2;
            return;
        }
        pathUtf8[pathLen] = 0;

        reloadPath = [NSString stringWithUTF8String:pathUtf8];
    }
    if (!reloadPath)
    {
        NSLog(@"IMM Metal validation failed: reload requested but loaded IMM path could not create NSString");
        _validationDone = true;
        gExitCode = 2;
        return;
    }

    const uint64_t reloadFrame = _validationReloadFrame;
    _validationReloadDone = true;
    _validationReloadPending = false;
    if (![self loadContentFile:reloadPath allowValidationReload:YES])
    {
        NSLog(@"IMM Metal validation failed: in-process reload failed for %@", reloadPath);
        _validationDone = true;
        gExitCode = 2;
        return;
    }

    NSLog(@"IMM Metal validation reload: frame=%llu path=%@", (unsigned long long)reloadFrame, reloadPath);
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    (void)notification;
    [self performCleanup];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    (void)sender;
    return YES;
}

- (void)performCleanup
{
    if (_didCleanup)
    {
        return;
    }
    _didCleanup = true;
    const bool validationWasEnabled = _validationEnabled;
    const bool validationWasDone = _validationDone;
    if (_renderer)
    {
        if (_viewerReady)
        {
            _viewer.Deinit();
            _viewerReady = false;
        }
        if (_soundBackend)
        {
            _soundBackend->Deinit();
            ImmCore::piDestroySoundEngineBackend(_soundBackend);
            _soundBackend = nullptr;
        }
        _resolve.DeInit(_renderer);
        [self destroyRenderTargetResources];
        if (_vertexArray)
        {
            _renderer->DestroyVertexArray(_vertexArray);
            _vertexArray = nullptr;
        }
        if (_pipelineSanityIndexedVertexArray)
        {
            _renderer->DestroyVertexArray(_pipelineSanityIndexedVertexArray);
            _pipelineSanityIndexedVertexArray = nullptr;
        }
        if (_pipelineSanityIndexBuffer)
        {
            _renderer->DestroyBuffer(_pipelineSanityIndexBuffer);
            _pipelineSanityIndexBuffer = nullptr;
        }
        if (_smokeDisplayBuffer)
        {
            _renderer->DestroyBuffer(_smokeDisplayBuffer);
            _smokeDisplayBuffer = nullptr;
        }
        if (_smokeLayerBuffer)
        {
            _renderer->DestroyBuffer(_smokeLayerBuffer);
            _smokeLayerBuffer = nullptr;
        }
        if (_smokeFrameBuffer)
        {
            _renderer->DestroyBuffer(_smokeFrameBuffer);
            _smokeFrameBuffer = nullptr;
        }
        if (_vertexBuffer)
        {
            _renderer->DestroyBuffer(_vertexBuffer);
            _vertexBuffer = nullptr;
        }
        if (_smokePictureSampler)
        {
            _renderer->DestroySampler(_smokePictureSampler);
            _smokePictureSampler = nullptr;
        }
        if (_smokeBlueNoiseTexture)
        {
            _renderer->DestroyTexture(_smokeBlueNoiseTexture);
            _smokeBlueNoiseTexture = nullptr;
        }
        if (_smokePictureTexture)
        {
            _renderer->DestroyTexture(_smokePictureTexture);
            _smokePictureTexture = nullptr;
        }
        if (_smokeCubemapTexture)
        {
            _renderer->DestroyTexture(_smokeCubemapTexture);
            _smokeCubemapTexture = nullptr;
        }
        if (_shader)
        {
            _renderer->DestroyShader(_shader);
            _shader = nullptr;
        }
        if (_pipelineSanityShader)
        {
            _renderer->DestroyShader(_pipelineSanityShader);
            _pipelineSanityShader = nullptr;
        }
        if (_smokePictureShader)
        {
            _renderer->DestroyShader(_smokePictureShader);
            _smokePictureShader = nullptr;
        }
        if (_smokeCubemapShader)
        {
            _renderer->DestroyShader(_smokeCubemapShader);
            _smokeCubemapShader = nullptr;
        }
        _renderer->Deinitialize();
        delete _renderer;
        _renderer = nullptr;
    }
    if (validationWasEnabled)
    {
        NSLog(@"IMM Metal validation cleanup: done=%d exitCode=%d", validationWasDone ? 1 : 0, gExitCode);
    }
    else
    {
        NSLog(@"IMM Metal player cleanup: exitCode=%d", gExitCode);
    }
    _settings.End();
    _timer.End();
    _log.End();
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
    (void)view;
    if (![self ensureRenderTargetForSize:size])
    {
        NSLog(@"IMM Metal player: resize render texture setup failed");
        [self terminateWithExitCode:1];
    }
}

- (void)drawInMTKView:(MTKView *)view
{
    [self applyValidationResizeIfRequested];

    if (_validationForceNativeFrameFailure)
    {
        [self handleNativeFrameFailure:"forced native frame setup failure"];
        return;
    }

    [self applyValidationReloadIfRequested];
    if (_validationDone)
    {
        if (_exitAfterValidation)
        {
            [self terminateWithExitCode:gExitCode];
        }
        return;
    }

    MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!pass || !drawable)
    {
        [self handleNativeFrameFailure:(!pass ? "missing render pass descriptor" : "missing drawable")];
        return;
    }

    if (![self ensureRenderTargetForSize:view.drawableSize])
    {
        return;
    }

    if (!_renderer->BeginNativeFrame((__bridge void *)pass, (__bridge void *)drawable))
    {
        [self handleNativeFrameFailure:"BeginNativeFrame returned false"];
        return;
    }
    _nativeFrameFailureCount = 0;

    const float clearColor[4] = { 0.08f, 0.10f, 0.13f, 1.0f };
    bool exitAfterSmokeFrame = false;
    if (_viewerReady)
    {
        if (_validationHelperDraws && !_validationPipelineSanityDone)
        {
            if (![self runValidationPipelineSanity])
            {
                if (_validationEnabled)
                {
                    _validationDone = true;
                    gExitCode = 2;
                }
                return;
            }
            if (![self runValidationShaderPathSanity])
            {
                if (_validationEnabled)
                {
                    _validationDone = true;
                    gExitCode = 2;
                }
                return;
            }
        }

        if (_validationHelperDraws && !_validationHelperDrawsDone)
        {
            const int drawableWidth = (int)view.drawableSize.width;
            const int drawableHeight = (int)view.drawableSize.height;
            const int nativeViewport[4] = { 0, 0, drawableWidth, drawableHeight };
            _renderer->SetRenderTarget(nullptr);
            _renderer->SetViewport(0, nativeViewport);
            const uint64_t validationTextureHandle = _renderer->GetTextureHandle(_colorTexture);
            _renderer->MakeResident(_colorTexture);
            _renderer->MakeNonResident(_colorTexture);
            _renderer->SetPointSize(false, 1.0f);
            _renderer->SetLineWidth(1.0f);
            _renderer->RenderMemoryBarrier(ImmCore::piRenderer::BarrierType::ALL);
            _renderer->AttachShader(_shader);
            _renderer->StartPerformanceMeasure();
            _renderer->DrawUnitQuad_XY(1);
            _renderer->SetBlending(0,
                                   ImmCore::piRenderer::BlendEquation::piBLEND_ADD,
                                   ImmCore::piRenderer::BlendOperations::piBLEND_SRC_ALPHA,
                                   ImmCore::piRenderer::BlendOperations::piBLEND_ONE_MINUS_SRC_ALPHA,
                                   ImmCore::piRenderer::BlendEquation::piBLEND_ADD,
                                   ImmCore::piRenderer::BlendOperations::piBLEND_ONE,
                                   ImmCore::piRenderer::BlendOperations::piBLEND_ONE_MINUS_SRC_ALPHA);
            _renderer->DrawUnitQuad_XY(1);
            _renderer->SetBlending(0,
                                   ImmCore::piRenderer::BlendEquation::piBLEND_ADD,
                                   ImmCore::piRenderer::BlendOperations::piBLEND_ONE,
                                   ImmCore::piRenderer::BlendOperations::piBLEND_ZERO,
                                   ImmCore::piRenderer::BlendEquation::piBLEND_ADD,
                                   ImmCore::piRenderer::BlendOperations::piBLEND_ONE,
                                   ImmCore::piRenderer::BlendOperations::piBLEND_ZERO);
            const ImmCore::piDrawArraysIndirectCommand indirectQuad = { 4, 1, 0, 0 };
            ImmCore::piBuffer indirectQuadBuffer = _renderer->CreateBuffer(&indirectQuad,
                                                                           sizeof(indirectQuad),
                                                                           ImmCore::piRenderer::BufferType::Static,
                                                                           ImmCore::piRenderer::BufferUse::DrawCommands);
            if (indirectQuadBuffer)
            {
                _renderer->DrawPrimitiveNotIndexedIndirect(ImmCore::piRenderer::PrimitiveType::TriangleStrip,
                                                           indirectQuadBuffer,
                                                           1);
                _renderer->DestroyBuffer(indirectQuadBuffer);
            }
            const uint16_t indexedTriangleIndices[3] = { 0, 1, 2 };
            ImmCore::piBuffer indexedTriangleIndexBuffer = _renderer->CreateBuffer(indexedTriangleIndices,
                                                                                   sizeof(indexedTriangleIndices),
                                                                                   ImmCore::piRenderer::BufferType::Static,
                                                                                   ImmCore::piRenderer::BufferUse::Index);
            ImmCore::piVertexArray indexedTriangleVertexArray = nullptr;
            if (indexedTriangleIndexBuffer)
            {
                ImmCore::piRArrayLayout indexedLayout = {};
                indexedLayout.mStride = sizeof(ImmMetalDebugVertex);
                indexedLayout.mNumElements = 2;
                indexedLayout.mEntry[0].mNumComponents = 2;
                indexedLayout.mEntry[0].mType = ImmCore::piRArrayType_Float;
                indexedLayout.mEntry[1].mNumComponents = 4;
                indexedLayout.mEntry[1].mType = ImmCore::piRArrayType_Float;
                indexedTriangleVertexArray = _renderer->CreateVertexArray(1,
                                                                           _vertexBuffer,
                                                                           &indexedLayout,
                                                                           nullptr,
                                                                           nullptr,
                                                                           indexedTriangleIndexBuffer,
                                                                           ImmCore::piRenderer::IndexArrayFormat::UINT_16);
            }
            if (indexedTriangleVertexArray)
            {
                const ImmCore::piDrawElementsIndirectCommand indexedTriangle = { 3, 1, 0, 0, 0 };
                ImmCore::piBuffer indexedTriangleCommandBuffer = _renderer->CreateBuffer(&indexedTriangle,
                                                                                         sizeof(indexedTriangle),
                                                                                         ImmCore::piRenderer::BufferType::Static,
                                                                                         ImmCore::piRenderer::BufferUse::DrawCommands);
                _renderer->AttachVertexArray(indexedTriangleVertexArray);
                if (indexedTriangleCommandBuffer)
                {
                    _renderer->DrawPrimitiveIndirect(ImmCore::piRenderer::PrimitiveType::Triangle,
                                                     indexedTriangleCommandBuffer,
                                                     0,
                                                     1);
                    _renderer->DestroyBuffer(indexedTriangleCommandBuffer);
                }
                _renderer->DettachVertexArray();
                _renderer->DestroyVertexArray(indexedTriangleVertexArray);
            }
            if (indexedTriangleIndexBuffer)
            {
                _renderer->DestroyBuffer(indexedTriangleIndexBuffer);
            }
            _renderer->DrawUnitCube_XYZ(1);
            _renderer->DrawUnitCube_XYZ_NOR(1);
            _renderer->EndPerformanceMeasure();
            const uint64_t helperDrawTimeNs = _renderer->GetPerformanceMeasure();
            _renderer->DettachShader();
            _validationHelperDrawsDone = true;
            NSLog(@"IMM Metal helper validation: unitQuad=1 blendQuad=1 indirectQuad=1 indirectIndexedTriangle=1 unitCubeXYZ=1 unitCubeXYZNOR=1 fixedStateHints=1 memoryBarrier=1 drawable=%dx%d textureHandle=%llu timingNs=%llu",
                  drawableWidth,
                  drawableHeight,
                  (unsigned long long)validationTextureHandle,
                  (unsigned long long)helperDrawTimeNs);
            _renderer->Report();
        }

        const double now = _timer.GetTime() - _timeBase;
        const float dt = (float)(now - _lastTime);
        _lastTime = now;

        const ImmCore::trans3d head = ImmCore::trans3d::identity();
        _viewer.GlobalWork(&_events, false, head, nullptr, nullptr, &_log, dt, _renderSize, true, 9000, _firstFrame);
        [self clearTransientInput];

        const int vp[4] = { 0, 0, _renderSize.x, _renderSize.y };
        const float offscreenClear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        _renderer->SetRenderTarget(_renderTarget);
        _renderer->SetViewport(0, vp);
        _renderer->SetWriteMask(true, false, false, false, true);
        _renderer->Clear(offscreenClear, nullptr, nullptr, nullptr, true);
        _viewer.GlobalRender(head, ImmCore::vec4(0.0f));
        _viewer.RenderMono(_renderSize, head, 0);
        [self validateOffscreenFrameIfRequested];

        _resolve.Do(_renderer, nullptr, vp, 0, 1.0f, _colorTexture);
        _soundBackend->Tick();
        ++_frameIndex;
        if (!_validationEnabled && _smokeExitAfterSeconds > 0.0 && now >= _smokeExitAfterSeconds)
        {
            [self runPlaybackControlSmokeIfRequested];
            [self runOpenFailureRestoreSmokeIfRequested];
            NSLog(@"IMM Metal interactive smoke: elapsed=%.3f seconds, exiting", now);
            exitAfterSmokeFrame = true;
        }
    }
    else
    {
        _renderer->Clear(clearColor, nullptr, nullptr, nullptr, true);
        _renderer->AttachShader(_shader);
        _renderer->AttachVertexArray(_vertexArray);
        _renderer->DrawPrimitiveNotIndexed(ImmCore::piRenderer::PrimitiveType::Triangle, 0, 3, 1);
        _renderer->DettachVertexArray();
        _renderer->DettachShader();
    }
    _renderer->EndNativeFrame();

    if (_exitAfterValidation && _validationDone)
    {
        [self terminateWithExitCode:gExitCode];
    }
    else if (exitAfterSmokeFrame)
    {
        [self terminateWithExitCode:gExitCode];
    }
}

- (void)validationDrawTick
{
    if (!_validationEnabled || _validationDone || !_view)
    {
        return;
    }

    [_view draw];

    if (!_validationDone)
    {
        [self performSelector:@selector(validationDrawTick) withObject:nil afterDelay:(1.0 / 60.0)];
    }
}

- (void)interactiveSmokeDrawTick
{
    if (_validationEnabled || _didCleanup || !_view)
    {
        return;
    }

    [_view draw];

    if (!_didCleanup)
    {
        [self performSelector:@selector(interactiveSmokeDrawTick) withObject:nil afterDelay:(1.0 / 60.0)];
    }
}

@end

int main(int argc, char **argv)
{
    @autoreleasepool
    {
        iUseBundledSettingsIfAvailable();
    }

    for (int i = 1; i < argc; ++i)
    {
        if (iHasExtension(argv[i], ".json"))
        {
            if (!iSetSettingsPath(argv[i]))
            {
                return 1;
            }
        }
        else if (!iAddContentPath(argv[i]))
        {
            fprintf(stderr, "appImmViewerMetal accepts at most one command-line IMM content path; use a settings JSON for multiple File.Load entries.\n");
            return 1;
        }
    }

    @autoreleasepool
    {
        NSApplication *app = [NSApplication sharedApplication];
        if (!iIsValidationRequested())
        {
            [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        }
        iInstallApplicationMenu();
        ImmMetalPlayerDelegate *delegate = [[ImmMetalPlayerDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }

    return gExitCode;
}
