#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*  The web view that shows AbletonAi's page.

    On macOS this is a WKWebView with its own cookie/cache store (so it never mixes with
    other plugins), links that leave the page open in your browser, and Cmd+C/V/X/A that
    work in the text box even though Live has its own Edit menu.

    Page -> plugin:  window.webkit.messageHandlers.ai.postMessage(JSON string)
    Plugin -> page:  evaluate ("AI.recv({...})") */
class AiWebView : public juce::Component
{
public:
    struct Callbacks
    {
        std::function<void (const juce::var&)> onMessage;   // a message from the page
        std::function<void()> onLoadFailed;                 // the page itself couldn't load
        std::function<void()> onCrashed;                    // the web content process died
        std::function<void (const juce::String&)> onOpenExternal;   // a link that should open in the browser
    };

    explicit AiWebView (Callbacks);
    ~AiWebView() override;

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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiWebView)
};
