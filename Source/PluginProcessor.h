#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RecentStore.h"

/*  Tube Player's audio side.

    Audio passes straight through: the plugin exists to give the video a proper,
    resizable plugin window inside Live. What it adds on the audio side:
      - host parameters (Play, Volume, Speed, Loop, Follow Live) that you can
        automate or MIDI-map in Live
      - Live's transport state, for "Follow Live"
      - the video, position, loop points and window size, saved inside the Live set
*/
class TubeProcessor : public juce::AudioProcessor
{
public:
    TubeProcessor();
    ~TubeProcessor() override = default;

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

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- shared with the editor (message thread) ----
    juce::AudioProcessorValueTreeState params;
    static constexpr int numRates = 8;
    static const float rates[numRates];
    static int rateToIndex (double rate);

    // what's loaded: id, list, title, t, dur, a, b (guarded by stateLock)
    juce::ValueTree getVideo() const;
    void setVideoProperty (const juce::Identifier& key, const juce::var& value);
    void replaceVideo (const juce::ValueTree& v);

    juce::Point<int> getEditorSize() const { return { editorW.load(), editorH.load() }; }
    void setEditorSize (int w, int h) { editorW = w; editorH = h; }

    std::atomic<bool> hostPlaying { false };
    RecentStore recent;

private:
    template <typename Sample> void process (juce::AudioBuffer<Sample>&);
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    mutable juce::CriticalSection stateLock;
    juce::ValueTree video { "Video" };
    std::atomic<int> editorW { 980 }, editorH { 620 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TubeProcessor)
};
