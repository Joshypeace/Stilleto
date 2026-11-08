#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <mutex>
#include <optional>
#include <vector>

namespace stilleto
{

struct HistoryEntry
{
    juce::String id;               // UUID
    juce::String text;
    juce::Time timestamp;
    double durationSeconds = 0.0;
    int wordCount = 0;
    juce::String language;
    juce::String model;
    bool inserted = false;
    juce::String insertionNote;    // e.g. "Inserted into Notepad", "Not inserted: target closed"
};

/** Local transcription history in %APPDATA%\Stilleto\history.json.

    Lives on the message thread; writes are serialised there and handed to a background writer so
    the UI never blocks on disk. Writes are atomic (temp file + replace), so a crash cannot leave a
    truncated history. Raw audio is never stored here. */
class HistoryManager : private juce::Thread
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void historyChanged() = 0;
    };

    explicit HistoryManager (juce::File storageFile);
    ~HistoryManager() override;

    /** Loads the file once at startup. Returns false (with an error) if it exists but is unreadable. */
    bool load (juce::String& error);

    void setPersistenceEnabled (bool enabled);
    [[nodiscard]] bool isPersistenceEnabled() const noexcept { return persistenceEnabled; }

    void add (HistoryEntry entry);
    bool remove (const juce::String& id);
    void clear();
    void updateInsertion (const juce::String& id, bool inserted, const juce::String& note);

    [[nodiscard]] const std::vector<HistoryEntry>& entries() const noexcept { return items; }   // newest first
    [[nodiscard]] std::vector<HistoryEntry> search (const juce::String& query) const;
    [[nodiscard]] const HistoryEntry* find (const juce::String& id) const;

    [[nodiscard]] int totalDictations() const noexcept { return static_cast<int> (items.size()); }
    [[nodiscard]] int64_t totalWords() const noexcept;
    [[nodiscard]] double totalSeconds() const noexcept;

    [[nodiscard]] juce::String lastWriteError() const;
    [[nodiscard]] const juce::File& file() const noexcept { return storage; }

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    /** Blocks until pending writes reach the disk (used at shutdown and by tests). */
    void flushNow();

    static constexpr int maxEntries = 1000;

private:
    void run() override;
    void scheduleWrite();
    void notify();
    [[nodiscard]] juce::String serialise() const;
    static bool writeAtomically (const juce::File& target, const juce::String& json, juce::String& error);

    juce::File storage;
    std::vector<HistoryEntry> items;
    bool persistenceEnabled = true;
    juce::ListenerList<Listener> listeners;

    mutable std::mutex writeMutex;
    std::optional<juce::String> pendingJson;   // latest snapshot to write; older ones are superseded
    bool deletePending = false;
    juce::String writeError;
    juce::WaitableEvent wake, idle { true };
};

} // namespace stilleto
