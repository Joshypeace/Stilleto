#include "pages/ModelsPage.h"

namespace stilleto
{
using namespace ui;

namespace
{
juce::String str (std::string_view s) { return juce::String (std::string (s)); }
}

/** A row in either list: name, description, meta line, and right-aligned actions. */
class ModelsPage::ModelRow : public juce::Component
{
public:
    juce::String title, description, meta;
    bool active = false;
    float progress = -2.0f;   // < -1: not downloading; -1: indeterminate; 0..1: progress
    juce::String progressText;

    StilletoButton primary { "Use", StilletoButton::Style::Secondary };
    IconButton secondary { Icons::Id::Trash, "Remove", true };

    ModelRow()
    {
        addChildComponent (primary);
        addChildComponent (secondary);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18, 0);
        auto right = r.removeFromRight (190);
        if (secondary.isVisible())
        {
            secondary.setBounds (right.removeFromRight (34).withSizeKeepingCentre (34, 34));
            right.removeFromRight (8);
        }
        primary.setBounds (right.removeFromRight (juce::jmax (96, primary.getIdealWidth())).withSizeKeepingCentre (juce::jmax (96, primary.getIdealWidth()), 36));
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.0f, 4.0f);
        g.setColour (active ? Colours::gold.withAlpha (0.06f) : Colours::background2.withAlpha (0.6f));
        g.fillRoundedRectangle (r, Metrics::radiusMedium);
        g.setColour (active ? Colours::gold.withAlpha (0.45f) : Colours::border.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), Metrics::radiusMedium, 1.0f);

        auto c = r.reduced (18.0f, 12.0f).withTrimmedRight (200.0f);

        auto titleRow = c.removeFromTop (22.0f);
        g.setColour (Colours::text);
        g.setFont (Fonts::bodyStrong());
        g.drawText (title, titleRow, juce::Justification::centredLeft, true);

        if (active)
        {
            const float w = (float) juce::GlyphArrangement::getStringWidthInt (Fonts::bodyStrong(), title);
            const auto badge = juce::Rectangle<float> (titleRow.getX() + w + 10.0f, titleRow.getCentreY() - 10.0f, 58.0f, 20.0f);
            g.setColour (Colours::gold.withAlpha (0.15f));
            g.fillRoundedRectangle (badge, 10.0f);
            g.setColour (Colours::gold);
            g.setFont (Fonts::caption());
            g.drawText ("ACTIVE", badge, juce::Justification::centred);
        }

        g.setColour (Colours::textSecondary);
        g.setFont (Fonts::small());
        g.drawText (description, c.removeFromTop (20.0f), juce::Justification::centredLeft, true);

        if (progress > -1.5f)
        {
            auto bar = c.removeFromTop (16.0f).withHeight (4.0f).translated (0, 7.0f).withWidth (juce::jmin (c.getWidth(), 320.0f));
            g.setColour (Colours::border);
            g.fillRoundedRectangle (bar, 2.0f);
            if (progress >= 0.0f)
            {
                g.setGradientFill (goldGradient (bar));
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, progress)), 2.0f);
            }
            g.setColour (Colours::textMuted);
            g.drawText (progressText, c.removeFromTop (18.0f), juce::Justification::centredLeft, true);
        }
        else
        {
            g.setColour (Colours::textMuted);
            g.setFont (Fonts::mono (11.5f));
            g.drawText (meta, c.removeFromTop (20.0f), juce::Justification::centredLeft, true);
        }
    }
};

/** Scrollable page body: active model summary, installed models, catalog. */
class ModelsPage::Content : public juce::Component
{
public:
    explicit Content (ModelsPage& p) : page (p)
    {
        addAndMakeVisible (activeCard);
        addAndMakeVisible (installedCard);
        addAndMakeVisible (catalogCard);
        activeCard.addAndMakeVisible (status);
        activeCard.addAndMakeVisible (openFolder);
        activeCard.addAndMakeVisible (changeFolder);
        activeCard.addAndMakeVisible (rescan);

        openFolder.onClick = [this]
        {
            auto dir = page.controller.getModels().userDirectory();
            dir.createDirectory();
            dir.startAsProcess();
        };
        changeFolder.onClick = [this] { page.chooseFolder(); };
        rescan.onClick = [this] { page.controller.getModels().rescan(); };
    }

    juce::OwnedArray<ModelRow> installedRows, catalogRows;
    Card activeCard { "Active model" }, installedCard { "Installed models" },
         catalogCard { "Download models", "From huggingface.co/ggerganov/whisper.cpp - verified against the published SHA-1" };
    StatusIndicator status;
    StilletoButton openFolder { "Open folder", StilletoButton::Style::Secondary, Icons::Id::Folder };
    StilletoButton changeFolder { "Change folder", StilletoButton::Style::Ghost };
    StilletoButton rescan { "Rescan", StilletoButton::Style::Ghost, Icons::Id::Refresh };
    juce::String activeName, activePath, activeDetail;

    int layout (int width)
    {
        const int pad = Metrics::cardPadding;
        int y = 0;

        activeCard.setBounds (0, y, width, 150);
        status.setBounds (pad, 56, juce::jmax (140, status.getIdealWidth()), 26);
        auto buttons = juce::Rectangle<int> (pad, 150 - pad - 36, width - pad * 2, 36);
        openFolder.setBounds (buttons.removeFromRight (openFolder.getIdealWidth()));
        buttons.removeFromRight (8);
        changeFolder.setBounds (buttons.removeFromRight (changeFolder.getIdealWidth()));
        buttons.removeFromRight (8);
        rescan.setBounds (buttons.removeFromRight (rescan.getIdealWidth()));
        y += 150 + Metrics::gap;

        auto placeRows = [&] (Card& card, juce::OwnedArray<ModelRow>& rows, bool hasSubtitle, int emptyHeight)
        {
            const int header = pad + (hasSubtitle ? 50 : 32);
            const int rowsHeight = rows.isEmpty() ? emptyHeight : rows.size() * 86;
            card.setBounds (0, y, width, header + rowsHeight + pad);
            int ry = header;
            for (auto* row : rows)
            {
                row->setBounds (pad, ry, width - pad * 2, 82);
                ry += 86;
            }
            y += card.getHeight() + Metrics::gap;
        };

        placeRows (installedCard, installedRows, false, 60);
        placeRows (catalogCard, catalogRows, true, 60);
        return y;
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        auto r = activeCard.getBounds().reduced (Metrics::cardPadding);
        r.removeFromTop (32);
        auto line = r.removeFromTop (26).withTrimmedLeft (status.getRight() + 12 - activeCard.getX());
        g.setColour (Colours::text);
        g.setFont (Fonts::make (19.0f, "Semibold"));
        g.drawText (activeName, line, juce::Justification::centredLeft, true);

        r.removeFromTop (6);
        g.setColour (Colours::textMuted);
        g.setFont (Fonts::mono (11.5f));
        g.drawText (activePath, r.removeFromTop (18).withTrimmedRight (420), juce::Justification::centredLeft, true);
        g.setFont (Fonts::small());
        g.setColour (page.lastMessageIsError ? Colours::error : Colours::textSecondary);
        g.drawText (page.lastMessage.isNotEmpty() ? page.lastMessage : activeDetail,
                    r.removeFromTop (20).withTrimmedRight (420), juce::Justification::centredLeft, true);

        auto emptyText = [&] (const Card& card, const juce::OwnedArray<ModelRow>& rows, const juce::String& text)
        {
            if (! rows.isEmpty()) return;
            g.setColour (Colours::textMuted);
            g.setFont (Fonts::body());
            g.drawText (text, card.getBounds().withTrimmedTop (card.getHeight() - 60 - Metrics::cardPadding).withHeight (60).reduced (Metrics::cardPadding, 0),
                        juce::Justification::centredLeft, true);
        };
        emptyText (installedCard, installedRows, "No models found. Download one below - Base (English) is recommended.");
        emptyText (catalogCard, catalogRows, "Every catalog model is already installed.");
    }

private:
    ModelsPage& page;
};

//==============================================================================================
ModelsPage::ModelsPage (AppController& c) : Page (c), content (std::make_unique<Content> (*this))
{
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    controller.addListener (this);
    controller.getModels().addListener (this);
    rebuild();
}

ModelsPage::~ModelsPage()
{
    alive->store (false);
    cancelPendingUpdate();
    controller.removeListener (this);
    controller.getModels().removeListener (this);
}

void ModelsPage::pageShown()
{
    controller.getModels().rescan();
}

void ModelsPage::rebuild()
{
    auto& models = controller.getModels();
    auto& c = *content;

    c.activeName = controller.modelName();
    c.activePath = controller.activeModelFile() != juce::File() ? controller.activeModelFile().getFullPathName() : models.userDirectory().getFullPathName();

    switch (controller.modelStatus())
    {
        case AppController::ModelStatus::Ready:
            c.status.setStatus ("Loaded", StatusIndicator::Tone::Good);
            c.activeDetail = "Loaded once and kept in memory for fast dictation.";
            break;
        case AppController::ModelStatus::Loading:
            c.status.setStatus ("Loading", StatusIndicator::Tone::Busy, true);
            c.activeDetail = "Loading on a background thread...";
            break;
        case AppController::ModelStatus::Failed:
            c.status.setStatus ("Failed", StatusIndicator::Tone::Bad);
            c.activeDetail = controller.modelMessage();
            break;
        case AppController::ModelStatus::NoModel:
            c.status.setStatus ("Not installed", StatusIndicator::Tone::Warning);
            c.activeName = "No model selected";
            c.activeDetail = "Download a model below to start dictating.";
            break;
    }

    // Installed models.
    c.installedRows.clear();
    for (const auto& m : models.installed())
    {
        auto* row = c.installedRows.add (new ModelRow());
        c.installedCard.addAndMakeVisible (row);

        row->title = m.displayName;
        row->description = m.catalog != nullptr ? str (m.catalog->description) : "Custom model file";
        juce::String location = m.inUserDirectory ? "Stilleto folder" : m.file.getParentDirectory().getFullPathName();
        row->meta = m.file.getFileName() + "  " + ui::middot() + "  " + juce::String (formatBytes ((std::uint64_t) m.sizeBytes)) + "  " + ui::middot() + "  " + location;
        row->active = m.file == controller.activeModelFile();

        if (m.status != ModelFileStatus::Valid)
        {
            row->description = juce::String (std::string (describe (m.status)));
            row->primary.setVisible (false);
        }
        else
        {
            row->primary.setButtonText (row->active ? "In use" : "Use");
            row->primary.setStyle (row->active ? StilletoButton::Style::Ghost : StilletoButton::Style::Secondary);
            row->primary.setEnabled (! row->active);
            row->primary.setVisible (true);
            row->primary.onClick = [this, file = m.file] { controller.activateModel (file); };
        }

        row->secondary.setVisible (m.inUserDirectory && ! row->active);
        row->secondary.onClick = [this, file = m.file, name = m.displayName]
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                              .withIconType (juce::MessageBoxIconType::WarningIcon)
                                              .withTitle ("Remove " + name + "?")
                                              .withMessage ("The model file will be deleted from " + file.getParentDirectory().getFullPathName() + ".")
                                              .withButton ("Remove")
                                              .withButton ("Cancel")
                                              .withAssociatedComponent (this),
                                          [this, file, token = alive] (int r)
                                          {
                                              if (! token->load() || r != 1) return;
                                              juce::String error;
                                              if (! controller.getModels().removeModel (file, error))
                                              {
                                                  lastMessage = error;
                                                  lastMessageIsError = true;
                                                  content->repaint();
                                              }
                                          });
        };
    }

    // Catalog models that are not installed yet.
    c.catalogRows.clear();
    for (const auto& m : ModelCatalog::all())
    {
        if (models.findInstalled (juce::String (m.fileName())) != nullptr)
            continue;

        auto* row = c.catalogRows.add (new ModelRow());
        c.catalogCard.addAndMakeVisible (row);
        row->title = str (m.displayName);
        row->description = str (m.description);
        row->meta = juce::String (m.fileName()) + "  " + ui::middot() + "  " + juce::String (formatBytes (m.approximateBytes))
                    + (m.englishOnly ? "  " + ui::middot() + "  English" : "  " + ui::middot() + "  99 languages");

        const bool downloadingThis = models.downloadingId() == str (m.id);
        if (downloadingThis)
        {
            row->progress = -1.0f;
            row->progressText = "Starting download...";
        }

        row->primary.setButtonText (downloadingThis ? "Cancel" : "Download");
        row->primary.setIcon (downloadingThis ? std::nullopt : std::optional<Icons::Id> (Icons::Id::Download));
        row->primary.setStyle (downloadingThis ? StilletoButton::Style::Ghost : StilletoButton::Style::Secondary);
        row->primary.setEnabled (downloadingThis || ! models.isDownloading());
        row->primary.setVisible (true);
        row->primary.onClick = [this, model = &m]
        {
            if (controller.getModels().downloadingId() == str (model->id)) controller.getModels().cancelDownload();
            else requestDownload (*model);
        };
        row->primary.getProperties().set ("modelId", str (m.id));
    }

    resized();
    content->repaint();
}

void ModelsPage::requestDownload (const CatalogModel& model)
{
    const auto size = juce::String (formatBytes (model.approximateBytes));
    const bool large = model.approximateBytes > ModelCatalog::largeModelThresholdBytes;

    juce::String message = "Stilleto will download " + str (model.displayName) + " (" + size + ") from huggingface.co "
                           "into " + controller.getModels().userDirectory().getFullPathName() + ".";
    if (large)
        message << "\n\nThis is a large model: it needs " << size << " of disk space, and transcription will be noticeably slower on CPU.";

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Download " + str (model.displayName) + "?")
                                      .withMessage (message)
                                      .withButton ("Download " + size)
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [this, id = str (model.id), token = alive] (int r)
                                  {
                                      if (! token->load() || r != 1) return;
                                      const auto* m = ModelCatalog::findById (id.toStdString());
                                      if (m == nullptr) return;

                                      juce::String error;
                                      if (! controller.getModels().startDownload (*m, error))
                                      {
                                          lastMessage = error;
                                          lastMessageIsError = true;
                                      }
                                      else
                                      {
                                          lastMessage.clear();
                                      }
                                      rebuild();
                                  });
}

void ModelsPage::downloadProgress (const juce::String& id, float progress, juce::int64 bytes, juce::int64 total)
{
    for (auto* row : content->catalogRows)
    {
        if (row->primary.getProperties()["modelId"].toString() != id)
            continue;

        row->progress = progress;
        if (bytes == 0 && total == 0 && progress >= 1.0f)
            row->progressText = "Verifying checksum...";
        else if (total > 0)
            row->progressText = juce::String (formatBytes ((std::uint64_t) bytes)) + " of " + juce::String (formatBytes ((std::uint64_t) total))
                                + "  (" + juce::String (juce::roundToInt (progress * 100.0f)) + "%)";
        else
            row->progressText = juce::String (formatBytes ((std::uint64_t) bytes)) + " downloaded";
        row->repaint();
    }
}

void ModelsPage::downloadFinished (const juce::String& id, bool success, const juce::String& message)
{
    lastMessage = message;
    lastMessageIsError = ! success;

    // Offer to switch to the newly installed model if nothing usable is active.
    if (success && controller.modelStatus() != AppController::ModelStatus::Ready)
        if (const auto* m = ModelCatalog::findById (id.toStdString()))
            if (const auto* installed = controller.getModels().findInstalled (juce::String (m->fileName())))
                controller.activateModel (installed->file);

    triggerAsyncUpdate();
}

void ModelsPage::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose the folder that holds your Whisper models",
                                                   controller.getModels().userDirectory());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this, token = alive] (const juce::FileChooser& fc)
                          {
                              if (! token->load()) return;
                              const auto dir = fc.getResult();
                              if (dir.isDirectory())
                                  controller.getSettings().setModelDirectory (dir);
                          });
}

void ModelsPage::resized()
{
    viewport.setBounds (getLocalBounds().reduced (Metrics::pagePadding - 4, Metrics::pagePadding - 8));
    const int width = viewport.getWidth() - viewport.getScrollBarThickness() - 8;
    const int height = content->layout (width);
    content->setSize (width, height);
}

} // namespace stilleto
