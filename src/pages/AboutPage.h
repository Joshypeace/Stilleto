#pragma once

#include "pages/Page.h"

namespace stilleto
{

/** How Stilleto works, its limitations, and diagnostics for support. */
class AboutPage : public Page
{
public:
    explicit AboutPage (AppController&);

    juce::String pageTitle() const override { return "Help & About"; }
    juce::String pageSubtitle() const override { return "Version " + juce::JUCEApplication::getInstance()->getApplicationVersion(); }

    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    ui::Card brandCard, stepsCard { "How dictation works" }, limitsCard { "Good to know" }, diagnosticsCard { "Diagnostics" };
    ui::StilletoButton dataFolder { "Open data folder", ui::StilletoButton::Style::Secondary, ui::Icons::Id::Folder };
    ui::StilletoButton logFile { "Open log", ui::StilletoButton::Style::Ghost };
    juce::String systemInfo;
};

} // namespace stilleto
