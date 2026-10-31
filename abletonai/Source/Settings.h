#pragma once

#include <juce_core/juce_core.h>

/*  Your AbletonAi settings (Claude API key, model), shared by every AbletonAi in every set.
    Kept in Application Support/AbletonAi/settings.json, readable only by your user account. */
struct AiSettings
{
    juce::String apiKey;
    juce::String model { "balanced" };

    static juce::File defaultFile();
    static AiSettings load (const juce::File& f = defaultFile());
    bool save (const juce::File& f = defaultFile()) const;
};
