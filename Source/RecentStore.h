#pragma once

#include <juce_core/juce_core.h>

/*  Recent videos and where you stopped in each, shared by every Tube Player.
    Kept outside the web view's data (Application Support/Tube Player/recent.json),
    so Restart can wipe the browser data without losing the list.
    The first time, it picks up the list from the Max for Live Tube Player if there is one. */
class RecentStore
{
public:
    static constexpr int maxItems = 25;

    RecentStore();

    juce::var toVar() const;                     // [{ id, list, title, t, dur }]
    void touch (const juce::String& id, const juce::String& list);            // opened: move to the top
    void update (const juce::String& id, const juce::String& list, double t, double dur, const juce::String& title);
    double positionOf (const juce::String& id, const juce::String& list) const;
    void clear();
    void saveIfNeeded();
    bool save();

    static juce::File defaultFile();
    juce::File getFile() const { return file; }
    void setFileForTesting (const juce::File& f) { file = f; items.clear(); load(); }

private:
    void load();
    int indexOf (const juce::String& id, const juce::String& list) const;

    juce::File file;
    juce::Array<juce::var> items;
    bool dirty = false;
    juce::uint32 lastSave = 0;
};
