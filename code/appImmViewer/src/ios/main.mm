#import <AVFoundation/AVFoundation.h>
#import <MetalKit/MetalKit.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "appImmViewer/src/apple/metal_player_core.h"

#include <limits.h>
#include <wchar.h>

static NSString *const ImmIOSValidationArgument = @"--imm-ios-validation";
static NSString *const ImmIOSLifecycleArgument = @"--imm-ios-lifecycle-smoke";

static bool ImmToWide(NSString *value, wchar_t *destination, size_t capacity)
{
    if (!value || !destination || capacity == 0)
        return false;
    const char *utf8 = value.fileSystemRepresentation;
    const size_t length = mbstowcs(destination, utf8, capacity - 1);
    if (length == (size_t)-1)
        return false;
    destination[length] = 0;
    return true;
}

static BOOL ImmHasArgument(NSString *argument)
{
    return [NSProcessInfo.processInfo.arguments containsObject:argument];
}

@interface ImmIOSViewController : UIViewController <MTKViewDelegate, UIDocumentPickerDelegate>
- (BOOL)openDocumentURL:(NSURL *)url;
@end

@implementation ImmIOSViewController
{
    MTKView *_metalView;
    ExePlayer::MetalPlayerCore _player;
    CGPoint _lastPanTranslation;
    CGFloat _lastPinchScale;
    BOOL _validationRequested;
    BOOL _lifecycleSmokeRequested;
    BOOL _lifecycleSuspended;
    BOOL _lifecycleResumed;
    BOOL _validationFinished;
}

- (void)loadView
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    _metalView = [[MTKView alloc] initWithFrame:UIScreen.mainScreen.bounds device:device];
    _metalView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    _metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
    _metalView.clearColor = MTLClearColorMake(0.08, 0.10, 0.13, 1.0);
    _metalView.preferredFramesPerSecond = 60;
    _metalView.autoResizeDrawable = YES;
    _metalView.delegate = self;
    self.view = _metalView;

    UIPanGestureRecognizer *pan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(handlePan:)];
    UIPinchGestureRecognizer *pinch = [[UIPinchGestureRecognizer alloc] initWithTarget:self action:@selector(handlePinch:)];
    UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleTap:)];
    [_metalView addGestureRecognizer:pan];
    [_metalView addGestureRecognizer:pinch];
    [_metalView addGestureRecognizer:tap];

    UIBarButtonItem *open = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFolder
                                                                         target:self
                                                                         action:@selector(openDocumentPicker:)];
    self.navigationItem.rightBarButtonItem = open;
    self.navigationItem.title = @"IMM Viewer";

    NSString *settingsPath = [NSBundle.mainBundle pathForResource:@"appImmViewerIOS-settings" ofType:@"json"];
    NSString *contentPath = [NSBundle.mainBundle pathForResource:@"sample1" ofType:@"imm"];
    NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *logPath = [documents stringByAppendingPathComponent:@"standalone-ios.log"];
    wchar_t settingsWide[PATH_MAX] = {};
    wchar_t contentWide[PATH_MAX] = {};
    wchar_t logWide[PATH_MAX] = {};
    if (!device ||
        !ImmToWide(settingsPath, settingsWide, PATH_MAX) ||
        !ImmToWide(contentPath, contentWide, PATH_MAX) ||
        !ImmToWide(logPath, logWide, PATH_MAX) ||
        !_player.Initialize((__bridge void *)device,
                            settingsWide,
                            contentWide,
                            logWide,
                            NSTemporaryDirectory().fileSystemRepresentation))
    {
        NSLog(@"IMM_IOS_STANDALONE phase=startup status=failed");
        return;
    }
    _validationRequested = ImmHasArgument(ImmIOSValidationArgument);
    _lifecycleSmokeRequested = ImmHasArgument(ImmIOSLifecycleArgument);
    if (_validationRequested)
    {
        _metalView.autoResizeDrawable = NO;
        _metalView.drawableSize = CGSizeMake(1280, 720);
        _player.Resize(1280, 720);
    }
    NSLog(@"IMM_IOS_STANDALONE phase=startup status=passed document=sample1.imm renderer=Metal");

    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    [center addObserver:self selector:@selector(applicationWillResignActive:) name:UIApplicationWillResignActiveNotification object:nil];
    [center addObserver:self selector:@selector(applicationDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:nil];
    [center addObserver:self selector:@selector(audioInterrupted:) name:AVAudioSessionInterruptionNotification object:nil];
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
    _player.Shutdown();
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    [self setNeedsUpdateOfSupportedInterfaceOrientations];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskLandscape;
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
    if (_player.Resize((int)size.width, (int)size.height))
        NSLog(@"IMM_IOS_STANDALONE phase=resize status=passed width=%d height=%d", (int)size.width, (int)size.height);
}

- (void)drawInMTKView:(MTKView *)view
{
    if (!_player.IsReady())
        return;
    if (_player.RenderSize().x == 0)
        _player.Resize((int)view.drawableSize.width, (int)view.drawableSize.height);
    MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!_player.Draw((__bridge void *)pass, (__bridge void *)drawable))
        return;

    if (_lifecycleSmokeRequested && !_lifecycleSuspended && _player.FrameIndex() >= 45)
    {
        _lifecycleSuspended = YES;
        _player.Suspend();
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.15 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            self->_player.Resume();
            self->_lifecycleResumed = YES;
            NSLog(@"IMM_IOS_STANDALONE phase=lifecycle status=passed transition=suspend-resume");
        });
        return;
    }

    if (_validationRequested && !_validationFinished && _player.FrameIndex() >= 120 &&
        (!_lifecycleSmokeRequested || _lifecycleResumed))
    {
        NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
        NSString *capturePath = [documents stringByAppendingPathComponent:@"standalone-ios-metal-render.png"];
        NSString *resultPath = [documents stringByAppendingPathComponent:@"standalone-ios-result.log"];
        wchar_t captureWide[PATH_MAX] = {};
        const BOOL captured = ImmToWide(capturePath, captureWide, PATH_MAX) && _player.WriteCapture(captureWide);
        const BOOL loaded = _player.IsDocumentLoaded();
        NSString *result = [NSString stringWithFormat:@"phase=render status=%@\nphase=document status=%@ path=sample1.imm\nphase=lifecycle status=%@\nframes=%llu\n",
                            captured ? @"passed" : @"failed",
                            loaded ? @"passed" : @"failed",
                            (!_lifecycleSmokeRequested || _lifecycleResumed) ? @"passed" : @"failed",
                            (unsigned long long)_player.FrameIndex()];
        [result writeToFile:resultPath atomically:YES encoding:NSUTF8StringEncoding error:nil];
        _validationFinished = YES;
        NSLog(@"IMM_IOS_STANDALONE phase=validation status=%@", captured && loaded ? @"passed" : @"failed");
    }
}

- (void)applicationWillResignActive:(NSNotification *)notification
{
    (void)notification;
    _player.Suspend();
    _metalView.paused = YES;
    NSLog(@"IMM_IOS_STANDALONE phase=lifecycle transition=resign-active");
}

- (void)applicationDidBecomeActive:(NSNotification *)notification
{
    (void)notification;
    _player.Resume();
    _metalView.paused = NO;
    NSLog(@"IMM_IOS_STANDALONE phase=lifecycle transition=became-active");
}

- (void)audioInterrupted:(NSNotification *)notification
{
    NSNumber *type = notification.userInfo[AVAudioSessionInterruptionTypeKey];
    if (type.unsignedIntegerValue == AVAudioSessionInterruptionTypeBegan)
        _player.Suspend();
    else
        _player.Resume();
    NSLog(@"IMM_IOS_STANDALONE phase=audio-interruption type=%@", type);
}

- (void)handlePan:(UIPanGestureRecognizer *)gesture
{
    CGPoint translation = [gesture translationInView:_metalView];
    if (gesture.state == UIGestureRecognizerStateBegan)
        _lastPanTranslation = translation;
    CGPoint delta = CGPointMake(translation.x - _lastPanTranslation.x, translation.y - _lastPanTranslation.y);
    _lastPanTranslation = translation;
    _player.Rotate((float)(delta.x * 0.004), (float)(delta.y * 0.004));
}

- (void)handlePinch:(UIPinchGestureRecognizer *)gesture
{
    if (gesture.state == UIGestureRecognizerStateBegan)
        _lastPinchScale = gesture.scale;
    const CGFloat delta = gesture.scale - _lastPinchScale;
    _lastPinchScale = gesture.scale;
    _player.MoveForward((float)(delta * 0.5));
}

- (void)handleTap:(UITapGestureRecognizer *)gesture
{
    if (gesture.state == UIGestureRecognizerStateRecognized)
        _player.TogglePlayback();
}

- (void)openDocumentPicker:(id)sender
{
    (void)sender;
    UTType *immType = [UTType typeWithFilenameExtension:@"imm" conformingToType:UTTypeData];
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[immType ?: UTTypeData] asCopy:YES];
    picker.delegate = self;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
    (void)controller;
    if (urls.count > 0)
        [self openDocumentURL:urls.firstObject];
}

- (BOOL)openDocumentURL:(NSURL *)url
{
    if (!url.isFileURL || ![url.pathExtension.lowercaseString isEqualToString:@"imm"])
        return NO;
    NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *destination = [documents stringByAppendingPathComponent:url.lastPathComponent];
    [NSFileManager.defaultManager removeItemAtPath:destination error:nil];
    NSError *error = nil;
    if (![NSFileManager.defaultManager copyItemAtPath:url.path toPath:destination error:&error])
    {
        NSLog(@"IMM_IOS_STANDALONE phase=file-open status=failed error=%@", error);
        return NO;
    }
    wchar_t pathWide[PATH_MAX] = {};
    const BOOL loaded = ImmToWide(destination, pathWide, PATH_MAX) && _player.LoadDocument(pathWide);
    NSLog(@"IMM_IOS_STANDALONE phase=file-open status=%@ path=%@", loaded ? @"passed" : @"failed", destination.lastPathComponent);
    return loaded;
}

@end

@interface ImmIOSAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@property(nonatomic, strong) ImmIOSViewController *viewer;
@end

@implementation ImmIOSAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    (void)application;
    (void)launchOptions;
    AVAudioSession *session = AVAudioSession.sharedInstance;
    NSError *audioError = nil;
    [session setCategory:AVAudioSessionCategoryPlayback mode:AVAudioSessionModeDefault options:0 error:&audioError];
    [session setActive:YES error:&audioError];
    if (audioError)
        NSLog(@"IMM_IOS_STANDALONE phase=audio-session status=failed error=%@", audioError);
    else
        NSLog(@"IMM_IOS_STANDALONE phase=audio-session status=passed backend=AVFoundation");

    self.viewer = [[ImmIOSViewController alloc] init];
    UINavigationController *navigation = [[UINavigationController alloc] initWithRootViewController:self.viewer];
    navigation.navigationBar.translucent = YES;
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = navigation;
    [self.window makeKeyAndVisible];
    return YES;
}

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options
{
    (void)application;
    (void)options;
    return [self.viewer openDocumentURL:url];
}

@end

int main(int argc, char *argv[])
{
    @autoreleasepool
    {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(ImmIOSAppDelegate.class));
    }
}
