#pragma once

#include "components/StatCard.h"
#include "pages/Page.h"

namespace stilleto
{

/** Dashboard home: primary action, readiness, real usage metrics and recent activity. */
class OverviewPage : public Page,
                     private AppController::Listener,
                     private HistoryManager::Listener
{
public:
    explicit OverviewPage (AppController&);
    ~OverviewPage() override;

    juce::String pageTitle() const override { return "Overview"; }
    juce::String pageSubtitle() const override;

    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void pageShown() override { refresh(); }

private:
    void sessionChanged() override        { refresh(); }
    void modelStatusChanged() override    { refresh(); }
    void hotkeyStatusChanged() override   { refresh(); }
    void microphoneStatusChanged() override { refresh(); }
    void historyChanged() override        { refresh(); }
    void refresh();

    struct Readiness { ui::Icons::Id icon; juce::String label, value; juce::Colour tone; };
    [[nodiscard]] juce::Array<Readiness> readiness() const;

    ui::Card hero, readinessCard { "Readiness", "Everything Stilleto needs to type for you" };
    ui::Card recentCard { "Latest transcription" }, activityCard { "Recent activity" };
    ui::StatCard dictations { ui::Icons::Id::Dictation, "Dictations" };
    ui::StatCard words { ui::Icons::Id::Words, "Words dictated" };
    ui::StatCard minutes { ui::Icons::Id::Clock, "Time spoken" };

    ui::StilletoButton startButton { "Start dictation", ui::StilletoButton::Style::Primary, ui::Icons::Id::Mic };
    ui::StilletoButton modelsButton { "Get a model", ui::StilletoButton::Style::Secondary, ui::Icons::Id::Download };
    ui::IconButton copyButton { ui::Icons::Id::Copy, "Copy transcript" };
    ui::IconButton insertButton { ui::Icons::Id::Insert, "Insert into the last application" };
    juce::Rectangle<int> recentTextArea, activityArea;
    juce::String copyFeedback;
    bool copyFeedbackIsError = false;
    juce::uint32 copyFeedbackUntil = 0;
};

} // namespace stilleto
