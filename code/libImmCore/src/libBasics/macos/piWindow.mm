//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#import <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "../piWindow.h"

typedef struct _iMacWindowMgr iMacWindowMgr;

typedef struct
{
    NSWindow *window;
    NSView *view;
    NSOpenGLContext *context;
    CGLContextObj cglContext;
    int full;
    ImmCore::piWindowEvents eventinfo;
    int mXres;
    int mYres;
    int exitReq;
    iMacWindowMgr *mMgr;
} iMacWindow;

typedef struct _iMacWindowMgr
{
    NSApplication *app;
    int mNumWindows;
    iMacWindow *mWindows[256];
    ImmCore::piWindowEvents mEventinfo;
} iMacWindowMgr;

static int iMacKeyToPi(NSEvent *event)
{
    const unsigned short keyCode = [event keyCode];
    switch (keyCode)
    {
        case 123: return KEY_LEFT;
        case 124: return KEY_RIGHT;
        case 126: return KEY_UP;
        case 125: return KEY_DOWN;
        case 116: return KEY_PGUP;
        case 121: return KEY_PGDOWN;
        case 36: return KEY_ENTER;
        case 51: return KEY_BACK;
        case 115: return KEY_HOME;
        case 119: return KEY_END;
        case 117: return KEY_DELETE;
        case 48: return KEY_TAB;
        case 122: return KEY_F1;
        case 120: return KEY_F2;
        case 99: return KEY_F3;
        case 118: return KEY_F4;
        case 96: return KEY_F5;
        case 97: return KEY_F6;
        case 98: return KEY_F7;
        case 100: return KEY_F8;
        case 101: return KEY_F9;
        case 109: return KEY_F10;
        case 103: return KEY_F11;
        case 111: return KEY_F12;
        default: break;
    }

    NSString *chars = [event charactersIgnoringModifiers];
    if ([chars length] > 0)
    {
        unichar c = [chars characterAtIndex:0];
        if (c >= 32 && c < 128)
            return (int)c;
    }

    return (int)keyCode;
}

static void iUpdateModifiers(iMacWindow *me, NSEvent *event)
{
    const NSEventModifierFlags flags = [event modifierFlags];
    const int shift = (flags & NSEventModifierFlagShift) ? 1 : 0;
    const int ctrl = (flags & NSEventModifierFlagControl) ? 1 : 0;
    const int alt = (flags & NSEventModifierFlagOption) ? 1 : 0;

    me->eventinfo.keyb.state[KEY_SHIFT] = shift;
    me->eventinfo.keyb.state[KEY_CONTROL] = ctrl;
    me->eventinfo.keyb.state[KEY_ALT] = alt;

    me->eventinfo.keyb.state[KEY_LSHIFT] = shift;
    me->eventinfo.keyb.state[KEY_RSHIFT] = shift;
    me->eventinfo.keyb.state[KEY_LCONTROL] = ctrl;
    me->eventinfo.keyb.state[KEY_RCONTROL] = ctrl;
    me->eventinfo.keyb.state[KEY_LALT] = alt;
    me->eventinfo.keyb.state[KEY_RALT] = alt;
}

@interface ImmWindowDelegate : NSObject<NSWindowDelegate>
{
    iMacWindow *mOwner;
}
- (id)initWithOwner:(iMacWindow*)owner;
@end

@implementation ImmWindowDelegate
- (id)initWithOwner:(iMacWindow*)owner
{
    self = [super init];
    if (self) mOwner = owner;
    return self;
}

- (BOOL)windowShouldClose:(id)sender
{
    (void)sender;
    if (mOwner) mOwner->exitReq = 1;
    return YES;
}
@end

@interface ImmWindowView : NSOpenGLView
{
    iMacWindow *mOwner;
}
- (id)initWithFrame:(NSRect)frame pixelFormat:(NSOpenGLPixelFormat*)format owner:(iMacWindow*)owner;
@end

@implementation ImmWindowView
- (id)initWithFrame:(NSRect)frame pixelFormat:(NSOpenGLPixelFormat*)format owner:(iMacWindow*)owner
{
    self = [super initWithFrame:frame pixelFormat:format];
    if (self) mOwner = owner;
    return self;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)keyDown:(NSEvent *)event
{
    if (!mOwner) return;
    const int key = iMacKeyToPi(event);
    if (key >= 0 && key < 256)
    {
        mOwner->eventinfo.keyb.state[key] = 1;
        mOwner->eventinfo.keyb.key[key] = 1;
        if (mOwner->eventinfo.keyb.queueLen < 1024)
            mOwner->eventinfo.keyb.queue[mOwner->eventinfo.keyb.queueLen++] = key;

        mOwner->mMgr->mEventinfo.keyb.state[key] = 1;
        mOwner->mMgr->mEventinfo.keyb.key[key] = 1;
    }
    iUpdateModifiers(mOwner, event);
}

- (void)keyUp:(NSEvent *)event
{
    if (!mOwner) return;
    const int key = iMacKeyToPi(event);
    if (key >= 0 && key < 256)
    {
        mOwner->eventinfo.keyb.state[key] = 0;
        mOwner->mMgr->mEventinfo.keyb.state[key] = 0;
    }
    iUpdateModifiers(mOwner, event);
}

- (void)flagsChanged:(NSEvent *)event
{
    if (!mOwner) return;
    iUpdateModifiers(mOwner, event);
}

static void iUpdateMousePos(iMacWindow *me, NSEvent *event)
{
    NSPoint p = [event locationInWindow];
    NSView *view = me->view;
    if (view)
        p = [view convertPoint:p fromView:nil];
    const int height = (int)[view bounds].size.height;
    const int x = (int)p.x;
    const int y = height - (int)p.y;
    me->eventinfo.mouse.x = x;
    me->eventinfo.mouse.y = y;
    me->mMgr->mEventinfo.mouse.x = x;
    me->mMgr->mEventinfo.mouse.y = y;
}

- (void)mouseDown:(NSEvent *)event
{
    if (!mOwner) return;
    iUpdateMousePos(mOwner, event);
    mOwner->eventinfo.mouse.lb_isDown = 1;
    mOwner->eventinfo.mouse.ox = mOwner->eventinfo.mouse.x;
    mOwner->eventinfo.mouse.oy = mOwner->eventinfo.mouse.y;
    mOwner->mMgr->mEventinfo.mouse.lb_isDown = 1;
    mOwner->mMgr->mEventinfo.mouse.ox = mOwner->mMgr->mEventinfo.mouse.x;
    mOwner->mMgr->mEventinfo.mouse.oy = mOwner->mMgr->mEventinfo.mouse.y;
}

- (void)mouseUp:(NSEvent *)event
{
    if (!mOwner) return;
    iUpdateMousePos(mOwner, event);
    mOwner->eventinfo.mouse.lb_isDown = 0;
    mOwner->eventinfo.mouse.ox = -1;
    mOwner->eventinfo.mouse.oy = -1;
    mOwner->mMgr->mEventinfo.mouse.lb_isDown = 0;
    mOwner->mMgr->mEventinfo.mouse.ox = -1;
    mOwner->mMgr->mEventinfo.mouse.oy = -1;
}

- (void)rightMouseDown:(NSEvent *)event
{
    if (!mOwner) return;
    iUpdateMousePos(mOwner, event);
    mOwner->eventinfo.mouse.rb_isDown = 1;
    mOwner->eventinfo.mouse.ox = mOwner->eventinfo.mouse.x;
    mOwner->eventinfo.mouse.oy = mOwner->eventinfo.mouse.y;
    mOwner->mMgr->mEventinfo.mouse.rb_isDown = 1;
    mOwner->mMgr->mEventinfo.mouse.ox = mOwner->mMgr->mEventinfo.mouse.x;
    mOwner->mMgr->mEventinfo.mouse.oy = mOwner->mMgr->mEventinfo.mouse.y;
}

- (void)rightMouseUp:(NSEvent *)event
{
    if (!mOwner) return;
    iUpdateMousePos(mOwner, event);
    mOwner->eventinfo.mouse.rb_isDown = 0;
    mOwner->eventinfo.mouse.ox = -1;
    mOwner->eventinfo.mouse.oy = -1;
    mOwner->mMgr->mEventinfo.mouse.rb_isDown = 0;
    mOwner->mMgr->mEventinfo.mouse.ox = -1;
    mOwner->mMgr->mEventinfo.mouse.oy = -1;
}

- (void)mouseMoved:(NSEvent *)event
{
    if (!mOwner) return;
    iUpdateMousePos(mOwner, event);
}

- (void)mouseDragged:(NSEvent *)event
{
    if (!mOwner) return;
    iUpdateMousePos(mOwner, event);
}

- (void)rightMouseDragged:(NSEvent *)event
{
    if (!mOwner) return;
    iUpdateMousePos(mOwner, event);
}

- (void)scrollWheel:(NSEvent *)event
{
    if (!mOwner) return;
    const CGFloat delta = [event scrollingDeltaY];
    mOwner->eventinfo.mouse.wheel += (int)(delta * 120.0f);
    mOwner->mMgr->mEventinfo.mouse.wheel += (int)(delta * 120.0f);
}
@end

namespace ImmCore {

//==============================================================================================

int piWindowEvents_GetMouse_Dx( piMouseInput *me )
{
    int res = me->x - me->ox;
    me->ox = me->x;
    return res;
}
int piWindowEvents_GetMouse_Dy( piMouseInput *me )
{
    int res = me->y - me->oy;
    me->oy = me->y;
    return res;
}
int piWindowEvents_GetMouse_Dz( piMouseInput *me )
{
    int res = me->wheel/120;
    me->wheel = 0;
    return res;
}
void piWindowEvents_GetMouse_D( piMouseInput *me )
{
    me->dx = me->x - me->ox;
    me->dy = me->y - me->oy;
    me->dz = me->wheel/120;
    me->oy = me->y;
    me->ox = me->x;
    me->wheel = me->wheel;
}

piWindowMgr piWindowMgr_Init( void )
{
    @autoreleasepool
    {
        iMacWindowMgr *me = (iMacWindowMgr*)malloc(sizeof(iMacWindowMgr));
        if (!me) return 0;
        memset(me, 0, sizeof(iMacWindowMgr));
        me->app = [NSApplication sharedApplication];
        [me->app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [me->app activateIgnoringOtherApps:YES];
        return (piWindowMgr)me;
    }
}

int piWindowMgr_MessageLoop( piWindowMgr vme )
{
    iMacWindowMgr *me = (iMacWindowMgr*)vme;
    if (!me || !me->app) return 0;

    @autoreleasepool
    {
        int done = 0;
        NSEvent *event = nil;
        do
        {
            event = [me->app nextEventMatchingMask:NSEventMaskAny
                                         untilDate:[NSDate distantPast]
                                            inMode:NSDefaultRunLoopMode
                                           dequeue:YES];
            if (event)
                [me->app sendEvent:event];
        } while (event != nil);

        [me->app updateWindows];

        for (int i = 0; i < me->mNumWindows; i++)
        {
            if (me->mWindows[i] && me->mWindows[i]->exitReq)
                done = 1;
        }
        return done;
    }
}

void piWindowMgr_End( piWindowMgr vme )
{
    iMacWindowMgr *me = (iMacWindowMgr*)vme;
    if (!me) return;
    free(me);
}

piWindowEvents *piWindowMgr_getEvents( piWindowMgr vme )
{
    iMacWindowMgr *me = (iMacWindowMgr*)vme;
    return &me->mEventinfo;
}

piWindow piWindow_init( piWindowMgr vmgr, const wchar_t *title, int xo, int yo, int xres, int yres, bool full, bool decoration, bool resizable, bool hideCursor )
{
    (void)hideCursor;
    iMacWindowMgr *mgr = (iMacWindowMgr*)vmgr;
    if (!mgr) return 0;

    @autoreleasepool
    {
        iMacWindow *me = (iMacWindow*)malloc(sizeof(iMacWindow));
        if (!me) return 0;
        memset(me, 0, sizeof(iMacWindow));
        me->mMgr = mgr;
        me->mXres = xres;
        me->mYres = yres;
        me->full = full ? 1 : 0;

        NSUInteger style = 0;
        if (decoration)
        {
            style |= NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
            if (resizable) style |= NSWindowStyleMaskResizable;
        }
        else
        {
            style = NSWindowStyleMaskBorderless;
        }

        NSRect rect = NSMakeRect(xo, yo, xres, yres);
        me->window = [[NSWindow alloc] initWithContentRect:rect
                                                 styleMask:style
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];

        NSString *titleStr = [[NSString alloc] initWithBytes:title
                                                     length:wcslen(title)*sizeof(wchar_t)
                                                   encoding:NSUTF32LittleEndianStringEncoding];
        if (titleStr)
            [me->window setTitle:titleStr];

        NSOpenGLPixelFormatAttribute attrs[] =
        {
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
            NSOpenGLPFAAccelerated,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAColorSize, 24,
            NSOpenGLPFADepthSize, 24,
            NSOpenGLPFAStencilSize, 8,
            0
        };
        NSOpenGLPixelFormat *format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        ImmWindowView *view = [[ImmWindowView alloc] initWithFrame:rect pixelFormat:format owner:me];
        [me->window setContentView:view];
        [me->window makeFirstResponder:view];
        [me->window setAcceptsMouseMovedEvents:YES];

        ImmWindowDelegate *delegate = [[ImmWindowDelegate alloc] initWithOwner:me];
        [me->window setDelegate:delegate];

        me->view = view;
        me->context = [view openGLContext];
        me->cglContext = [me->context CGLContextObj];

        if (full)
            [me->window toggleFullScreen:nil];

        [me->window makeKeyAndOrderFront:nil];

        mgr->mWindows[mgr->mNumWindows++] = me;
        return (piWindow)me;
    }
}

void piWindow_end( piWindow vme )
{
    iMacWindow *me = (iMacWindow*)vme;
    if (!me) return;
    if (me->window) [me->window close];
    free(me);
}

void piWindow_setText( piWindow vme, const wchar_t *str )
{
    iMacWindow *me = (iMacWindow*)vme;
    if (!me || !me->window) return;
    NSString *titleStr = [[NSString alloc] initWithBytes:str
                                                 length:wcslen(str)*sizeof(wchar_t)
                                               encoding:NSUTF32LittleEndianStringEncoding];
    if (titleStr)
        [me->window setTitle:titleStr];
}

void *piWindow_getHandle( piWindow vme )
{
    iMacWindow *me = (iMacWindow*)vme;
    return (void*)me->cglContext;
}

piWindowEvents *piWindow_getEvents( piWindow vme )
{
    iMacWindow *me = (iMacWindow*)vme;
    return &me->eventinfo;
}

int piWindow_getExitReq( piWindow vme )
{
    iMacWindow *me = (iMacWindow*)vme;
    return me->exitReq;
}

void piWIndow_getSize( piWindow vme, int *res )
{
    iMacWindow *me = (iMacWindow*)vme;
    res[0] = me->mXres;
    res[1] = me->mYres;
}

void piWindow_hide( piWindow vme )
{
    iMacWindow *me = (iMacWindow*)vme;
    if (me && me->window) [me->window orderOut:nil];
}

void piWindow_show( piWindow vme )
{
    iMacWindow *me = (iMacWindow*)vme;
    if (me && me->window) [me->window makeKeyAndOrderFront:nil];
}

void piWindowEvents_Erase( piWindow vme )
{
    iMacWindow *me = (iMacWindow*)vme;
    memset( me->eventinfo.keyb.key, 0, 256*sizeof(int) );
}

void piWindowEvents_EraseFull( piKeyboardInput *me )
{
    me->queueLen = 0;
    memset( me->state, 0, 256*sizeof(int) );
    memset( me->key,   0, 256*sizeof(int) );
}

}
