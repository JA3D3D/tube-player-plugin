// Tests for AbletonAi's plugin side: settings, the conversation saved with the set, the page
// bridge, and the link to the Live helper (the real Python helper with a fake Live set).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/HelperClient.h"
#include "../Source/Settings.h"
#include "BinaryData.h"

#if JUCE_MAC || JUCE_LINUX
 #include <sys/stat.h>
#endif

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

template <typename Fn>
static bool waitFor (Fn&& done, int ms)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end)
    {
        if (done()) return true;
        pump (20);
    }
    return done();
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto tmp = juce::File::createTempFile ("abletonai-test");
    tmp.createDirectory();

    // ---------------------------------------------------------------- the page
    {
        const auto page = juce::String::fromUTF8 (BinaryData::uibuilt_html, BinaryData::uibuilt_htmlSize);
        ok (page.contains ("AbletonAiAgent") && page.contains ("write_clip") && ! page.contains ("src=\"agent.js\""),
            "the built page has the AI tools inside it (one self-contained file)");
    }

    // ---------------------------------------------------------------- settings
    {
        auto f = tmp.getChildFile ("settings.json");
        AiSettings s;
        s.apiKey = "sk-ant-test";
        s.model = "best";
        ok (s.save (f), "settings saved");
       #if JUCE_MAC || JUCE_LINUX
        struct stat st {};
        ::stat (f.getFullPathName().toRawUTF8(), &st);
        ok ((st.st_mode & 0777) == 0600, "settings file readable only by you");
       #endif
        auto back = AiSettings::load (f);
        ok (back.apiKey == "sk-ant-test" && back.model == "best", "settings load back");
        f.replaceWithText ("{\"model\": \"turbo\"}");
        ok (AiSettings::load (f).model == "balanced", "unknown model falls back to Balanced");
    }

    // ---------------------------------------------------------------- helper link: nothing running
    auto infoDir = tmp.getChildFile ("info");
    infoDir.createDirectory();
    auto infoFile = infoDir.getChildFile ("helper.json");
    {
        HelperClient hc (infoFile);
        bool got = false, okRes = true;
        juce::String err;
        hc.request ("ping", {}, [&] (bool o, const juce::var&, const juce::String& e) { got = true; okRes = o; err = e; });
        ok (waitFor ([&] { return got; }, 3000) && ! okRes && err.contains ("Control Surface"), "with Live's helper off, requests fail with how to switch it on");
        ok (! hc.getStatus().connected, "status: not connected");
    }

    // ---------------------------------------------------------------- helper link: the real helper with a fake Live set
    juce::ChildProcess py;
    const auto script = juce::File (AI_SOURCE_DIR).getChildFile ("live-helper/tests/fake_helper_server.py");
    const bool started = py.start (juce::StringArray { "python3", script.getFullPathName(), infoDir.getFullPathName(), "60" });
    ok (started && waitFor ([&] { return infoFile.existsAsFile(); }, 8000), "the helper starts and writes where to find it");
    {
        HelperClient hc (infoFile);
        ok (waitFor ([&] { return hc.getStatus().connected; }, 6000) && hc.getStatus().live == "11.3.35", "connects and sees Live 11.3.35 (the green dot)");

        int done = 0;
        juce::var created, written, notes;
        juce::String badErr;
        hc.request ("create_track", obj ({ { "type", "midi" }, { "name", "Sub Bass" }, { "color", "pink" } }),
                    [&] (bool o, const juce::var& r, const juce::String&) { if (o) created = r; ++done; });
        juce::Array<juce::var> ns;
        for (int i = 0; i < 8; ++i)
            ns.add (obj ({ { "pitch", 29 }, { "start", i * 0.5 }, { "duration", 0.4 }, { "velocity", 110 } }));
        hc.request ("write_clip", obj ({ { "track", 2 }, { "slot", 0 }, { "length", 4 }, { "name", juce::String::fromUTF8 ("Sub \xe2\x80\x94 drop") }, { "notes", ns } }),
                    [&] (bool o, const juce::var& r, const juce::String&) { if (o) written = r; ++done; });
        hc.request ("get_clip_notes", obj ({ { "track", 2 }, { "slot", 0 } }),
                    [&] (bool o, const juce::var& r, const juce::String&) { if (o) notes = r; ++done; });
        hc.request ("set_tempo", obj ({ { "bpm", 5 } }),
                    [&] (bool o, const juce::var&, const juce::String& e) { if (! o) badErr = e; ++done; });
        ok (waitFor ([&] { return done == 4; }, 10000), "four requests answered in order");
        ok ((int) created.getProperty ("index", -1) == 2 && created.getProperty ("color", "") == "#ff39d4", "create_track in the set");
        ok ((int) written.getProperty ("notes", 0) == 8, "write_clip wrote 8 notes");
        ok (notes.getProperty ("name", "") == juce::String::fromUTF8 ("Sub \xe2\x80\x94 drop") && notes.getProperty ("notes", juce::var()).size() == 8,
            "notes read back, with non-English characters intact");
        ok (badErr.contains ("at least"), "Live's refusal comes back as a readable error");

        // a big request (a whole song's worth of notes)
        juce::Array<juce::var> big;
        for (int i = 0; i < 2000; ++i)
            big.add (obj ({ { "pitch", 36 + (i % 24) }, { "start", i * 0.125 }, { "duration", 0.1 }, { "velocity", 100 } }));
        bool bigDone = false;
        int bigNotes = 0;
        hc.request ("write_clip", obj ({ { "track", 0 }, { "slot", 3 }, { "length", 256 }, { "notes", big } }),
                    [&] (bool o, const juce::var& r, const juce::String&) { bigDone = true; if (o) bigNotes = (int) r.getProperty ("notes", 0); });
        ok (waitFor ([&] { return bigDone; }, 15000) && bigNotes == 2000, "a 2000-note clip goes through in one request");
    }

    // ---------------------------------------------------------------- the window: page bridge
    AiProcessor proc;
    auto settingsFile = tmp.getChildFile ("settings2.json");
    AiSettings { "sk-ant-abc", "fast" }.save (settingsFile);
    juce::StringArray sent;
    auto lastOf = [&] (const juce::String& type) -> juce::var
    {
        for (int i = sent.size(); --i >= 0;) { auto v = juce::JSON::parse (sent[i]); if (v["type"] == type) return v; }
        return {};
    };
    {
        AiEditor ed (proc, settingsFile, infoFile);
        ok (ed.isResizable(), "window is resizable");
        ed.onSendForTesting = [&] (const juce::String& s) { sent.add (s); };
        ed.receiveForTesting (obj ({ { "type", "ready" } }));
        auto init = lastOf ("init");
        ok (init["settings"]["apiKey"] == "sk-ant-abc" && init["settings"]["model"] == "fast", "page gets your key and model");

        ed.receiveForTesting (obj ({ { "type", "saveSettings" }, { "apiKey", "sk-ant-new" }, { "model", "best" } }));
        ok (AiSettings::load (settingsFile).apiKey == "sk-ant-new" && AiSettings::load (settingsFile).model == "best", "Settings saves through the plugin");

        ed.receiveForTesting (obj ({ { "type", "helper" }, { "reqId", 7 }, { "cmd", "get_session" }, { "args", juce::var (new juce::DynamicObject()) } }));
        ok (waitFor ([&] { return lastOf ("helperResult").isObject(); }, 8000), "a page request reaches Live and comes back");
        auto hr = lastOf ("helperResult");
        ok ((int) hr["reqId"] == 7 && (bool) hr["ok"] && (double) hr["result"]["tempo"] == 120.0, "with the request number and Live's answer");
        ok (waitFor ([&] { return lastOf ("status").isObject(); }, 5000) && (bool) lastOf ("status")["helper"]["connected"], "status pushed to the page when Live connects");

        auto chat = obj ({ { "history", juce::Array<juce::var> { obj ({ { "role", "user" }, { "content", "make a bassline" } }) } },
                           { "transcript", juce::Array<juce::var> { obj ({ { "role", "user" }, { "text", "make a bassline" } }) } } });
        ed.receiveForTesting (obj ({ { "type", "saveChat" }, { "chat", chat } }));
        ed.setSize (1100, 760);
    }
    // saved with the set and back
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    AiProcessor proc2;
    proc2.setStateInformation (state.getData(), (int) state.getSize());
    auto chat2 = juce::JSON::parse (proc2.getChat());
    ok (chat2["history"][0]["content"] == "make a bassline", "the conversation is saved with the Live set");
    ok (proc2.getEditorSize() == juce::Point<int> (1100, 760), "window size saved with the set");
    {
        sent.clear();
        AiEditor ed2 (proc2, settingsFile, infoFile);
        ed2.onSendForTesting = [&] (const juce::String& s) { sent.add (s); };
        ed2.receiveForTesting (obj ({ { "type", "ready" } }));
        ok (lastOf ("init")["chat"]["transcript"][0]["text"] == "make a bassline", "reopening the window shows the conversation");
    }
    {
        juce::AudioBuffer<float> buf (2, 256);
        for (int i = 0; i < 256; ++i) { buf.setSample (0, i, 0.5f); buf.setSample (1, i, -0.25f); }
        juce::MidiBuffer midi;
        proc.processBlock (buf, midi);
        ok (buf.getSample (0, 100) == 0.5f && buf.getSample (1, 100) == -0.25f, "audio passes through untouched");
    }

    infoDir.getChildFile ("stop").create();
    py.waitForProcessToFinish (5000);
    ok (! infoFile.existsAsFile(), "when Live closes, the helper removes its address");
    tmp.deleteRecursively();
    std::cout << (fails ? juce::String (fails) + " FAILURES" : juce::String ("ALL PLUGIN TESTS PASSED")) << std::endl;
    return fails ? 1 : 0;
}
