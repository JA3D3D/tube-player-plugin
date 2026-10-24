#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "TubeWebView.h"
#include "PageServer.h"

/*  The plugin window: one resizable web view showing the player page.
    This class is the bridge between the page and Live: it loads the page,
    passes host parameter changes and Live's transport to it, and stores
    what the page reports (video, position, loop) in the plugin's state. */
class TubeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int minW = 560, minH = 360, maxW = 3200, maxH = 2000;

    explicit TubeEditor (TubeProcessor&);
    ~TubeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // for tests: what the page would receive
    std::function<void (const juce::String&)> onSendForTesting;
    void receiveForTesting (const juce::var& msg) { onPageMessage (msg); }

private:
    void timerCallback() override;
    void createWebView();
    void loadPage();
    void loadFallback();
    void restart (bool forgetRecent);
    void onPageMessage (const juce::var& msg);
    void send (const juce::var& msg);
    void sendInit();
    void sendRecent();
    float plainValue (const juce::String& id) const;
    void setParam (const juce::String& id, float plain, bool fromUser);
    juce::var paramsToVar() const;

    TubeProcessor& proc;
    std::unique_ptr<PageServer> server;
    std::unique_ptr<TubeWebView> web;
    std::map<juce::String, float> lastSent;
    bool pageReady = false, lastHostPlaying = false, usedFallback = false, restarting = false;
    juce::String lastStatusId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TubeEditor)
};
