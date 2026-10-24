// Tests for Tube Player's plugin side (processor, editor bridge, page server, recent list).
// The web view itself is the stand-in here; the page is tested separately in a browser.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/PageServer.h"
#include "BinaryData.h"

static int fails = 0;
static void ok (bool c, const juce::String& m)
{
    if (! c) ++fails;
    std::cout << (c ? "ok   " : "FAIL ") << m << std::endl;
}

static juce::var obj (std::initializer_list<std::pair<const char*, juce::var>> props)
{
    auto* o = new juce::DynamicObject();
    for (auto& p : props) o->setProperty (p.first, p.second);
    return juce::var (o);
}

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static juce::String httpGet (int port, const juce::String& path, int& status)
{
    juce::StreamingSocket s;
    status = 0;
    if (! s.connect ("127.0.0.1", port, 2000)) return {};
    auto req = ("GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n").toStdString();
    s.write (req.data(), (int) req.size());
    juce::MemoryOutputStream out;
    char buf[4096];
    for (;;)
    {
        if (s.waitUntilReady (true, 2000) != 1) break;
        int n = s.read (buf, sizeof (buf), false);
        if (n <= 0) break;
        out.write (buf, (size_t) n);
    }
    auto text = out.toString();
    status = text.fromFirstOccurrenceOf (" ", false, false).getIntValue();
    return text.fromFirstOccurrenceOf ("\r\n\r\n", false, false);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto tmp = juce::File::createTempFile ("tp-recent.json");

    // ---------------------------------------------------------------- page server
    {
        PageServer server (juce::MemoryBlock (BinaryData::ui_html, (size_t) BinaryData::ui_htmlSize));
        const bool started = server.start();
        ok (started && server.getPort() > 49151, "page server starts on a local port (" + juce::String (server.getPort()) + ")");
        ok (server.getURL().startsWith ("http://127.0.0.1:"), "page address is on 127.0.0.1");
        int status = 0;
        auto path = server.getURL().fromFirstOccurrenceOf (juce::String (server.getPort()), false, false);
        auto body = httpGet (server.getPort(), path, status);
        ok (status == 200 && body.contains ("TUBE <span>PLAYER</span>") && body.contains ("iframe_api"), "serves the player page");
        httpGet (server.getPort(), "/", status);
        ok (status == 404, "nothing else is served (" + juce::String (status) + ")");
        httpGet (server.getPort(), "/etc/passwd", status);
        ok (status == 404, "no files from disk");
        auto t0 = juce::Time::getMillisecondCounter();
        // destructor must return promptly
        {
            PageServer s2 (juce::MemoryBlock ("x", 1));
            s2.start();
        }
        ok (juce::Time::getMillisecondCounter() - t0 < 1500, "server shuts down quickly");
    }

    // ---------------------------------------------------------------- recent list
    {
        RecentStore r;
        r.setFileForTesting (tmp);
        r.clear();
        r.touch ("dQw4w9WgXcQ", "");
        r.touch ("9bZkp7q19f0", "");
        r.update ("dQw4w9WgXcQ", "", 95.5, 300, "Growl tutorial");
        r.touch ("dQw4w9WgXcQ", "");
        auto v = r.toVar();
        ok (v.size() == 2 && v[0]["id"] == "dQw4w9WgXcQ" && v[0]["title"] == "Growl tutorial" && (double) v[0]["t"] == 95.5, "recent list: most recent first, with title and position");
        r.save();
        RecentStore r2;
        r2.setFileForTesting (tmp);
        ok (r2.toVar().size() == 2 && r2.positionOf ("dQw4w9WgXcQ", "") == 95.5, "recent list survives a restart (saved outside the web view)");
        for (int i = 0; i < 40; ++i) r2.touch (juce::String ("vid") + juce::String (i).paddedLeft ('0', 8), "");
        ok (r2.toVar().size() == RecentStore::maxItems, "keeps the last 25");
    }

    // ---------------------------------------------------------------- processor
    TubeProcessor proc;
    proc.recent.setFileForTesting (tmp);
    proc.recent.clear();
    {
        proc.setPlayConfigDetails (2, 2, 48000, 512);
        proc.prepareToPlay (48000, 512);
        juce::AudioBuffer<float> buf (2, 512);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i) buf.setSample (ch, i, std::sin (i * 0.01f) * (ch + 1));
        juce::AudioBuffer<float> copy (buf);
        juce::MidiBuffer midi;
        proc.processBlock (buf, midi);
        bool same = true;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i) same = same && buf.getSample (ch, i) == copy.getSample (ch, i);
        ok (same, "audio passes through untouched");
        ok (proc.getLatencySamples() == 0 && proc.getTailLengthSeconds() == 0.0, "no latency, no tail");
        auto names = juce::StringArray();
        for (auto* p : proc.getParameters()) names.add (p->getName (40));
        ok (names.joinIntoString (",") == "Play,Volume,Speed,Loop,Follow Live", "parameters Live can automate: " + names.joinIntoString (", "));
        auto* speed = proc.params.getParameter ("speed");
        ok (speed->getText (speed->convertTo0to1 (1), 20) == "0.5x" && speed->getText (speed->getDefaultValue(), 20) == "1x", "speed shows as 0.5x, 1x...");
    }

    // ---------------------------------------------------------------- editor <-> page
    juce::StringArray sent;
    std::unique_ptr<TubeEditor> ed (dynamic_cast<TubeEditor*> (proc.createEditor()));
    ok (ed != nullptr && ed->isResizable(), "editor window is resizable");
    ed->onSendForTesting = [&] (const juce::String& s) { sent.add (s); };
    auto lastOf = [&] (const juce::String& type) -> juce::var
    {
        for (int i = sent.size(); --i >= 0;) { auto v = juce::JSON::parse (sent[i]); if (v["type"] == type) return v; }
        return {};
    };

    ed->receiveForTesting (obj ({ { "type", "ready" } }));
    auto init = lastOf ("init");
    ok (init.isObject() && init["params"]["speed"] == juce::var (1.0) && init["params"]["volume"] == juce::var (100) && ! (bool) init["params"]["play"], "page gets the parameters when it's ready");

    // opening a video
    ed->receiveForTesting (obj ({ { "type", "opened" }, { "id", "dQw4w9WgXcQ" }, { "list", "" }, { "start", 30 } }));
    ok (proc.getVideo()["id"] == "dQw4w9WgXcQ" && (double) proc.getVideo()["t"] == 30.0, "opened video stored in the plugin state");
    ok (lastOf ("recent")["items"].size() == 1, "recent list sent back to the page");

    // status from the page
    ed->receiveForTesting (obj ({ { "type", "status" }, { "state", "playing" }, { "t", 42.5 }, { "dur", 300.0 }, { "title", "Growl tutorial" }, { "id", "dQw4w9WgXcQ" }, { "list", "" } }));
    ok ((double) proc.getVideo()["t"] == 42.5 && proc.getVideo()["title"] == "Growl tutorial", "position and title kept");
    ok (proc.params.getParameter ("play")->getValue() > 0.5f, "Play parameter follows the video (Live sees it playing)");
    sent.clear();
    pump (150);
    ok (! lastOf ("param").isObject(), "no echo back to the page for changes it made");

    // the page changes parameters
    ed->receiveForTesting (obj ({ { "type", "setParam" }, { "name", "speed" }, { "value", 0.75 } }));
    ok ((int) std::round (proc.params.getParameter ("speed")->convertFrom0to1 (proc.params.getParameter ("speed")->getValue())) == 2, "speed 0.75x from the page sets the Speed parameter");
    ed->receiveForTesting (obj ({ { "type", "setParam" }, { "name", "volume" }, { "value", 40 } }));
    ed->receiveForTesting (obj ({ { "type", "setParam" }, { "name", "follow" }, { "value", 1 } }));
    ed->receiveForTesting (obj ({ { "type", "loop" }, { "a", 60.0 }, { "b", 75.5 } }));
    ok ((double) proc.getVideo()["a"] == 60.0 && (double) proc.getVideo()["b"] == 75.5, "loop points stored");

    // automation / MIDI mapping in Live -> page
    sent.clear();
    auto* play = proc.params.getParameter ("play");
    play->setValueNotifyingHost (0.0f);
    auto* sp = proc.params.getParameter ("speed");
    sp->setValueNotifyingHost (sp->convertTo0to1 (5));
    pump (150);
    bool gotPlay = false, gotSpeed = false;
    for (auto& s : sent) { auto v = juce::JSON::parse (s); if (v["type"] == "param" && v["name"] == "play" && ! (bool) v["value"]) gotPlay = true; if (v["type"] == "param" && v["name"] == "speed" && (double) v["value"] == 1.5) gotSpeed = true; }
    ok (gotPlay && gotSpeed, "automating Play and Speed in Live reaches the page (pause, 1.5x)");

    // Live's transport -> page (for Follow Live)
    sent.clear();
    proc.hostPlaying = true;
    pump (150);
    ok (lastOf ("host").isObject() && (bool) lastOf ("host")["playing"], "Live starting playback is passed to the page");
    proc.hostPlaying = false;

    // clipboard
    juce::SystemClipboard::copyTextToClipboard ("https://youtu.be/9bZkp7q19f0");
    ed->receiveForTesting (obj ({ { "type", "paste" }, { "load", true } }));
    auto clip = lastOf ("clipboard");
    ok (clip.isObject() && (bool) clip["load"], "Paste & Load asks the plugin for the clipboard");

    // window size is remembered
    ed->setSize (1200, 760);
    ok (proc.getEditorSize() == juce::Point<int> (1200, 760), "window size stored");

    // saved with the Live set and restored
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    TubeProcessor proc2;
    proc2.setStateInformation (state.getData(), (int) state.getSize());
    auto v2 = proc2.getVideo();
    ok (v2["id"] == "dQw4w9WgXcQ" && (double) v2["t"] == 42.5 && (double) v2["a"] == 60.0 && v2["title"] == "Growl tutorial", "video, position, loop restored from the Live set");
    ok ((int) std::round (proc2.params.getParameter ("volume")->convertFrom0to1 (proc2.params.getParameter ("volume")->getValue())) == 40
        && proc2.params.getParameter ("follow")->getValue() > 0.5f, "parameters restored");
    ok (proc2.getEditorSize() == juce::Point<int> (1200, 760), "window size restored");
    proc2.setStateInformation ("junk", 4);
    ok (proc2.getVideo()["id"] == "dQw4w9WgXcQ", "damaged state is ignored");

    // a second editor (window reopened) starts the page where it left off
    ed.reset();
    sent.clear();
    std::unique_ptr<TubeEditor> ed2 (dynamic_cast<TubeEditor*> (proc.createEditor()));
    ed2->onSendForTesting = [&] (const juce::String& s) { sent.add (s); };
    ed2->receiveForTesting (obj ({ { "type", "ready" } }));
    auto init2 = lastOf ("init");
    ok (init2["state"]["id"] == "dQw4w9WgXcQ" && (double) init2["state"]["t"] == 42.5 && (double) init2["state"]["a"] == 60.0, "reopening the window carries on at 0:42 with the loop");
    ok (init2["recent"].size() == 1 && init2["recent"][0]["title"] == "Growl tutorial", "recent list with titles in the page");

    // restart with forget
    ed2->receiveForTesting (obj ({ { "type", "restart" }, { "forgetRecent", true } }));
    pump (300);
    ok (proc.recent.toVar().size() == 0, "Restart with 'forget' empties the recent list");
    ed2.reset();

    tmp.deleteFile();
    std::cout << (fails ? juce::String (fails) + " FAILURES" : juce::String ("ALL PLUGIN TESTS PASSED")) << std::endl;
    return fails ? 1 : 0;
}
