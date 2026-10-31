#include "PluginProcessor.h"
#include "PluginEditor.h"

AiProcessor::AiProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool AiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

template <typename S>
void AiProcessor::process (juce::AudioBuffer<S>& buffer)
{
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

void AiProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)  { process (b); }
void AiProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { process (b); }

juce::AudioProcessorEditor* AiProcessor::createEditor() { return new AiEditor (*this); }

juce::String AiProcessor::getChat() const
{
    const juce::ScopedLock sl (lock);
    return chat;
}

void AiProcessor::setChat (const juce::String& json)
{
    const juce::ScopedLock sl (lock);
    chat = json.length() <= maxChatChars ? json : juce::String();
}

void AiProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("AbletonAiState");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("w", editorW.load(), nullptr);
    root.setProperty ("h", editorH.load(), nullptr);
    root.setProperty ("chat", getChat(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void AiProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("AbletonAiState"))
        return;
    editorW = juce::jlimit (AiEditor::minW, AiEditor::maxW, (int) root.getProperty ("w", 900));
    editorH = juce::jlimit (AiEditor::minH, AiEditor::maxH, (int) root.getProperty ("h", 680));
    setChat (root.getProperty ("chat", "").toString());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AiProcessor(); }
