#pragma once

#include "transcription/ModelCatalog.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>

namespace stilleto
{

struct InstalledModel
{
    juce::File file;
    juce::String displayName;
    juce::int64 sizeBytes = 0;
    const CatalogModel* catalog = nullptr;   // nullptr for custom models
    ModelFileStatus status = ModelFileStatus::Missing;
    bool inUserDirectory = false;            // removable from the UI
};

/** Finds installed whisper models and downloads catalog models from the upstream repository.

    Downloads stream to "<name>.partial" with real progress from the HTTP transfer, are verified
    against the published SHA-1 on a background thread, and are renamed into place only after
    verification succeeds, so a model is never reported as installed before it is usable. */
class ModelManager : private juce::URL::DownloadTaskListener
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void modelsChanged() {}
        virtual void downloadProgress (const juce::String& modelId, float progress, juce::int64 bytes, juce::int64 total) { juce::ignoreUnused (modelId, progress, bytes, total); }
        virtual void downloadFinished (const juce::String& modelId, bool success, const juce::String& message) { juce::ignoreUnused (modelId, success, message); }
    };

    explicit ModelManager (juce::File userModelDirectory);
    ~ModelManager() override;

    void setUserDirectory (const juce::File& dir);
    [[nodiscard]] const juce::File& userDirectory() const noexcept { return userDir; }

    /** Rescans every search location (user directory, next to the executable, and a repository
        "models" folder found above the executable in development builds). */
    void rescan();
    [[nodiscard]] const juce::Array<InstalledModel>& installed() const noexcept { return models; }
    [[nodiscard]] const InstalledModel* findInstalled (const juce::String& fileName) const;
    [[nodiscard]] juce::Array<juce::File> searchDirectories() const;

    bool startDownload (const CatalogModel& model, juce::String& error);
    void cancelDownload();
    [[nodiscard]] bool isDownloading() const noexcept { return task != nullptr || verifying; }
    [[nodiscard]] juce::String downloadingId() const { return currentId; }

    bool removeModel (const juce::File& file, juce::String& error);

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

private:
    void progress (juce::URL::DownloadTask*, juce::int64 downloaded, juce::int64 total) override;
    void finished (juce::URL::DownloadTask*, bool success) override;
    void completeDownload (bool success, const juce::String& message);

    juce::File userDir;
    juce::Array<InstalledModel> models;
    juce::ListenerList<Listener> listeners;

    std::unique_ptr<juce::URL::DownloadTask> task;
    juce::String currentId;
    juce::File partialFile, finalFile;
    std::string expectedSha1;
    std::atomic<bool> verifying { false };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

} // namespace stilleto
