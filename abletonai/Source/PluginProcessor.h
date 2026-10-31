#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/*  AbletonAi's audio side: audio passes straight through. The plugin is there to give the
    AI a proper window inside Live; this class keeps the conversation and window size
    with each Live set. */
class AiProcessor : public juce::AudioProcessor
{
public:
    AiProcessor();

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // the conversation in this set, as JSON text (message thread)
    juce::String getChat() const;
    void setChat (const juce::String& json);

    juce::Point<int> getEditorSize() const { return { editorW.load(), editorH.load() }; }
    void setEditorSize (int w, int h) { editorW = w; editorH = h; }

    static constexpr int maxChatChars = 400000;

private:
    template <typename S> void process (juce::AudioBuffer<S>&);
    mutable juce::CriticalSection lock;
    juce::String chat;
    std::atomic<int> editorW { 900 }, editorH { 680 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiProcessor)
};
