#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "AiWebView.h"
#include "PageServer.h"
#include "HelperClient.h"
#include "Settings.h"

/*  The AbletonAi window: one resizable web view showing the chat page.
    Connects the page to your settings (API key), to the Live helper, and to the
    conversation saved with the set. The AI requests themselves go from the page
    straight to Claude's API. */
class AiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int minW = 480, minH = 420, maxW = 3200, maxH = 2200;

    AiEditor (AiProcessor&, juce::File settingsFile = AiSettings::defaultFile(), juce::File helperInfo = HelperClient::defaultInfoFile());
    ~AiEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // for tests
    std::function<void (const juce::String&)> onSendForTesting;
    void receiveForTesting (const juce::var& msg) { onPageMessage (msg); }

private:
    void timerCallback() override;
    void createWebView();
    void loadFallback();
    void onPageMessage (const juce::var& msg);
    void send (const juce::var& msg);
    juce::var statusVar() const;
    static bool isAllowedLink (const juce::String& url);

    AiProcessor& proc;
    juce::File settingsFile;
    AiSettings settings;
    std::unique_ptr<HelperClient> helper;
    std::unique_ptr<PageServer> server;
    std::unique_ptr<AiWebView> web;
    bool pageReady = false, usedFallback = false, lastConnected = false;
    juce::String lastLive;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiEditor)
};
