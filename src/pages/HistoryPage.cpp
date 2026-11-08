#include "pages/HistoryPage.h"

namespace stilleto
{
using namespace ui;

/** One history entry with hover actions. */
class HistoryPage::Row : public juce::Component
{
public:
    explicit Row (HistoryPage& p) : page (p)
    {
        for (auto* b : { &copy, &insert, &remove })
            addAndMakeVisible (b);

        copy.onClick = [this] { page.controller.copyToClipboard (entry.text); flash = "Copied"; repaint(); };
        insert.onClick = [this] { page.controller.insertIntoLastApplication (entry.text, entry.id); };
        remove.onClick = [this]
        {
            // Deferred: removing rebuilds the list, which may delete this row mid-callback.
            // HistoryManager also rewrites the file without the entry.
            juce::MessageManager::callAsync ([&history = page.controller.getHistory(), id = entry.id, token = page.alive]
            {
                if (token->load()) history.remove (id);
            });
        };
    }

    void setEntry (const HistoryEntry& e)
    {
        entry = e;
        flash.clear();
        const auto app = page.controller.lastApplicationName();
        insert.setTooltip (app.isNotEmpty() ? "Insert again into " + app : "Insert again (click into an app first)");
        insert.setEnabled (app.isNotEmpty() && ! isBusy (page.controller.state()));
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 0).removeFromRight (112).withSizeKeepingCentre (112, 32).translated (0, -10);
        remove.setBounds (r.removeFromRight (32));
        r.removeFromRight (6);
        insert.setBounds (r.removeFromRight (32));
        r.removeFromRight (6);
        copy.setBounds (r.removeFromRight (32));
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (2.0f, 4.0f);
        const bool hover = isMouseOver (true);

        g.setColour (hover ? Colours::elevated : Colours::surface);
        g.fillRoundedRectangle (r, Metrics::radiusMedium);
        g.setColour (hover ? Colours::silver.withAlpha (0.25f) : Colours::border.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), Metrics::radiusMedium, 1.0f);

        auto content = r.reduced (18.0f, 12.0f).withTrimmedRight (126.0f);
        auto meta = content.removeFromBottom (18.0f);

        g.setColour (Colours::text);
        g.setFont (Fonts::body());
        g.drawFittedText (entry.text, content.toNearestInt(), juce::Justification::topLeft, 2, 1.0f);

        juce::String info = relativeTime (entry.timestamp) + "   ·   " + formatDuration (entry.durationSeconds)
                          + "   ·   " + juce::String (entry.wordCount) + " words";
        if (entry.language.isNotEmpty()) info << "   ·   " << entry.language.toUpperCase();
        if (entry.model.isNotEmpty())    info << "   ·   " << entry.model;

        g.setColour (Colours::textMuted);
        g.setFont (Fonts::small());
        g.drawText (info, meta, juce::Justification::centredLeft, true);

        // Insertion status, right aligned under the actions.
        const auto statusArea = r.reduced (18.0f, 12.0f).removeFromRight (240.0f).removeFromBottom (18.0f);
        g.setColour (flash.isNotEmpty() ? Colours::success : entry.inserted ? Colours::success.withAlpha (0.85f) : Colours::textMuted);
        g.drawText (flash.isNotEmpty() ? flash : entry.insertionNote, statusArea, juce::Justification::centredRight, true);
    }

private:
    HistoryPage& page;
    HistoryEntry entry;
    juce::String flash;
    IconButton copy { Icons::Id::Copy, "Copy transcript" };
    IconButton insert { Icons::Id::Insert, "Insert again" };
    IconButton remove { Icons::Id::Trash, "Delete entry", true };
};

//==============================================================================================
HistoryPage::HistoryPage (AppController& c) : Page (c)
{
    addAndMakeVisible (search);
    addAndMakeVisible (list);
    addAndMakeVisible (clearButton);

    search.setTextToShowWhenEmpty ("Search transcripts", Colours::textMuted);
    search.setFont (Fonts::body());
    search.setIndents (40, 0);
    search.setJustification (juce::Justification::centredLeft);
    search.onTextChange = [this] { refresh(); };
    search.setTitle ("Search history");

    list.setRowHeight (92);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setOutlineThickness (0);

    clearButton.onClick = [this] { confirmClearAll(); };

    controller.getHistory().addListener (this);
    refresh();
}

HistoryPage::~HistoryPage()
{
    alive->store (false);
    controller.getHistory().removeListener (this);
}

juce::String HistoryPage::pageSubtitle() const
{
    const auto& h = controller.getHistory();
    if (! h.isPersistenceEnabled()) return "History is turned off - nothing is being saved";
    return juce::String (h.totalDictations()) + " saved on this PC  ·  raw audio is never stored";
}

void HistoryPage::refresh()
{
    filtered = controller.getHistory().search (search.getText());
    list.updateContent();
    list.repaint();
    clearButton.setEnabled (! controller.getHistory().entries().empty());
    repaint();
}

void HistoryPage::confirmClearAll()
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::WarningIcon)
                                      .withTitle ("Clear all history?")
                                      .withMessage ("This permanently deletes all " + juce::String (controller.getHistory().totalDictations())
                                                    + " saved transcripts from this PC. This cannot be undone.")
                                      .withButton ("Delete everything")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [this, token = alive] (int result)
                                  {
                                      if (token->load() && result == 1)
                                          controller.getHistory().clear();
                                  });
}

juce::Component* HistoryPage::refreshComponentForRow (int row, bool, juce::Component* existing)
{
    auto* r = dynamic_cast<Row*> (existing);
    if (! juce::isPositiveAndBelow (row, (int) filtered.size()))
    {
        delete existing;
        return nullptr;
    }

    if (r == nullptr)
    {
        delete existing;
        r = new Row (*this);
    }

    r->setEntry (filtered[static_cast<size_t> (row)]);
    return r;
}

void HistoryPage::resized()
{
    auto r = getLocalBounds().reduced (Metrics::pagePadding, Metrics::pagePadding - 8);
    auto top = r.removeFromTop (40);
    clearButton.setBounds (top.removeFromRight (juce::jmax (130, clearButton.getIdealWidth())));
    top.removeFromRight (Metrics::gap);
    search.setBounds (top.removeFromLeft (juce::jmin (460, top.getWidth())));
    r.removeFromTop (Metrics::gap);
    list.setBounds (r);
}

void HistoryPage::paintOverChildren (juce::Graphics& g)
{
    // Search glyph inside the field (the editor's background would cover it in paint()).
    const auto field = search.getBounds().toFloat();
    Icons::draw (g, Icons::Id::Search, juce::Rectangle<float> (field.getX() + 12.0f, field.getCentreY() - 8.0f, 16.0f, 16.0f),
                 Colours::textMuted, 1.6f);
}

void HistoryPage::paint (juce::Graphics& g)
{
    if (! filtered.empty())
        return;

    const auto area = list.getBounds().toFloat();
    const auto& h = controller.getHistory();

    juce::String title, body;
    if (! h.isPersistenceEnabled())
    {
        title = "History is off";
        body = "Turn on \"Save transcription history\" in Settings > Privacy to keep a searchable record on this PC.";
    }
    else if (h.entries().empty())
    {
        title = "No transcriptions yet";
        body = "Each dictation is saved here with its time, duration, word count and model. Press "
               + controller.hotkeyText() + " in any application to start.";
    }
    else
    {
        title = "No matches";
        body = "Nothing in your history contains \"" + search.getText().trim() + "\".";
    }

    auto box = area.withSizeKeepingCentre (juce::jmin (440.0f, area.getWidth()), 150.0f);
    Icons::draw (g, Icons::Id::History, box.removeFromTop (44.0f).withSizeKeepingCentre (34, 34), Colours::silver.withAlpha (0.6f), 1.4f);
    box.removeFromTop (8.0f);
    g.setColour (Colours::text);
    g.setFont (Fonts::heading());
    g.drawText (title, box.removeFromTop (24.0f), juce::Justification::centred);
    g.setColour (Colours::textMuted);
    g.setFont (Fonts::body());
    g.drawFittedText (body, box.toNearestInt(), juce::Justification::centredTop, 3);
}

} // namespace stilleto
