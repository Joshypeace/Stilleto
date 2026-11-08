#pragma once

#include "components/MicOrb.h"
#include "components/StatusIndicator.h"
#include "components/WaveformView.h"
#include "pages/Page.h"

namespace stilleto
{

/** The dictation focal point: large mic control, live waveform, timer, state and transcript. */
class DictationPage : public Page,
                      private AppController::Listener,
                      private juce::Timer
{
public:
    explicit DictationPage (AppController&);
    ~DictationPage() override;

    juce::String pageTitle() const override { return "Dictation"; }
    juce::String pageSubtitle() const override { return "Record, transcribe locally, and insert anywhere"; }

    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void pageShown() override { sessionChanged(); }

private:
    void sessionChanged() override;
    void modelStatusChanged() override { sessionChanged(); }
    void timerCallback() override;

    ui::Card stage, transcriptCard { "Transcript", "Review, copy or send it to another application" };
    ui::MicOrb orb;
    ui::WaveformView waveform;
    ui::StatusIndicator stateBadge;
    juce::TextEditor transcript;
    ui::StilletoButton copyButton { "Copy", ui::StilletoButton::Style::Secondary, ui::Icons::Id::Copy };
    ui::StilletoButton insertButton { "Insert", ui::StilletoButton::Style::Secondary, ui::Icons::Id::Insert };
    ui::StilletoButton cancelButton { "Cancel", ui::StilletoButton::Style::Ghost, ui::Icons::Id::Close };
    ui::ToggleSwitch autoInsert;
    juce::Label autoInsertLabel;
    juce::Rectangle<int> stageTextArea;
    juce::String feedback;
    juce::uint32 feedbackUntil = 0;
};

} // namespace stilleto
