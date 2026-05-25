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

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <wchar.h>
#include <math.h>

using namespace ImmCore;

static wchar_t gSettingsPath[PATH_MAX] = L"code/appImmViewer/exe/settings.json";
static wchar_t gContentPath[PATH_MAX] = {};
static int gContentPathCount = 0;
static int gExitCode = 0;

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
    [fileMenuItem setSubmenu:fileMenu];

    [NSApp setMainMenu:mainMenu];
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
    ImmCore::piBuffer _vertexBuffer;
    ImmCore::piBuffer _pipelineSanityIndexBuffer;
    ImmCore::piVertexArray _pipelineSanityIndexedVertexArray;
    ImmCore::piVertexArray _vertexArray;
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
    uint64_t _frameIndex;
    int _nativeFrameFailureCount;
    uint64_t _validationFrame;
    uint64_t _validationMaxFrame;
    uint64_t _validationMinNonZeroPixels;
    uint64_t _validationMinDrawCalls;
    uint64_t _validationMinPictureDrawCalls;
    uint64_t _validationMinPicture360DrawCalls;
    uint64_t _validationMinTriangles;
    uint64_t _validationResizeFrame;
    int _validationResizeWidth;
    int _validationResizeHeight;
    char _validationCapturePath[PATH_MAX];
    bool _viewerReady;
    bool _firstFrame;
    bool _validationEnabled;
    bool _validationDone;
    bool _exitAfterValidation;
    bool _validationResizeDone;
    bool _validationHelperDraws;
    bool _validationHelperDrawsDone;
    bool _validationPipelineSanityDone;
    bool _validationForceNativeFrameFailure;
    bool _didCleanup;
    bool _isStopping;
}
- (void)handleKeyEvent:(NSEvent *)event down:(BOOL)down;
- (void)handleFlagsChanged:(NSEvent *)event;
- (void)handleMouseEvent:(NSEvent *)event button:(int)button down:(BOOL)down dragged:(BOOL)dragged;
- (void)handleScrollEvent:(NSEvent *)event;
- (CGSize)initialDrawableSizeForView:(MTKView *)view fallbackFrame:(NSRect)frame;
- (void)handleNativeFrameFailure:(const char *)reason;
- (BOOL)runValidationPipelineSanity;
- (IBAction)openDocument:(id)sender;
- (BOOL)loadContentFile:(NSString *)filename;
- (void)performCleanup;
- (void)terminateWithExitCode:(int)exitCode;
- (void)validationDrawTick;
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
    _validationMinPicture360DrawCalls = 0;
    _validationMinTriangles = 0;
    _validationResizeFrame = 0;
    _validationResizeWidth = 0;
    _validationResizeHeight = 0;
    _validationCapturePath[0] = 0;
    _exitAfterValidation = false;
    _validationDone = false;
    _validationResizeDone = false;
    const char *helperDraws = getenv("IMM_METAL_VALIDATE_HELPER_DRAWS");
    _validationHelperDraws = helperDraws && helperDraws[0] && strcmp(helperDraws, "0") != 0;
    _validationHelperDrawsDone = false;
    _validationPipelineSanityDone = false;
    const char *forceNativeFrameFailure = getenv("IMM_METAL_VALIDATE_FORCE_NATIVE_FRAME_FAILURE");
    _validationForceNativeFrameFailure = forceNativeFrameFailure &&
                                         forceNativeFrameFailure[0] &&
                                         strcmp(forceNativeFrameFailure, "0") != 0;

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
    const char *minPicture360DrawCalls = getenv("IMM_METAL_VALIDATE_MIN_PICTURE360_DRAWCALLS");
    if (minPicture360DrawCalls && minPicture360DrawCalls[0])
    {
        _validationMinPicture360DrawCalls = strtoull(minPicture360DrawCalls, nullptr, 10);
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
        (uint64_t)perf.numPicture360DrawCalls >= _validationMinPicture360DrawCalls &&
        (uint64_t)perf.numTriangles >= _validationMinTriangles;

    if (!passed)
    {
        if (_frameIndex < _validationMaxFrame)
        {
            free(pixels);
            return;
        }
        _validationDone = true;
        NSLog(@"IMM Metal validation failed: frame=%llu pixels=%zu nonZero=%llu minNonZero=%llu hash=%llu drawCalls=%d minDrawCalls=%llu paintDrawCalls=%d pictureDrawCalls=%d minPictureDrawCalls=%llu picture2DDrawCalls=%d picture360DrawCalls=%d minPicture360DrawCalls=%llu modelDrawCalls=%d triangles=%d minTriangles=%llu culledCalls=%d",
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
              perf.numPicture360DrawCalls,
              (unsigned long long)_validationMinPicture360DrawCalls,
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
    NSLog(@"IMM Metal validation: frame=%llu pixels=%zu nonZero=%llu hash=%llu drawCalls=%d paintDrawCalls=%d pictureDrawCalls=%d picture2DDrawCalls=%d picture360DrawCalls=%d modelDrawCalls=%d triangles=%d culledCalls=%d",
          (unsigned long long)_frameIndex,
          pixelCount,
          (unsigned long long)nonZeroPixels,
          (unsigned long long)hash,
          perf.numDrawCalls,
          perf.numPaintDrawCalls,
          perf.numPictureDrawCalls,
          perf.numPicture2DDrawCalls,
          perf.numPicture360DrawCalls,
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

    _soundBackend = ImmCore::piCreateSoundEngineBackend(ImmCore::piSoundEngineBackend::API::Null, &_log);
    if (!_soundBackend || !_soundBackend->Init(nullptr, -1, nullptr))
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

- (BOOL)loadContentFile:(NSString *)filename
{
    if (!filename || !_viewerReady || _validationEnabled)
    {
        return NO;
    }

    const char *path = [filename fileSystemRepresentation];
    if (!path || !path[0])
    {
        NSLog(@"IMM Metal player: invalid opened IMM file path");
        return NO;
    }

    wchar_t pathW[PATH_MAX] = {};
    const size_t pathLen = mbstowcs(pathW, path, PATH_MAX - 1);
    if (pathLen == (size_t)-1)
    {
        NSLog(@"IMM Metal player: invalid opened IMM file path encoding");
        return NO;
    }
    pathW[pathLen] = 0;

    [_view setPaused:YES];
    _viewer.Deinit();
    _viewerReady = false;

    if (!iSetSingleLoadedFile(&_settings, pathW))
    {
        NSLog(@"IMM Metal player: could not apply opened IMM file path");
        [self terminateWithExitCode:1];
        return NO;
    }

    if (!_viewer.Init(nullptr, _renderer, _soundBackend->GetEngine(), &_log, &_timer, ImmPlayer::StereoMode::None, &_settings))
    {
        NSLog(@"IMM Metal player: viewer reinitialization failed for opened IMM file");
        [self terminateWithExitCode:1];
        return NO;
    }

    _viewerReady = true;
    _firstFrame = true;
    _timeBase = _timer.GetTime();
    _lastTime = 0.0;
    _frameIndex = 0;
    _nativeFrameFailureCount = 0;
    memset(&_events, 0, sizeof(_events));

    [_window setTitle:iWindowTitleForSettings(_settings)];
    [_window makeKeyAndOrderFront:nil];
    [_window makeFirstResponder:_view];
    [NSApp activateIgnoringOtherApps:YES];
    [_view setPaused:NO];

    NSLog(@"IMM Metal player: opened IMM file %@", filename);
    return YES;
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
        if (_vertexBuffer)
        {
            _renderer->DestroyBuffer(_vertexBuffer);
            _vertexBuffer = nullptr;
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
        _renderer->Deinitialize();
        delete _renderer;
        _renderer = nullptr;
    }
    if (validationWasEnabled)
    {
        NSLog(@"IMM Metal validation cleanup: done=%d exitCode=%d", validationWasDone ? 1 : 0, gExitCode);
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
