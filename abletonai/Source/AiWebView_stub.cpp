// Stand-in for systems without the macOS web view (used for the Linux compile check).
#include "AiWebView.h"

struct AiWebView::Impl
{
    juce::Label label { {}, "AbletonAi needs macOS in this version." };
};

AiWebView::AiWebView (Callbacks) : impl (std::make_unique<Impl>())
{
    impl->label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (impl->label);
}

AiWebView::~AiWebView() = default;
void AiWebView::loadURL (const juce::String&) {}
void AiWebView::loadHTML (const juce::String&, const juce::String&) {}
void AiWebView::evaluate (const juce::String&) {}
void AiWebView::resized() { impl->label.setBounds (getLocalBounds()); }
bool AiWebView::isSupported() { return false; }

void AiWebView::clearWebsiteData (std::function<void()> whenDone)
{
    juce::MessageManager::callAsync (std::move (whenDone));
}
