#pragma once

#include "transcription/WhisperEngine.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace stilleto
{

/** Asynchronous front end for WhisperEngine.

    One worker thread owns the engine: model loads, inference and unloads all run there, one at a
    time, so the whisper context is never freed while in use and no lock is held during inference.
    Completion callbacks are delivered on the JUCE message thread and are dropped if this object
    has been destroyed. */
class WhisperTranscriber : private juce::Thread
{
public:
    enum class ModelState { NotLoaded, Loading, Ready, Failed };

    struct Job
    {
        std::vector<float> samples;   // mono at sampleRate (resampled to 16 kHz on the worker)
        double sampleRate = 0.0;
        WhisperEngine::TranscribeOptions options;
        bool saveWavTo = false;
        juce::File wavFile;           // written on the worker if saveWavTo (opt-in privacy setting)
    };

    struct Result
    {
        enum class Status { Ok, NoSpeech, Cancelled, Failed };

        Status status = Status::Failed;
        std::string rawText;
        std::string language;
        std::string error;
        double audioSeconds = 0.0;
        double inferenceSeconds = 0.0;
    };

    using LoadCallback = std::function<void (const WhisperEngine::LoadResult&)>;
    using ResultCallback = std::function<void (const Result&)>;

    WhisperTranscriber();
    ~WhisperTranscriber() override;

    /** Queues a model (re)load. A newer request supersedes one that has not started yet. */
    void loadModel (const juce::File& file, WhisperEngine::LoadOptions options, LoadCallback onDone);

    /** Queues a transcription. Returns false if one is already queued or running. */
    bool transcribe (Job job, ResultCallback onDone);

    /** Requests cancellation of the running/queued transcription; its callback reports Cancelled. */
    void cancelTranscription();

    [[nodiscard]] bool isTranscribing() const noexcept { return busy.load(); }
    [[nodiscard]] ModelState modelState() const noexcept { return state.load(); }
    [[nodiscard]] juce::String modelError() const;
    [[nodiscard]] WhisperEngine::ModelInfo modelInfo() const;

    /** Stops the worker (cancelling any inference) and frees the model. Called at shutdown. */
    void shutdown();

private:
    void run() override;
    Result process (Job& job);
    void post (std::function<void()> fn);

    struct LoadRequest
    {
        juce::File file;
        WhisperEngine::LoadOptions options;
        LoadCallback callback;
    };

    struct JobRequest
    {
        Job job;
        ResultCallback callback;
    };

    WhisperEngine engine;   // worker thread only

    std::mutex queueMutex;
    std::optional<LoadRequest> pendingLoad;
    std::optional<JobRequest> pendingJob;
    juce::WaitableEvent wake;

    std::atomic<bool> busy { false };
    std::atomic<bool> cancelRequested { false };
    std::atomic<ModelState> state { ModelState::NotLoaded };

    mutable std::mutex infoMutex;
    WhisperEngine::ModelInfo info;
    juce::String lastModelError;

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

} // namespace stilleto
