#include "HelperClient.h"

const char* const HelperClient::notConnectedMessage =
    "The Live helper is not connected. In Live: Settings > Link, Tempo & MIDI > Control Surface > AbletonAi (Input and Output: None).";

juce::File HelperClient::defaultInfoFile()
{
   #if JUCE_MAC
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support");
   #else
    auto base = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
   #endif
    return base.getChildFile ("AbletonAi").getChildFile ("helper.json");
}

HelperClient::HelperClient (juce::File helperInfoFile)
    : juce::Thread ("AbletonAi helper link"), infoFile (std::move (helperInfoFile))
{
    startThread (juce::Thread::Priority::normal);
}

HelperClient::~HelperClient()
{
    alive->store (false);
    signalThreadShouldExit();
    wake.signal();
    stopThread (4000);
    disconnect();
}

void HelperClient::request (const juce::String& cmd, const juce::var& args, Callback whenDone)
{
    {
        const juce::ScopedLock sl (queueLock);
        queue.push_back ({ nextId++, cmd, args, std::move (whenDone) });
    }
    wake.signal();
}

HelperClient::Status HelperClient::getStatus() const
{
    const juce::ScopedLock sl (statusLock);
    return status;
}

void HelperClient::setStatus (bool connected, const juce::String& live, const juce::String& helperVersion, const juce::String& error)
{
    const juce::ScopedLock sl (statusLock);
    status.connected = connected;
    if (connected) { status.live = live; status.helper = helperVersion; }
    status.error = error;
}

// ------------------------------------------------------------------ background thread
void HelperClient::run()
{
    while (! threadShouldExit())
    {
        wake.wait (500);
        if (threadShouldExit())
            break;

        for (;;)
        {
            Request r;
            {
                const juce::ScopedLock sl (queueLock);
                if (queue.empty())
                    break;
                r = std::move (queue.front());
                queue.pop_front();
            }
            juce::var reply;
            juce::String error;
            const bool ok = exchange (r.id, r.cmd, r.args, reply, error, requestTimeoutMs);
            auto cb = r.cb;
            auto flag = alive;
            const juce::var result = ok ? reply.getProperty ("result", juce::var()) : juce::var();
            juce::MessageManager::callAsync ([flag, cb, ok, result, error]
            {
                if (flag->load() && cb)
                    cb (ok, result, error);
            });
            if (threadShouldExit())
                return;
        }

        // keep the connection status fresh for the green dot
        if (juce::Time::getMillisecondCounter() - lastPing > 2000)
        {
            lastPing = juce::Time::getMillisecondCounter();
            juce::var reply;
            juce::String error;
            if (exchange (0, "ping", juce::var (new juce::DynamicObject()), reply, error, 3000))
            {
                auto res = reply.getProperty ("result", juce::var());
                setStatus (true, res.getProperty ("live", "").toString(), res.getProperty ("helper", "").toString(), {});
            }
        }
    }
}

bool HelperClient::connect (juce::String& error)
{
    if (socket != nullptr && socket->isConnected())
        return true;
    disconnect();
    if (! infoFile.existsAsFile())
    {
        error = notConnectedMessage;
        setStatus (false, {}, {}, "helper not running");
        return false;
    }
    auto info = juce::JSON::parse (infoFile.loadFileAsString());
    const int port = (int) info.getProperty ("port", 0);
    token = info.getProperty ("token", "").toString();
    if (port <= 0 || token.isEmpty())
    {
        error = notConnectedMessage;
        setStatus (false, {}, {}, "helper info unreadable");
        return false;
    }
    auto s = std::make_unique<juce::StreamingSocket>();
    if (! s->connect ("127.0.0.1", port, 1500))
    {
        error = notConnectedMessage;
        setStatus (false, {}, {}, "helper not answering");
        return false;
    }
    socket = std::move (s);
    buffered.clear();
    return true;
}

void HelperClient::disconnect()
{
    if (socket != nullptr)
        socket->close();
    socket.reset();
    buffered.clear();
}

bool HelperClient::exchange (juce::int64 id, const juce::String& cmd, const juce::var& args, juce::var& reply, juce::String& error, int timeoutMs)
{
    for (int attempt = 0; attempt < 2; ++attempt)       // one reconnect: Live may have restarted the helper
    {
        if (! connect (error))
            return false;

        auto* o = new juce::DynamicObject();
        o->setProperty ("id", id);
        o->setProperty ("token", token);
        o->setProperty ("cmd", cmd);
        o->setProperty ("args", args.isObject() ? args : juce::var (new juce::DynamicObject()));
        const auto line = (juce::JSON::toString (juce::var (o), true) + "\n").toStdString();

        int sent = 0;
        while (sent < (int) line.size())
        {
            const int n = socket->write (line.data() + sent, (int) line.size() - sent);
            if (n <= 0)
                break;
            sent += n;
        }
        if (sent < (int) line.size())
        {
            disconnect();
            continue;
        }

        const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        char buf[16384];
        while (! threadShouldExit())
        {
            const auto nl = buffered.find ('\n');
            if (nl != std::string::npos)
            {
                const auto text = juce::String::fromUTF8 (buffered.data(), (int) nl);
                buffered.erase (0, nl + 1);
                auto v = juce::JSON::parse (text);
                if ((juce::int64) v.getProperty ("id", -1) != id)
                    continue;                   // an answer to an earlier, timed-out request
                if ((bool) v.getProperty ("ok", false))
                {
                    reply = v;
                    return true;
                }
                error = v.getProperty ("error", "the Live helper refused that").toString();
                if (error == "wrong token")
                {
                    disconnect();               // Live restarted the helper with a new token
                    error.clear();
                    break;
                }
                return false;
            }
            if (juce::Time::getMillisecondCounter() > deadline)
            {
                error = "Live took too long to answer";
                disconnect();
                return false;
            }
            const int ready = socket->waitUntilReady (true, 200);
            if (ready < 0)
            {
                disconnect();
                break;
            }
            if (ready == 0)
                continue;
            const int n = socket->read (buf, (int) sizeof (buf), false);
            if (n <= 0)
            {
                disconnect();
                break;
            }
            buffered.append (buf, (size_t) n);
        }
        if (threadShouldExit())
            return false;
    }
    if (error.isEmpty())
        error = notConnectedMessage;
    setStatus (false, {}, {}, error);
    return false;
}
