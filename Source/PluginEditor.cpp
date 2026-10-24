#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const char* const paramIds[] = { "play", "volume", "speed", "loop", "follow" };
    // address used when the local page server can't start; YouTube accepts an app's
    // own https address (its bundle id) as the page address
    const char* const fallbackBase = "https://com.bassbible.tubeplayer/";

    juce::var obj (std::initializer_list<std::pair<const char*, juce::var>> props)
    {
        auto* o = new juce::DynamicObject();
        for (auto& p : props)
            o->setProperty (p.first, p.second);
        return juce::var (o);
    }

    bool isYouTubeURL (const juce::String& u)
    {
        return u.startsWith ("https://www.youtube.com/") || u.startsWith ("https://youtube.com/")
            || u.startsWith ("https://m.youtube.com/") || u.startsWith ("https://youtu.be/")
            || u.startsWith ("https://music.youtube.com/");
    }
}

TubeEditor::TubeEditor (TubeProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (minW, minH, maxW, maxH);
    const auto size = proc.getEditorSize();
    setSize (juce::jlimit (minW, maxW, size.x), juce::jlimit (minH, maxH, size.y));

    server = std::make_unique<PageServer> (juce::MemoryBlock (BinaryData::ui_html, (size_t) BinaryData::ui_htmlSize));
    if (! server->start())
        server.reset();

    createWebView();
    startTimerHz (20);
}

TubeEditor::~TubeEditor()
{
    stopTimer();
    proc.recent.save();
    web.reset();
    server.reset();
}

void TubeEditor::paint (juce::Graphics& g)
{
    // shows for a moment while the page loads
    g.fillAll (juce::Colour (0xff121316));
}

void TubeEditor::resized()
{
    if (web != nullptr)
        web->setBounds (getLocalBounds());
    proc.setEditorSize (getWidth(), getHeight());
}

// ------------------------------------------------------------------ the web view
void TubeEditor::createWebView()
{
    pageReady = false;
    lastSent.clear();
    juce::Component::SafePointer<TubeEditor> safe (this);

    TubeWebView::Callbacks cb;
    cb.onMessage = [safe] (const juce::var& m) { if (safe != nullptr) safe->onPageMessage (m); };
    cb.onLoadFailed = [safe] { if (safe != nullptr) safe->loadFallback(); };
    cb.onCrashed = [safe]
    {
        // the web content process died: start a fresh one after this callback returns
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->web.reset(); safe->createWebView(); } });
    };
    cb.onOpenExternal = [] (const juce::String& url)
    {
        if (url.startsWithIgnoreCase ("https://") || url.startsWithIgnoreCase ("http://"))
            juce::URL (url).launchInDefaultBrowser();
    };

    web = std::make_unique<TubeWebView> (std::move (cb));
    addAndMakeVisible (*web);
    web->setBounds (getLocalBounds());
    loadPage();
}

void TubeEditor::loadPage()
{
    if (server != nullptr && server->isRunning() && ! usedFallback)
        web->loadURL (server->getURL());
    else
        loadFallback();
}

void TubeEditor::loadFallback()
{
    if (web == nullptr || usedFallback)
        return;
    usedFallback = true;
    web->loadHTML (juce::String::fromUTF8 (BinaryData::ui_html, BinaryData::ui_htmlSize), fallbackBase);
}

// Restart: throw the web view away, wipe everything it stored, start a fresh one
void TubeEditor::restart (bool forgetRecent)
{
    if (restarting)
        return;
    restarting = true;
    web.reset();
    pageReady = false;
    if (forgetRecent)
        proc.recent.clear();
    repaint();

    juce::Component::SafePointer<TubeEditor> safe (this);
    TubeWebView::clearWebsiteData ([safe]
    {
        if (safe == nullptr)
            return;
        safe->restarting = false;
        safe->usedFallback = false;
        safe->createWebView();
    });
}

// ------------------------------------------------------------------ talking to the page
void TubeEditor::send (const juce::var& msg)
{
    const auto json = juce::JSON::toString (msg, true);
    if (onSendForTesting)
        onSendForTesting (json);
    if (web != nullptr && pageReady)
        web->evaluate ("window.TP && TP.recv(" + json + ");");
}

float TubeEditor::plainValue (const juce::String& id) const
{
    if (auto* p = proc.params.getParameter (id))
        return p->convertFrom0to1 (p->getValue());
    return 0.0f;
}

juce::var TubeEditor::paramsToVar() const
{
    return obj ({ { "play", plainValue ("play") > 0.5f },
                  { "volume", (int) std::round (plainValue ("volume")) },
                  { "speed", (double) TubeProcessor::rates[juce::jlimit (0, TubeProcessor::numRates - 1, (int) std::round (plainValue ("speed")))] },
                  { "loop", plainValue ("loop") > 0.5f },
                  { "follow", plainValue ("follow") > 0.5f } });
}

void TubeEditor::sendInit()
{
    auto v = proc.getVideo();
    auto state = obj ({ { "id", v.getProperty ("id", "") }, { "list", v.getProperty ("list", "") },
                        { "title", v.getProperty ("title", "") }, { "t", v.getProperty ("t", 0.0) },
                        { "dur", v.getProperty ("dur", 0.0) }, { "a", v.getProperty ("a", -1.0) },
                        { "b", v.getProperty ("b", -1.0) } });
    for (auto* id : paramIds)
        lastSent[id] = plainValue (id);
    lastHostPlaying = proc.hostPlaying.load();
    send (obj ({ { "type", "init" }, { "state", state }, { "params", paramsToVar() }, { "recent", proc.recent.toVar() },
                 { "host", obj ({ { "playing", lastHostPlaying } }) }, { "version", JucePlugin_VersionString },
                 { "native", TubeWebView::isSupported() ? "mac" : "none" } }));
}

void TubeEditor::sendRecent()
{
    send (obj ({ { "type", "recent" }, { "items", proc.recent.toVar() } }));
}

void TubeEditor::setParam (const juce::String& id, float plain, bool fromUser)
{
    auto* p = proc.params.getParameter (id);
    if (p == nullptr)
        return;
    const float norm = juce::jlimit (0.0f, 1.0f, p->convertTo0to1 (plain));
    lastSent[id] = p->convertFrom0to1 (norm);       // the page already shows this: don't echo it back
    if (std::abs (p->getValue() - norm) < 1.0e-5f)
        return;
    if (fromUser) p->beginChangeGesture();
    p->setValueNotifyingHost (norm);
    if (fromUser) p->endChangeGesture();
}

void TubeEditor::onPageMessage (const juce::var& m)
{
    const auto type = m.getProperty ("type", "").toString();

    if (type == "ready")
    {
        pageReady = true;
        sendInit();
    }
    else if (type == "status")
    {
        const auto id = m.getProperty ("id", "").toString();
        const auto list = m.getProperty ("list", "").toString();
        const double t = m.getProperty ("t", 0.0), dur = m.getProperty ("dur", 0.0);
        const auto title = m.getProperty ("title", "").toString();
        const auto state = m.getProperty ("state", "").toString();
        if (id.isNotEmpty() || list.isNotEmpty())
        {
            if (t > 0) proc.setVideoProperty ("t", t);
            if (dur > 0) proc.setVideoProperty ("dur", dur);
            if (title.isNotEmpty()) proc.setVideoProperty ("title", title);
            proc.recent.update (id, list, t, dur, title);
            if (title.isNotEmpty() && title != lastStatusId)
            {
                lastStatusId = title;
                sendRecent();
            }
        }
        // the video's own state drives the Play parameter, so Live shows it
        if (state == "playing" || state == "muted")
            setParam ("play", 1.0f, false);
        else if (state == "paused" || state == "ended")
            setParam ("play", 0.0f, false);
    }
    else if (type == "opened")
    {
        const auto id = m.getProperty ("id", "").toString();
        const auto list = m.getProperty ("list", "").toString();
        juce::ValueTree v ("Video");
        v.setProperty ("id", id, nullptr);
        v.setProperty ("list", list, nullptr);
        v.setProperty ("t", m.getProperty ("start", 0.0), nullptr);
        v.setProperty ("a", -1.0, nullptr);
        v.setProperty ("b", -1.0, nullptr);
        proc.replaceVideo (v);
        proc.recent.touch (id, list);
        lastStatusId.clear();
        sendRecent();
    }
    else if (type == "setParam")
    {
        const auto name = m.getProperty ("name", "").toString();
        const double value = m.getProperty ("value", 0.0);
        if (name == "speed")
            setParam (name, (float) TubeProcessor::rateToIndex (value), true);
        else if (name == "play" || name == "loop" || name == "follow")
            setParam (name, value > 0.5 ? 1.0f : 0.0f, true);
        else if (name == "volume")
            setParam (name, (float) juce::jlimit (0.0, 100.0, value), true);
    }
    else if (type == "loop")
    {
        proc.setVideoProperty ("a", m.getProperty ("a", -1.0));
        proc.setVideoProperty ("b", m.getProperty ("b", -1.0));
    }
    else if (type == "restart")
    {
        const bool forget = m.getProperty ("forgetRecent", false);
        juce::Component::SafePointer<TubeEditor> safe (this);
        // not from inside the web view's own callback
        juce::MessageManager::callAsync ([safe, forget] { if (safe != nullptr) safe->restart (forget); });
    }
    else if (type == "paste")
    {
        send (obj ({ { "type", "clipboard" }, { "text", juce::SystemClipboard::getTextFromClipboard().substring (0, 2000) },
                     { "load", m.getProperty ("load", false) } }));
    }
    else if (type == "open")
    {
        const auto url = m.getProperty ("url", "").toString();
        if (isYouTubeURL (url))
            juce::URL (url).launchInDefaultBrowser();
    }
    else if (type == "forget")
    {
        proc.recent.clear();
        sendRecent();
    }
}

// host automation, MIDI mapping and Live's transport -> page
void TubeEditor::timerCallback()
{
    proc.recent.saveIfNeeded();
    if (! pageReady)
        return;

    for (auto* id : paramIds)
    {
        const float v = plainValue (id);
        auto it = lastSent.find (id);
        if (it != lastSent.end() && std::abs (it->second - v) < 1.0e-4f)
            continue;
        lastSent[id] = v;
        juce::var value;
        if (juce::String (id) == "speed")
            value = (double) TubeProcessor::rates[juce::jlimit (0, TubeProcessor::numRates - 1, (int) std::round (v))];
        else if (juce::String (id) == "volume")
            value = (int) std::round (v);
        else
            value = v > 0.5f;
        send (obj ({ { "type", "param" }, { "name", id }, { "value", value } }));
    }

    const bool hp = proc.hostPlaying.load();
    if (hp != lastHostPlaying)
    {
        lastHostPlaying = hp;
        send (obj ({ { "type", "host" }, { "playing", hp } }));
    }
}
