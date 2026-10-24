// Stand-in for systems without the macOS web view (used for the Linux compile check).
#include "TubeWebView.h"

struct TubeWebView::Impl
{
    juce::Label label { {}, "The video panel needs macOS in this version." };
};

TubeWebView::TubeWebView (Callbacks) : impl (std::make_unique<Impl>())
{
    impl->label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (impl->label);
}

TubeWebView::~TubeWebView() = default;
void TubeWebView::loadURL (const juce::String&) {}
void TubeWebView::loadHTML (const juce::String&, const juce::String&) {}
void TubeWebView::evaluate (const juce::String&) {}
void TubeWebView::resized() { impl->label.setBounds (getLocalBounds()); }
bool TubeWebView::isSupported() { return false; }

void TubeWebView::clearWebsiteData (std::function<void()> whenDone)
{
    juce::MessageManager::callAsync (std::move (whenDone));
}
