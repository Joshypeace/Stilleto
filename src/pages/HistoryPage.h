#pragma once

#include "pages/Page.h"

namespace stilleto
{

/** Searchable local history with copy, insert-again and delete. */
class HistoryPage : public Page,
                    private HistoryManager::Listener,
                    private juce::ListBoxModel
{
public:
    explicit HistoryPage (AppController&);
    ~HistoryPage() override;

    juce::String pageTitle() const override { return "Transcription History"; }
    juce::String pageSubtitle() const override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void pageShown() override { refresh(); }

private:
    class Row;

    void historyChanged() override { refresh(); }
    void refresh();
    void confirmClearAll();

    int getNumRows() override { return static_cast<int> (filtered.size()); }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;

    juce::TextEditor search;
    juce::ListBox list { "History", this };
    ui::StilletoButton clearButton { "Clear all", ui::StilletoButton::Style::Danger, ui::Icons::Id::Trash };
    std::vector<HistoryEntry> filtered;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

} // namespace stilleto
