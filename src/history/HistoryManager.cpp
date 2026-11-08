#include "history/HistoryManager.h"

#include <algorithm>

namespace stilleto
{

HistoryManager::HistoryManager (juce::File storageFile)
    : juce::Thread ("Stilleto history writer"), storage (std::move (storageFile))
{
    startThread (juce::Thread::Priority::low);
}

HistoryManager::~HistoryManager()
{
    flushNow();
    signalThreadShouldExit();
    wake.signal();
    stopThread (4000);
}

bool HistoryManager::load (juce::String& error)
{
    items.clear();

    if (! storage.existsAsFile())
        return true;

    const auto parsed = juce::JSON::parse (storage.loadFileAsString());
    const auto* list = parsed.getProperty ("entries", {}).getArray();

    if (list == nullptr)
    {
        // Move the unreadable file aside before anything can overwrite it.
        const auto backup = storage.getSiblingFile ("history.unreadable-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".json");
        const bool moved = storage.moveFileTo (backup);
        error = moved ? "The transcription history file could not be read. It was kept as " + backup.getFullPathName()
                      : "The transcription history file could not be read: " + storage.getFullPathName();
        if (! moved)
            writesBlocked = true;   // never overwrite a file we could not preserve (survives toggling the setting)
        loadProblem = error;
        return false;
    }

    for (const auto& v : *list)
    {
        HistoryEntry e;
        e.id = v.getProperty ("id", juce::Uuid().toString()).toString();
        e.text = v.getProperty ("text", {}).toString();
        e.timestamp = juce::Time (static_cast<juce::int64> (v.getProperty ("timestamp", 0)));
        e.durationSeconds = static_cast<double> (v.getProperty ("duration", 0.0));
        e.wordCount = static_cast<int> (v.getProperty ("words", 0));
        e.language = v.getProperty ("language", {}).toString();
        e.model = v.getProperty ("model", {}).toString();
        e.inserted = static_cast<bool> (v.getProperty ("inserted", false));
        e.insertionNote = v.getProperty ("insertion", {}).toString();

        if (e.text.isNotEmpty())
            items.push_back (std::move (e));
    }

    std::sort (items.begin(), items.end(), [] (const auto& a, const auto& b) { return a.timestamp > b.timestamp; });
    return true;
}

void HistoryManager::setPersistenceEnabled (bool enabled)
{
    persistenceEnabled = enabled;
}

bool HistoryManager::isPersistenceEnabled() const noexcept
{
    return persistenceEnabled && ! writesBlocked;
}

void HistoryManager::add (HistoryEntry entry)
{
    // With history off nothing is kept, not even in memory, so the list always mirrors the file.
    if (! isPersistenceEnabled())
        return;

    if (entry.id.isEmpty())
        entry.id = juce::Uuid().toString();

    items.insert (items.begin(), std::move (entry));
    if (static_cast<int> (items.size()) > maxEntries)
        items.resize (maxEntries);

    scheduleWrite();
    notify();
}

bool HistoryManager::remove (const juce::String& id)
{
    const auto it = std::find_if (items.begin(), items.end(), [&] (const auto& e) { return e.id == id; });
    if (it == items.end())
        return false;

    items.erase (it);

    if (writesBlocked)
    {
        notify();
        return true;
    }

    // Deleting must remove the stored copy even if persistence was switched off afterwards.
    {
        std::lock_guard lock (writeMutex);
        if (items.empty()) deletePending = true;
        pendingJson = items.empty() ? std::optional<juce::String> {} : std::optional<juce::String> { serialise() };
        idle.reset();
    }
    wake.signal();
    notify();
    return true;
}

void HistoryManager::clear()
{
    items.clear();
    if (writesBlocked)
    {
        notify();
        return;
    }
    {
        std::lock_guard lock (writeMutex);
        pendingJson.reset();
        deletePending = true;
        idle.reset();
    }
    wake.signal();
    notify();
}

void HistoryManager::updateInsertion (const juce::String& id, bool inserted, const juce::String& note)
{
    for (auto& e : items)
    {
        if (e.id == id)
        {
            e.inserted = inserted;
            e.insertionNote = note;
            scheduleWrite();
            notify();
            return;
        }
    }
}

std::vector<HistoryEntry> HistoryManager::search (const juce::String& query) const
{
    const auto q = query.trim();
    if (q.isEmpty())
        return items;

    std::vector<HistoryEntry> result;
    for (const auto& e : items)
        if (e.text.containsIgnoreCase (q) || e.model.containsIgnoreCase (q) || e.insertionNote.containsIgnoreCase (q))
            result.push_back (e);
    return result;
}

const HistoryEntry* HistoryManager::find (const juce::String& id) const
{
    for (const auto& e : items)
        if (e.id == id)
            return &e;
    return nullptr;
}

int64_t HistoryManager::totalWords() const noexcept
{
    int64_t total = 0;
    for (const auto& e : items) total += e.wordCount;
    return total;
}

double HistoryManager::totalSeconds() const noexcept
{
    double total = 0;
    for (const auto& e : items) total += e.durationSeconds;
    return total;
}

juce::String HistoryManager::lastWriteError() const
{
    std::lock_guard lock (writeMutex);
    return writeError;
}

void HistoryManager::notify()
{
    listeners.call ([] (Listener& l) { l.historyChanged(); });
}

juce::String HistoryManager::serialise() const
{
    juce::Array<juce::var> list;

    for (const auto& e : items)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", e.id);
        obj->setProperty ("text", e.text);
        obj->setProperty ("timestamp", e.timestamp.toMilliseconds());
        obj->setProperty ("duration", e.durationSeconds);
        obj->setProperty ("words", e.wordCount);
        obj->setProperty ("language", e.language);
        obj->setProperty ("model", e.model);
        obj->setProperty ("inserted", e.inserted);
        obj->setProperty ("insertion", e.insertionNote);
        list.add (juce::var (obj));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("entries", list);
    return juce::JSON::toString (juce::var (root));
}

void HistoryManager::scheduleWrite()
{
    if (writesBlocked)
        return;
    {
        std::lock_guard lock (writeMutex);
        pendingJson = serialise();
        deletePending = false;
        idle.reset();
    }
    wake.signal();
}

void HistoryManager::flushNow()
{
    wake.signal();
    idle.wait (5000);
}

bool HistoryManager::writeAtomically (const juce::File& target, const juce::String& json, juce::String& error)
{
    target.getParentDirectory().createDirectory();

    juce::TemporaryFile temp (target);
    if (! temp.getFile().replaceWithText (json, false, false, "\n"))
    {
        error = "Could not write the history file: " + target.getFullPathName();
        return false;
    }

    if (! temp.overwriteTargetFileWithTemporary())
    {
        error = "Could not replace the history file: " + target.getFullPathName();
        return false;
    }

    return true;
}

void HistoryManager::run()
{
    while (! threadShouldExit())
    {
        wake.wait (-1);

        std::optional<juce::String> json;
        bool shouldDelete = false;
        {
            std::lock_guard lock (writeMutex);
            json.swap (pendingJson);
            shouldDelete = deletePending;
            deletePending = false;
        }

        juce::String error;
        if (shouldDelete && ! json.has_value())
        {
            if (storage.existsAsFile() && ! storage.deleteFile())
                error = "Could not delete the history file: " + storage.getFullPathName();
        }
        else if (json.has_value())
        {
            writeAtomically (storage, *json, error);
        }

        std::lock_guard lock (writeMutex);
        writeError = error;
        if (! pendingJson.has_value() && ! deletePending)
            idle.signal();
    }

    idle.signal();
}

} // namespace stilleto
