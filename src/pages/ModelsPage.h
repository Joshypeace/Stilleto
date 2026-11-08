#pragma once

#include "components/StatusIndicator.h"
#include "pages/Page.h"

namespace stilleto
{

/** Installed models, the active model, and verified downloads from the upstream repository. */
class ModelsPage : public Page,
                   private AppController::Listener,
                   private ModelManager::Listener,
                   private juce::AsyncUpdater
{
public:
    explicit ModelsPage (AppController&);
    ~ModelsPage() override;

    juce::String pageTitle() const override { return "Models"; }
    juce::String pageSubtitle() const override { return "Whisper models run locally. Larger models are more accurate but slower."; }

    void resized() override;
    void pageShown() override;

private:
    class ModelRow;
    class Content;

    // Rebuilding deletes row buttons, so it never runs inside a button's own click callback.
    void modelStatusChanged() override { triggerAsyncUpdate(); }
    void modelsChanged() override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { rebuild(); }
    void downloadProgress (const juce::String& id, float progress, juce::int64 bytes, juce::int64 total) override;
    void downloadFinished (const juce::String& id, bool success, const juce::String& message) override;

    void rebuild();
    void requestDownload (const CatalogModel& model);
    void chooseFolder();

    std::unique_ptr<Content> content;
    juce::Viewport viewport;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastMessage;
    bool lastMessageIsError = false;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

} // namespace stilleto
