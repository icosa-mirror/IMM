#import <UIKit/UIKit.h>

@interface IMMControlAppDelegate : UIResponder <UIApplicationDelegate>
@property(strong, nonatomic) UIWindow *window;
@end

@implementation IMMControlAppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    NSString *marker = [NSTemporaryDirectory() stringByAppendingPathComponent:@"imm-ios-on-mac-control.started"];
    [@"IMM_IOS_ON_MAC_CONTROL_STARTED\n" writeToFile:marker
                                         atomically:YES
                                           encoding:NSUTF8StringEncoding
                                              error:nil];
    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    UIViewController *controller = [[UIViewController alloc] init];
    controller.view.backgroundColor = [UIColor colorWithRed:0.08 green:0.55 blue:0.22 alpha:1.0];
    self.window.rootViewController = controller;
    [self.window makeKeyAndVisible];
    return YES;
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([IMMControlAppDelegate class]));
    }
}
