#pragma once

#include <juce_core/juce_core.h>

/*  Serves the player page on http://127.0.0.1:<port>/<token>.

    YouTube's embedded player refuses to run on a page without a real web address
    ("Error 153"). Loading the page from this tiny local server gives it one, the
    same way the Max for Live Tube Player does. It only answers on this computer
    (127.0.0.1) and only serves that one page. */
class PageServer : private juce::Thread
{
public:
    explicit PageServer (juce::MemoryBlock pageUtf8);
    ~PageServer() override;

    bool start();                      // false if no port could be opened
    bool isRunning() const { return port > 0 && isThreadRunning(); }
    juce::String getURL() const;       // http://127.0.0.1:port/token
    int getPort() const { return port; }

private:
    void run() override;
    void handle (juce::StreamingSocket& client);
    static bool writeAll (juce::StreamingSocket& s, const void* data, int size);

    juce::MemoryBlock page;
    juce::StreamingSocket listener;
    juce::String token;
    int port = 0;

    JUCE_DECLARE_NON_COPYABLE (PageServer)
};
