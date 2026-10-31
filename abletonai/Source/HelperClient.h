#pragma once

#include <juce_events/juce_events.h>
#include <deque>
#include <string>

/*  Talks to the AbletonAi Live helper (the control surface script running inside Live).

    The helper writes its port and a secret token to Application Support/AbletonAi/helper.json.
    Requests go out one at a time on a background thread, one JSON line each way, so a slow
    command in Live never freezes the plugin window. Answers come back on the message thread. */
class HelperClient : private juce::Thread
{
public:
    using Callback = std::function<void (bool ok, const juce::var& result, const juce::String& error)>;

    struct Status
    {
        bool connected = false;
        juce::String live, helper, error;
    };

    explicit HelperClient (juce::File helperInfoFile = defaultInfoFile());
    ~HelperClient() override;

    void request (const juce::String& cmd, const juce::var& args, Callback whenDone);
    Status getStatus() const;
    static juce::File defaultInfoFile();

    static constexpr int requestTimeoutMs = 40000;
    static const char* const notConnectedMessage;

private:
    struct Request { juce::int64 id; juce::String cmd; juce::var args; Callback cb; };

    void run() override;
    bool connect (juce::String& error);
    void disconnect();
    bool exchange (juce::int64 id, const juce::String& cmd, const juce::var& args, juce::var& reply, juce::String& error, int timeoutMs);
    void setStatus (bool connected, const juce::String& live, const juce::String& helperVersion, const juce::String& error);

    juce::File infoFile;
    juce::CriticalSection queueLock;
    std::deque<Request> queue;
    juce::WaitableEvent wake;
    std::unique_ptr<juce::StreamingSocket> socket;
    juce::String token;
    std::string buffered;           // raw bytes: a character can be split between two reads
    std::atomic<juce::int64> nextId { 1 };
    juce::uint32 lastPing = 0;

    mutable juce::CriticalSection statusLock;
    Status status;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE (HelperClient)
};
