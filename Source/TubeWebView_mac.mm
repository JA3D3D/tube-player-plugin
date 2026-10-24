// Tube Player's web view on macOS: a WKWebView hosted in a JUCE NSViewComponent.
// Compiled with ARC (see CMakeLists.txt).

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include <juce_gui_extra/juce_gui_extra.h>
#include "TubeWebView.h"

// Class names carry a version so they never clash with another build loaded in the same Live.
#define TP_WEBVIEW_CLASS TubePlayerBB_WebView_v1
#define TP_BRIDGE_CLASS  TubePlayerBB_Bridge_v1

//==============================================================================
// What the Objective-C side calls back into. Cleared before the C++ side goes away.
struct TubeBridge
{
    TubeWebView::Callbacks callbacks;
    juce::String allowedPrefix;     // main-frame address the page lives at
    bool alive = true;
};

static NSString* toNS (const juce::String& s)   { return [NSString stringWithUTF8String: s.toRawUTF8()]; }
static juce::String toJuce (NSString* s)        { return s != nil ? juce::String::fromUTF8 ([s UTF8String]) : juce::String(); }

// One persistent store just for Tube Player (macOS 14+), so Restart can wipe it completely.
// Older macOS: the shared store, cleaned of YouTube/Google/player entries only.
static WKWebsiteDataStore* tubeDataStore()
{
    if (@available (macOS 14.0, *))
    {
        NSUUID* uuid = [[NSUUID alloc] initWithUUIDString: @"6E1B2F0A-3C4D-4B5E-9F60-7A8B9C0D1E2F"];
        return [WKWebsiteDataStore dataStoreForIdentifier: uuid];
    }
    return [WKWebsiteDataStore defaultDataStore];
}

//==============================================================================
@interface TP_WEBVIEW_CLASS : WKWebView
@end

@implementation TP_WEBVIEW_CLASS

// the first click in an inactive plugin window should already press the button
- (BOOL) acceptsFirstMouse: (NSEvent*) event { juce::ignoreUnused (event); return YES; }

// Live's Edit menu can swallow Cmd+V/C/X/A before a web text field sees them.
// When the cursor is in this view, handle those keys here.
- (BOOL) performKeyEquivalent: (NSEvent*) event
{
    NSResponder* first = [[self window] firstResponder];
    const bool focusedHere = [first isKindOfClass: [NSView class]] && [(NSView*) first isDescendantOf: self];
    const NSEventModifierFlags mods = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;

    if (focusedHere && (mods == NSEventModifierFlagCommand) && [event type] == NSEventTypeKeyDown)
    {
        NSString* key = [[event charactersIgnoringModifiers] lowercaseString];
        SEL action = nil;
        if ([key isEqualToString: @"v"]) action = @selector (paste:);
        else if ([key isEqualToString: @"c"]) action = @selector (copy:);
        else if ([key isEqualToString: @"x"]) action = @selector (cut:);
        else if ([key isEqualToString: @"a"]) action = @selector (selectAll:);
        if (action != nil && [NSApp sendAction: action to: nil from: self])
            return YES;
    }
    return [super performKeyEquivalent: event];
}
@end

//==============================================================================
@interface TP_BRIDGE_CLASS : NSObject <WKScriptMessageHandler, WKNavigationDelegate, WKUIDelegate>
{
@public
    TubeBridge* bridge;
}
@end

@implementation TP_BRIDGE_CLASS

- (void) userContentController: (WKUserContentController*) controller didReceiveScriptMessage: (WKScriptMessage*) message
{
    juce::ignoreUnused (controller);
    if (bridge == nullptr || ! bridge->alive || ! bridge->callbacks.onMessage)
        return;
    // only the player page itself (main frame) may talk to the plugin
    if (! message.frameInfo.isMainFrame)
        return;

    id body = message.body;
    juce::String json;
    if ([body isKindOfClass: [NSString class]])
        json = toJuce ((NSString*) body);
    else if ([NSJSONSerialization isValidJSONObject: body])
    {
        NSData* data = [NSJSONSerialization dataWithJSONObject: body options: 0 error: nil];
        json = toJuce ([[NSString alloc] initWithData: data encoding: NSUTF8StringEncoding]);
    }
    if (json.isNotEmpty())
        bridge->callbacks.onMessage (juce::JSON::parse (json));
}

- (bool) isAllowedMainFrame: (NSURL*) url
{
    if (url == nil) return true;
    const auto s = toJuce ([url absoluteString]);
    return s.startsWith ("about:") || (bridge != nullptr && bridge->allowedPrefix.isNotEmpty() && s.startsWith (bridge->allowedPrefix));
}

- (void) openOutside: (NSURL*) url
{
    if (url == nil || bridge == nullptr || ! bridge->alive || ! bridge->callbacks.onOpenExternal)
        return;
    NSString* scheme = [[url scheme] lowercaseString];
    if ([scheme isEqualToString: @"https"] || [scheme isEqualToString: @"http"])
        bridge->callbacks.onOpenExternal (toJuce ([url absoluteString]));
}

// links that would take the whole player somewhere else open in your browser instead
- (void) webView: (WKWebView*) webView decidePolicyForNavigationAction: (WKNavigationAction*) action
         decisionHandler: (void (^)(WKNavigationActionPolicy)) decisionHandler
{
    juce::ignoreUnused (webView);
    NSURL* url = action.request.URL;
    WKFrameInfo* target = action.targetFrame;

    if (target == nil || (target.isMainFrame && ! [self isAllowedMainFrame: url]))
    {
        [self openOutside: url];
        decisionHandler (WKNavigationActionPolicyCancel);
        return;
    }
    decisionHandler (WKNavigationActionPolicyAllow);
}

// target="_blank" and window.open: browser, not a new web view
- (WKWebView*) webView: (WKWebView*) webView createWebViewWithConfiguration: (WKWebViewConfiguration*) configuration
           forNavigationAction: (WKNavigationAction*) action windowFeatures: (WKWindowFeatures*) features
{
    juce::ignoreUnused (webView, configuration, features);
    [self openOutside: action.request.URL];
    return nil;
}

- (void) webView: (WKWebView*) webView didFailProvisionalNavigation: (WKNavigation*) navigation withError: (NSError*) error
{
    juce::ignoreUnused (webView, navigation);
    if ([error code] == NSURLErrorCancelled)
        return;
    if (bridge != nullptr && bridge->alive && bridge->callbacks.onLoadFailed)
        bridge->callbacks.onLoadFailed();
}

- (void) webViewWebContentProcessDidTerminate: (WKWebView*) webView
{
    juce::ignoreUnused (webView);
    if (bridge != nullptr && bridge->alive && bridge->callbacks.onCrashed)
        bridge->callbacks.onCrashed();
}
@end

//==============================================================================
struct TubeWebView::Impl
{
    explicit Impl (TubeWebView::Callbacks cb)
    {
        bridge.callbacks = std::move (cb);

        WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
        config.websiteDataStore = tubeDataStore();
        config.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;   // device buttons can start sound
        config.suppressesIncrementalRendering = NO;
        config.applicationNameForUserAgent = @"Version/17.0 Safari/605.1.15";
        if (@available (macOS 12.3, *))
            config.preferences.elementFullscreenEnabled = YES;

        handler = [[TP_BRIDGE_CLASS alloc] init];
        handler->bridge = &bridge;

        WKUserContentController* ucc = config.userContentController;
        [ucc addScriptMessageHandler: handler name: @"tube"];
        WKUserScript* marker = [[WKUserScript alloc] initWithSource: @"window.__TUBE_NATIVE__ = 'mac';"
                                                      injectionTime: WKUserScriptInjectionTimeAtDocumentStart
                                                   forMainFrameOnly: YES];
        [ucc addUserScript: marker];

        webView = [[TP_WEBVIEW_CLASS alloc] initWithFrame: NSMakeRect (0, 0, 640, 360) configuration: config];
        webView.navigationDelegate = handler;
        webView.UIDelegate = handler;
        webView.allowsMagnification = NO;
        webView.allowsBackForwardNavigationGestures = NO;
        @try { [webView setValue: @NO forKey: @"drawsBackground"]; } @catch (NSException*) {}   // no white flash
        if (@available (macOS 13.3, *))
            webView.inspectable = YES;     // Safari > Develop can inspect the page if something goes wrong

        holder.setView ((__bridge void*) webView);
    }

    ~Impl()
    {
        bridge.alive = false;
        handler->bridge = nullptr;
        [webView stopLoading];
        webView.navigationDelegate = nil;
        webView.UIDelegate = nil;
        [webView.configuration.userContentController removeScriptMessageHandlerForName: @"tube"];
        [webView.configuration.userContentController removeAllUserScripts];
        // stop any playing video right away instead of when the view is finally released
        [webView loadHTMLString: @"" baseURL: nil];
        holder.setView (nullptr);
        webView = nil;
        handler = nil;
    }

    TubeBridge bridge;
    juce::NSViewComponent holder;
    TP_WEBVIEW_CLASS* webView = nil;
    TP_BRIDGE_CLASS* handler = nil;
};

//==============================================================================
TubeWebView::TubeWebView (Callbacks cb) : impl (std::make_unique<Impl> (std::move (cb)))
{
    addAndMakeVisible (impl->holder);
    setOpaque (false);
}

TubeWebView::~TubeWebView() = default;

void TubeWebView::resized() { impl->holder.setBounds (getLocalBounds()); }

void TubeWebView::loadURL (const juce::String& url)
{
    impl->bridge.allowedPrefix = url.upToFirstOccurrenceOf ("?", false, false).upToFirstOccurrenceOf ("#", false, false);
    NSURL* u = [NSURL URLWithString: toNS (url)];
    if (u != nil)
        [impl->webView loadRequest: [NSURLRequest requestWithURL: u]];
}

void TubeWebView::loadHTML (const juce::String& html, const juce::String& baseURL)
{
    impl->bridge.allowedPrefix = baseURL;
    [impl->webView loadHTMLString: toNS (html) baseURL: [NSURL URLWithString: toNS (baseURL)]];
}

void TubeWebView::evaluate (const juce::String& javascript)
{
    [impl->webView evaluateJavaScript: toNS (javascript) completionHandler: nil];
}

bool TubeWebView::isSupported() { return true; }

void TubeWebView::clearWebsiteData (std::function<void()> whenDone)
{
    auto done = std::make_shared<std::function<void()>> (std::move (whenDone));
    auto finish = ^{ juce::MessageManager::callAsync ([done] { if (*done) (*done)(); }); };

    WKWebsiteDataStore* store = tubeDataStore();
    NSSet<NSString*>* types = [WKWebsiteDataStore allWebsiteDataTypes];

    if (@available (macOS 14.0, *))
    {
        // the store is Tube Player's alone: remove everything in it
        [store removeDataOfTypes: types modifiedSince: [NSDate distantPast] completionHandler: finish];
        return;
    }

    // shared store on older macOS: only what the player could have created
    [store fetchDataRecordsOfTypes: types completionHandler: ^(NSArray<WKWebsiteDataRecord*>* records) {
        NSArray<NSString*>* keys = @[ @"youtube", @"ytimg", @"ggpht", @"googlevideo", @"google", @"gstatic",
                                      @"doubleclick", @"127.0.0.1", @"localhost", @"bassbible" ];
        NSMutableArray<WKWebsiteDataRecord*>* mine = [NSMutableArray array];
        for (WKWebsiteDataRecord* r in records)
        {
            NSString* name = [[r displayName] lowercaseString];
            for (NSString* k in keys)
                if ([name containsString: k]) { [mine addObject: r]; break; }
        }
        [store removeDataOfTypes: types forDataRecords: mine completionHandler: finish];
    }];
}
