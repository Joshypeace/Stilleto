#pragma once

#include "components/HotkeyEditor.h"
#include "pages/Page.h"

namespace stilleto
{

/** All preferences, grouped into sections. Every control writes through SettingsManager, which
    persists the value and notifies the controller. */
class SettingsPage : public Page,
                     private AppController::Listener,
                     private SettingsManager::Listener,
                     private juce::Timer
{
public:
    explicit SettingsPage (AppController&);
    ~SettingsPage() override;

    juce::String pageTitle() const override { return "Settings"; }
    juce::String pageSubtitle() const override { return "Changes are saved automatically"; }

    void resized() override;
    void pageShown() override;
    void pageHidden() override;

private:
    class Section;
    class Content;

    void hotkeyStatusChanged() override;
    void microphoneStatusChanged() override { loadValues(); }
    void modelStatusChanged() override { loadValues(); }
    void settingChanged (const juce::Identifier&) override { loadValues(); }
    void timerCallback() override;

    void loadValues();
    void setMonitoring (bool on);
    void confirmClearHistory();

    std::unique_ptr<Content> content;
    juce::Viewport viewport;
    bool loading = false;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

} // namespace stilleto
