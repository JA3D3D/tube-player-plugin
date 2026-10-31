#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const char* const fallbackBase = "https://com.bassbible.abletonai/";

    juce::var obj (std::initializer_list<std::pair<const char*, juce::var>> props)
    {
        auto* o = new juce::DynamicObject();
        for (auto& p : props)
            o->setProperty (p.first, p.second);
        return juce::var (o);
    }
}

AiEditor::AiEditor (AiProcessor& p, juce::File settingsFileToUse, juce::File helperInfo)
    : AudioProcessorEditor (p), proc (p), settingsFile (std::move (settingsFileToUse))
{
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (minW, minH, maxW, maxH);
    const auto size = proc.getEditorSize();
    setSize (juce::jlimit (minW, maxW, size.x), juce::jlimit (minH, maxH, size.y));

    settings = AiSettings::load (settingsFile);
    helper = std::make_unique<HelperClient> (helperInfo);
    server = std::make_unique<PageServer> (juce::MemoryBlock (BinaryData::uibuilt_html, (size_t) BinaryData::uibuilt_htmlSize));
    if (! server->start())
        server.reset();
    createWebView();
    startTimer (1000);
}

AiEditor::~AiEditor()
{
    stopTimer();
    web.reset();
    server.reset();
    helper.reset();
}

void AiEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0e0f11)); }

void AiEditor::resized()
{
    if (web != nullptr)
        web->setBounds (getLocalBounds());
    proc.setEditorSize (getWidth(), getHeight());
}

void AiEditor::createWebView()
{
    pageReady = false;
    juce::Component::SafePointer<AiEditor> safe (this);
    AiWebView::Callbacks cb;
    cb.onMessage = [safe] (const juce::var& m) { if (safe != nullptr) safe->onPageMessage (m); };
    cb.onLoadFailed = [safe] { if (safe != nullptr) safe->loadFallback(); };
    cb.onCrashed = [safe]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->web.reset(); safe->createWebView(); } });
    };
    cb.onOpenExternal = [] (const juce::String& url) { if (isAllowedLink (url)) juce::URL (url).launchInDefaultBrowser(); };
    web = std::make_unique<AiWebView> (std::move (cb));
    addAndMakeVisible (*web);
    web->setBounds (getLocalBounds());
    if (server != nullptr && server->isRunning() && ! usedFallback)
        web->loadURL (server->getURL());
    else
        loadFallback();
}

void AiEditor::loadFallback()
{
    if (web == nullptr || usedFallback)
        return;
    usedFallback = true;
    web->loadHTML (juce::String::fromUTF8 (BinaryData::uibuilt_html, BinaryData::uibuilt_htmlSize), fallbackBase);
}

bool AiEditor::isAllowedLink (const juce::String& url)
{
    return url.startsWith ("https://console.anthropic.com/") || url.startsWith ("https://docs.claude.com/")
        || url.startsWith ("https://www.anthropic.com/") || url.startsWith ("https://support.claude.com/");
}

void AiEditor::send (const juce::var& msg)
{
    const auto json = juce::JSON::toString (msg, true);
    if (onSendForTesting)
        onSendForTesting (json);
    if (web != nullptr && pageReady)
        web->evaluate ("window.AI && AI.recv(" + json + ");");
}

juce::var AiEditor::statusVar() const
{
    const auto s = helper->getStatus();
    return obj ({ { "connected", s.connected }, { "live", s.live }, { "helper", s.helper }, { "error", s.error } });
}

void AiEditor::onPageMessage (const juce::var& m)
{
    const auto type = m.getProperty ("type", "").toString();

    if (type == "ready")
    {
        pageReady = true;
        auto chat = juce::JSON::parse (proc.getChat());
        send (obj ({ { "type", "init" },
                     { "settings", obj ({ { "apiKey", settings.apiKey }, { "model", settings.model } }) },
                     { "helper", statusVar() },
                     { "chat", chat.isObject() ? chat : juce::var (new juce::DynamicObject()) },
                     { "version", JucePlugin_VersionString } }));
    }
    else if (type == "saveSettings")
    {
        if (m.hasProperty ("apiKey"))
            settings.apiKey = m.getProperty ("apiKey", "").toString().trim().substring (0, 300);
        auto model = m.getProperty ("model", "").toString();
        if (model == "fast" || model == "balanced" || model == "best")
            settings.model = model;
        settings.save (settingsFile);
    }
    else if (type == "helper")
    {
        const auto reqId = m.getProperty ("reqId", 0);
        juce::Component::SafePointer<AiEditor> safe (this);
        helper->request (m.getProperty ("cmd", "").toString(), m.getProperty ("args", juce::var()),
                         [safe, reqId] (bool ok, const juce::var& result, const juce::String& error)
                         {
                             if (safe == nullptr)
                                 return;
                             safe->send (obj ({ { "type", "helperResult" }, { "reqId", reqId }, { "ok", ok },
                                                { "result", result }, { "error", error } }));
                         });
    }
    else if (type == "saveChat")
    {
        proc.setChat (juce::JSON::toString (m.getProperty ("chat", juce::var()), true));
    }
    else if (type == "open")
    {
        const auto url = m.getProperty ("url", "").toString();
        if (isAllowedLink (url))
            juce::URL (url).launchInDefaultBrowser();
    }
}

// the green dot: tell the page when Live connects or disconnects
void AiEditor::timerCallback()
{
    const auto s = helper->getStatus();
    if (s.connected != lastConnected || s.live != lastLive)
    {
        lastConnected = s.connected;
        lastLive = s.live;
        send (obj ({ { "type", "status" }, { "helper", statusVar() } }));
    }
}
