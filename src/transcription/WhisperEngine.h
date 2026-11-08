#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>

struct whisper_context;

namespace stilleto
{

/** Synchronous whisper.cpp wrapper.

    Not thread-safe by design: WhisperTranscriber confines every call (load, transcribe, unload) to
    its single worker thread, so the context can never be freed while inference is using it and no
    lock is held during inference. Tests use it directly. */
class WhisperEngine
{
public:
    struct LoadOptions
    {
        bool useGpu = false;
        bool flashAttention = false;
    };

    struct ModelInfo
    {
        std::filesystem::path path;
        std::string typeName;     // "base", "small", ...
        bool multilingual = false;
    };

    struct LoadResult
    {
        bool ok = false;
        std::string error;
        ModelInfo info;
    };

    struct TranscribeOptions
    {
        std::string language = "en";   // ISO code, or "auto"
        int threads = 0;               // 0 = automatic
        bool translateToEnglish = false;
        std::string initialPrompt;     // optional vocabulary hint
    };

    enum class Status
    {
        Ok,
        Cancelled,
        Failed
    };

    struct TranscribeResult
    {
        Status status = Status::Failed;
        std::string text;             // raw segments joined with '\n'
        std::string language;         // detected or forced language code
        std::string error;
        double inferenceSeconds = 0.0;
    };

    WhisperEngine();
    ~WhisperEngine();

    WhisperEngine (const WhisperEngine&) = delete;
    WhisperEngine& operator= (const WhisperEngine&) = delete;

    /** Loads a model, replacing any existing one only if the new one loads successfully. */
    LoadResult load (const std::filesystem::path& modelFile, const LoadOptions& options);
    void unload();

    [[nodiscard]] bool isLoaded() const noexcept { return context != nullptr; }
    [[nodiscard]] const ModelInfo& modelInfo() const noexcept { return info; }

    /** pcm must be 16 kHz mono float in [-1, 1]. cancelFlag, if given, is polled during inference. */
    TranscribeResult transcribe (std::span<const float> pcm16k,
                                 const TranscribeOptions& options,
                                 const std::atomic<bool>* cancelFlag = nullptr);

    [[nodiscard]] static int defaultThreadCount() noexcept;
    [[nodiscard]] static std::string systemInfo();

    /** Routes whisper/ggml log output (errors only by default). */
    static void setLogHandler (std::function<void (const std::string&)> handler);

private:
    struct ContextDeleter { void operator() (whisper_context*) const noexcept; };

    std::unique_ptr<whisper_context, ContextDeleter> context;
    ModelInfo info;
};

} // namespace stilleto
