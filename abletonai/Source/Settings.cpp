#include "Settings.h"

#if JUCE_MAC || JUCE_LINUX
 #include <sys/stat.h>
#endif

juce::File AiSettings::defaultFile()
{
   #if JUCE_MAC
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support");
   #else
    auto base = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
   #endif
    return base.getChildFile ("AbletonAi").getChildFile ("settings.json");
}

AiSettings AiSettings::load (const juce::File& f)
{
    AiSettings s;
    if (f.existsAsFile())
    {
        auto v = juce::JSON::parse (f.loadFileAsString());
        s.apiKey = v.getProperty ("apiKey", "").toString().trim();
        auto m = v.getProperty ("model", "balanced").toString();
        if (m == "fast" || m == "balanced" || m == "best")
            s.model = m;
    }
    return s;
}

bool AiSettings::save (const juce::File& f) const
{
    if (! f.getParentDirectory().createDirectory())
        return false;
    auto* o = new juce::DynamicObject();
    o->setProperty ("apiKey", apiKey);
    o->setProperty ("model", model);
    auto tmp = f.getSiblingFile (f.getFileName() + ".tmp");
    if (! tmp.replaceWithText (juce::JSON::toString (juce::var (o), false)))
        return false;
   #if JUCE_MAC || JUCE_LINUX
    ::chmod (tmp.getFullPathName().toRawUTF8(), S_IRUSR | S_IWUSR);     // your account only
   #endif
    return tmp.moveFileTo (f);
}
