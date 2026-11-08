#include "transcription/ModelManager.h"

#include "core/Sha1.h"

#include <thread>

namespace stilleto
{
namespace
{
juce::File executableDirectory()
{
    return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
}

/** In development builds the executable lives under out/build/...; walk up to find the repo's
    models folder so a model downloaded into the source tree is picked up automatically. */
juce::File findRepositoryModels()
{
    auto dir = executableDirectory();
    for (int i = 0; i < 7 && dir.exists(); ++i)
    {
        const auto candidate = dir.getChildFile ("models");
        if (candidate.isDirectory() && dir.getChildFile ("CMakeLists.txt").existsAsFile())
            return candidate;
        dir = dir.getParentDirectory();
    }
    return {};
}
} // namespace

ModelManager::ModelManager (juce::File userModelDirectory) : userDir (std::move (userModelDirectory))
{
    rescan();
}

ModelManager::~ModelManager()
{
    alive->store (false);
    task.reset();   // cancels an in-flight transfer
    if (partialFile.existsAsFile())
        partialFile.deleteFile();
}

void ModelManager::setUserDirectory (const juce::File& dir)
{
    userDir = dir;
    rescan();
}

juce::Array<juce::File> ModelManager::searchDirectories() const
{
    juce::Array<juce::File> dirs;
    auto addUnique = [&] (const juce::File& d)
    {
        if (d != juce::File() && ! dirs.contains (d))
            dirs.add (d);
    };

    addUnique (userDir);
    addUnique (executableDirectory().getChildFile ("models"));
    addUnique (findRepositoryModels());
    return dirs;
}

void ModelManager::rescan()
{
    models.clear();

    for (const auto& dir : searchDirectories())
    {
        if (! dir.isDirectory())
            continue;

        for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*.bin", juce::File::findFiles))
        {
            const auto file = entry.getFile();
            if (findInstalled (file.getFileName()) != nullptr)
                continue;   // first location wins (user directory has priority)

            InstalledModel m;
            m.file = file;
            m.sizeBytes = file.getSize();
            m.catalog = ModelCatalog::findByFileName (file.getFileName().toStdString());
            m.status = inspectModelFile (std::filesystem::path (file.getFullPathName().toWideCharPointer()));
            m.inUserDirectory = dir == userDir;
            m.displayName = m.catalog != nullptr ? juce::String (std::string (m.catalog->displayName))
                                                 : file.getFileNameWithoutExtension().fromFirstOccurrenceOf ("ggml-", false, true);
            if (m.displayName.isEmpty())
                m.displayName = file.getFileNameWithoutExtension();

            if (m.status == ModelFileStatus::BadMagic)
                continue;   // some other .bin file, not a whisper model

            models.add (m);
        }
    }

    listeners.call ([] (Listener& l) { l.modelsChanged(); });
}

const InstalledModel* ModelManager::findInstalled (const juce::String& fileName) const
{
    for (const auto& m : models)
        if (m.file.getFileName().equalsIgnoreCase (fileName))
            return &m;
    return nullptr;
}

bool ModelManager::startDownload (const CatalogModel& model, juce::String& error)
{
    if (isDownloading())
    {
        error = "Another model is already downloading.";
        return false;
    }

    if (! userDir.createDirectory())
    {
        error = "Could not create the model folder: " + userDir.getFullPathName();
        return false;
    }

    finalFile = userDir.getChildFile (juce::String (model.fileName()));
    partialFile = finalFile.withFileExtension (".bin.partial");
    partialFile.deleteFile();
    expectedSha1 = std::string (model.sha1);
    currentId = juce::String (std::string (model.id));

    juce::URL url (juce::String (model.downloadUrl()));
    task = url.downloadToFile (partialFile, juce::URL::DownloadTaskOptions().withListener (this));

    if (task == nullptr)
    {
        currentId.clear();
        error = "The download could not be started. Check your internet connection.";
        return false;
    }

    return true;
}

void ModelManager::cancelDownload()
{
    if (task != nullptr)
    {
        task.reset();
        partialFile.deleteFile();
        completeDownload (false, "Download cancelled.");
    }
}

void ModelManager::progress (juce::URL::DownloadTask*, juce::int64 downloaded, juce::int64 total)
{
    // Called on a background thread: forward to the message thread.
    juce::MessageManager::callAsync ([this, token = alive, downloaded, total]
    {
        if (! token->load()) return;
        const float p = total > 0 ? static_cast<float> (static_cast<double> (downloaded) / static_cast<double> (total)) : -1.0f;
        listeners.call ([&] (Listener& l) { l.downloadProgress (currentId, p, downloaded, total); });
    });
}

void ModelManager::finished (juce::URL::DownloadTask* t, bool success)
{
    const int status = t->statusCode();
    const bool ok = success && ! t->hadError() && (status == 0 || status == 200);

    juce::MessageManager::callAsync ([this, token = alive, ok, status]
    {
        if (! token->load() || task == nullptr) return;
        task.reset();

        if (! ok)
        {
            partialFile.deleteFile();
            completeDownload (false, "The download failed" + (status > 0 ? " (HTTP " + juce::String (status) + ")" : juce::String()) + ".");
            return;
        }

        // Verify off the message thread: hashing hundreds of MB takes a moment.
        verifying = true;
        listeners.call ([&] (Listener& l) { l.downloadProgress (currentId, 1.0f, 0, 0); });

        std::thread ([this, token, partial = partialFile, final = finalFile, expected = expectedSha1]
        {
            const auto digest = Sha1::ofFile (std::filesystem::path (partial.getFullPathName().toWideCharPointer()),
                                              [&] { return token->load(); });
            const bool valid = digest.has_value() && *digest == expected;
            const bool moved = valid && partial.moveFileTo (final);
            if (! moved) partial.deleteFile();

            juce::MessageManager::callAsync ([this, token, valid, moved]
            {
                if (! token->load()) return;
                verifying = false;
                if (! valid)       completeDownload (false, "The downloaded file did not match the published checksum and was discarded.");
                else if (! moved)  completeDownload (false, "The model was downloaded but could not be moved into the model folder.");
                else               completeDownload (true, "Model installed and verified.");
            });
        }).detach();
    });
}

void ModelManager::completeDownload (bool success, const juce::String& message)
{
    const auto id = currentId;
    currentId.clear();
    rescan();
    listeners.call ([&] (Listener& l) { l.downloadFinished (id, success, message); });
}

bool ModelManager::removeModel (const juce::File& file, juce::String& error)
{
    if (! file.isAChildOf (userDir))
    {
        error = "Only models in Stilleto's model folder can be removed from here.";
        return false;
    }

    if (! file.deleteFile())
    {
        error = "The model file could not be deleted (it may be in use).";
        return false;
    }

    rescan();
    return true;
}

} // namespace stilleto
