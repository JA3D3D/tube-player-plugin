#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*  The web view that shows the player page.

    On macOS this is a WKWebView set up for video: sound plays without a click
    first, fullscreen works, links that leave the player open in your browser,
    and its cookies and cache live in their own store so Restart can wipe them
    completely without touching anything else in Live.

    Page -> plugin:  window.webkit.messageHandlers.tube.postMessage(JSON string)
    Plugin -> page:  evaluate ("TP.recv({...})") */
class TubeWebView : public juce::Component
{
public:
    struct Callbacks
    {
        std::function<void (const juce::var&)> onMessage;   // a message from the page
        std::function<void()> onLoadFailed;                 // the page itself couldn't load
        std::function<void()> onCrashed;                    // the web content process died
        std::function<void (const juce::String&)> onOpenExternal;   // a link that should open in the browser
    };

    explicit TubeWebView (Callbacks);
    ~TubeWebView() override;

    void loadURL (const juce::String& url);
    void loadHTML (const juce::String& html, const juce::String& baseURL);
    void evaluate (const juce::String& javascript);
    void resized() override;

    // remove cookies, cache, storage: everything this player's web views have saved
    static void clearWebsiteData (std::function<void()> whenDone);
    static bool isSupported();

    struct Impl;

private:
    std::unique_ptr<Impl> impl;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TubeWebView)
};
