#include "PluginProcessor.h"
#include "PluginEditor.h"

const float TubeProcessor::rates[TubeProcessor::numRates] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

int TubeProcessor::rateToIndex (double rate)
{
    int best = 3;
    for (int i = 0; i < numRates; ++i)
        if (std::abs (rates[i] - rate) < std::abs (rates[best] - rate))
            best = i;
    return best;
}

juce::AudioProcessorValueTreeState::ParameterLayout TubeProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "play", 1 }, "Play", false));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "volume", 1 }, "Volume", 0, 100, 100,
                                                     AudioParameterIntAttributes().withLabel ("%")));
    StringArray speeds;
    for (auto r : rates)
        speeds.add (String (r, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + "x");
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "speed", 1 }, "Speed", speeds, 3));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "loop", 1 }, "Loop", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "follow", 1 }, "Follow Live", false));
    return layout;
}

TubeProcessor::TubeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "TubePlayer", createLayout())
{
}

bool TubeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

template <typename Sample>
void TubeProcessor::process (juce::AudioBuffer<Sample>& buffer)
{
    // audio passes through untouched; only clear outputs that have no input
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
            hostPlaying.store (pos->getIsPlaying(), std::memory_order_relaxed);
}

void TubeProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)  { process (b); }
void TubeProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { process (b); }

juce::AudioProcessorEditor* TubeProcessor::createEditor() { return new TubeEditor (*this); }

// ------------------------------------------------------------------ video state
juce::ValueTree TubeProcessor::getVideo() const
{
    const juce::ScopedLock sl (stateLock);
    return video.createCopy();
}

void TubeProcessor::setVideoProperty (const juce::Identifier& key, const juce::var& value)
{
    const juce::ScopedLock sl (stateLock);
    video.setProperty (key, value, nullptr);
}

void TubeProcessor::replaceVideo (const juce::ValueTree& v)
{
    const juce::ScopedLock sl (stateLock);
    video = v.isValid() ? v.createCopy() : juce::ValueTree ("Video");
}

// ------------------------------------------------------------------ saved with the Live set
void TubeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("TubePlayerState");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("w", editorW.load(), nullptr);
    root.setProperty ("h", editorH.load(), nullptr);
    root.appendChild (params.copyState(), nullptr);
    root.appendChild (getVideo(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void TubeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("TubePlayerState"))
        return;
    auto p = root.getChildWithName (params.state.getType());
    if (p.isValid())
        params.replaceState (p);
    replaceVideo (root.getChildWithName ("Video"));
    editorW = juce::jlimit (TubeEditor::minW, TubeEditor::maxW, (int) root.getProperty ("w", 980));
    editorH = juce::jlimit (TubeEditor::minH, TubeEditor::maxH, (int) root.getProperty ("h", 620));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TubeProcessor(); }
