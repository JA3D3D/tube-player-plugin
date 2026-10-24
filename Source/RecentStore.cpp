#include "RecentStore.h"

juce::File RecentStore::defaultFile()
{
   #if JUCE_MAC
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support");
   #else
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #endif
    return base.getChildFile ("Tube Player").getChildFile ("recent.json");
}

RecentStore::RecentStore() : file (defaultFile()) { load(); }

static juce::var makeItem (const juce::String& id, const juce::String& list, const juce::String& title, double t, double dur)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("id", id);
    o->setProperty ("list", list);
    o->setProperty ("title", title);
    o->setProperty ("t", t);
    o->setProperty ("dur", dur);
    return juce::var (o);
}

void RecentStore::load()
{
    juce::var parsed;
    if (file.existsAsFile())
        parsed = juce::JSON::parse (file.loadFileAsString());

    if (! parsed.isArray())
    {
        // first run: borrow the list from the Max for Live Tube Player (Documents/Tube Player/tube-player.json)
        auto m4l = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("Tube Player").getChildFile ("tube-player.json");
        if (m4l.existsAsFile())
        {
            auto old = juce::JSON::parse (m4l.loadFileAsString());
            parsed = old.getProperty ("recent", juce::var());
            dirty = parsed.isArray();
        }
    }

    if (auto* arr = parsed.getArray())
        for (auto& v : *arr)
        {
            auto id = v.getProperty ("id", "").toString();
            auto list = v.getProperty ("list", "").toString();
            if ((id.isEmpty() && list.isEmpty()) || indexOf (id, list) >= 0)
                continue;
            items.add (makeItem (id, list, v.getProperty ("title", "").toString(),
                                 (double) v.getProperty ("t", 0.0), (double) v.getProperty ("dur", 0.0)));
            if (items.size() >= maxItems)
                break;
        }
}

int RecentStore::indexOf (const juce::String& id, const juce::String& list) const
{
    for (int i = 0; i < items.size(); ++i)
        if (items[i]["id"].toString() == id && items[i]["list"].toString() == list)
            return i;
    return -1;
}

juce::var RecentStore::toVar() const
{
    juce::Array<juce::var> copy;
    for (auto& v : items)
        copy.add (makeItem (v["id"], v["list"], v["title"], (double) v["t"], (double) v["dur"]));
    return juce::var (copy);
}

void RecentStore::touch (const juce::String& id, const juce::String& list)
{
    if (id.isEmpty() && list.isEmpty())
        return;
    auto i = indexOf (id, list);
    juce::var item = i >= 0 ? items[i] : makeItem (id, list, {}, 0.0, 0.0);
    if (i >= 0)
        items.remove (i);
    items.insert (0, item);
    while (items.size() > maxItems)
        items.removeLast();
    dirty = true;
    save();
}

void RecentStore::update (const juce::String& id, const juce::String& list, double t, double dur, const juce::String& title)
{
    auto i = indexOf (id, list);
    if (i < 0)
        return;
    auto* o = items.getReference (i).getDynamicObject();
    if (o == nullptr)
        return;
    if (t > 0)   o->setProperty ("t", t);
    if (dur > 0) o->setProperty ("dur", dur);
    if (title.isNotEmpty() && o->getProperty ("title").toString() != title)
    {
        o->setProperty ("title", title);
        dirty = true;
        save();          // titles show in the list straight away
        return;
    }
    dirty = true;
}

double RecentStore::positionOf (const juce::String& id, const juce::String& list) const
{
    auto i = indexOf (id, list);
    return i >= 0 ? (double) items[i]["t"] : 0.0;
}

void RecentStore::clear()
{
    items.clear();
    dirty = true;
    save();
}

void RecentStore::saveIfNeeded()
{
    if (dirty && juce::Time::getMillisecondCounter() - lastSave > 5000)
        save();
}

bool RecentStore::save()
{
    lastSave = juce::Time::getMillisecondCounter();
    if (! file.getParentDirectory().createDirectory())
        return false;
    juce::Array<juce::var> copy (items);
    if (! file.replaceWithText (juce::JSON::toString (juce::var (copy), false)))
        return false;
    dirty = false;
    return true;
}
