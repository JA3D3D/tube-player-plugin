#include "PageServer.h"

PageServer::PageServer (juce::MemoryBlock pageUtf8)
    : juce::Thread ("Tube Player page"), page (std::move (pageUtf8))
{
    juce::Random r;
    for (int i = 0; i < 24; ++i)
        token << juce::String::charToString ((juce::juce_wchar) ("abcdefghijklmnopqrstuvwxyz0123456789"[r.nextInt (36)]));
}

PageServer::~PageServer()
{
    signalThreadShouldExit();
    stopThread (3000);     // the loop wakes every 200 ms, so this returns quickly
    listener.close();
}

bool PageServer::start()
{
    juce::Random r;
    for (int attempt = 0; attempt < 20 && port == 0; ++attempt)
    {
        const int candidate = 49152 + r.nextInt (16000);
        if (listener.createListener (candidate, "127.0.0.1"))
            port = listener.getBoundPort() > 0 ? listener.getBoundPort() : candidate;
    }
    if (port == 0)
        return false;
    startThread (juce::Thread::Priority::low);
    return true;
}

juce::String PageServer::getURL() const
{
    return "http://127.0.0.1:" + juce::String (port) + "/" + token;
}

void PageServer::run()
{
    while (! threadShouldExit())
    {
        // poll instead of blocking in accept(), so the plugin can always shut this down promptly
        if (listener.waitUntilReady (true, 200) != 1)
            continue;
        std::unique_ptr<juce::StreamingSocket> client (listener.waitForNextConnection());
        if (client != nullptr)
            handle (*client);
    }
}

bool PageServer::writeAll (juce::StreamingSocket& s, const void* data, int size)
{
    auto* p = static_cast<const char*> (data);
    while (size > 0)
    {
        if (s.waitUntilReady (false, 2000) != 1)
            return false;
        const int n = s.write (p, size);
        if (n <= 0)
            return false;
        p += n;
        size -= n;
    }
    return true;
}

void PageServer::handle (juce::StreamingSocket& client)
{
    juce::MemoryOutputStream req;
    char buf[2048];
    for (int i = 0; i < 16; ++i)
    {
        if (client.waitUntilReady (true, 1000) != 1)
            break;
        const int n = client.read (buf, (int) sizeof (buf), false);
        if (n <= 0)
            break;
        req.write (buf, (size_t) n);
        if (req.toString().contains ("\r\n\r\n") || req.getDataSize() > 16384)
            break;
    }

    const auto text = req.toString();
    const auto line = text.upToFirstOccurrenceOf ("\r\n", false, false);
    const auto target = line.fromFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf (" ", false, false);
    const auto path = target.upToFirstOccurrenceOf ("?", false, false);
    const bool ok = line.startsWith ("GET ") && path == "/" + token;

    juce::String head;
    if (ok)
        head << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << (int) page.getSize()
             << "\r\nCache-Control: no-store\r\nReferrer-Policy: strict-origin-when-cross-origin\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n";
    else
        head << "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 9\r\nConnection: close\r\n\r\nnot found";

    const auto h = head.toStdString();
    if (writeAll (client, h.data(), (int) h.size()) && ok)
        writeAll (client, page.getData(), (int) page.getSize());
    client.close();
}
