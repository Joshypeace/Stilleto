#include "transcription/WhisperEngine.h"

#include "transcription/ModelCatalog.h"

#include <whisper.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>

namespace stilleto
{
namespace
{
std::mutex logMutex;
std::function<void (const std::string&)> logHandler;

void whisperLogCallback (ggml_log_level level, const char* text, void*)
{
    if (text == nullptr || level < GGML_LOG_LEVEL_WARN)
        return;

    std::lock_guard lock (logMutex);
    if (logHandler)
        logHandler (text);
}

struct LogInstaller
{
    LogInstaller() { whisper_log_set (whisperLogCallback, nullptr); }
};

void ensureLogInstalled()
{
    static LogInstaller installer;
}
} // namespace

void WhisperEngine::ContextDeleter::operator() (whisper_context* ctx) const noexcept
{
    if (ctx != nullptr)
        whisper_free (ctx);
}

WhisperEngine::WhisperEngine() { ensureLogInstalled(); }
WhisperEngine::~WhisperEngine() = default;

void WhisperEngine::setLogHandler (std::function<void (const std::string&)> handler)
{
    ensureLogInstalled();
    std::lock_guard lock (logMutex);
    logHandler = std::move (handler);
}

int WhisperEngine::defaultThreadCount() noexcept
{
    // Leave a core for the UI and the target application; whisper scales poorly beyond ~8 threads.
    const auto hw = static_cast<int> (std::thread::hardware_concurrency());
    return std::clamp (hw > 1 ? hw - 1 : 1, 1, 8);
}

std::string WhisperEngine::systemInfo()
{
    const char* s = whisper_print_system_info();
    return s != nullptr ? s : "";
}

WhisperEngine::LoadResult WhisperEngine::load (const std::filesystem::path& modelFile, const LoadOptions& options)
{
    LoadResult result;

    if (const auto status = inspectModelFile (modelFile); status != ModelFileStatus::Valid)
    {
        result.error = std::string (describe (status)) + ": " + modelFile.filename().string();
        return result;
    }

    auto params = whisper_context_default_params();
    params.use_gpu = options.useGpu;
    params.flash_attn = options.flashAttention;

    // whisper_init_from_file takes a narrow path; std::filesystem gives us UTF-8 on request.
    const auto utf8Path = modelFile.u8string();
    const std::string narrowPath (utf8Path.begin(), utf8Path.end());

    std::unique_ptr<whisper_context, ContextDeleter> newContext (whisper_init_from_file_with_params (narrowPath.c_str(), params));

    if (newContext == nullptr)
    {
        result.error = "whisper.cpp could not initialise the model (it may be corrupt or an unsupported format)";
        return result;
    }

    context = std::move (newContext);
    info.path = modelFile;
    info.typeName = whisper_model_type_readable (context.get());
    info.multilingual = whisper_is_multilingual (context.get()) != 0;

    result.ok = true;
    result.info = info;
    return result;
}

void WhisperEngine::unload()
{
    context.reset();
    info = {};
}

WhisperEngine::TranscribeResult WhisperEngine::transcribe (std::span<const float> pcm,
                                                           const TranscribeOptions& options,
                                                           const std::atomic<bool>* cancelFlag)
{
    TranscribeResult result;

    if (context == nullptr)
    {
        result.error = "No speech model is loaded";
        return result;
    }

    if (pcm.empty())
    {
        result.error = "The recording is empty";
        return result;
    }

    auto params = whisper_full_default_params (WHISPER_SAMPLING_GREEDY);
    params.n_threads = options.threads > 0 ? options.threads : defaultThreadCount();
    params.print_progress = false;
    params.print_realtime = false;
    params.print_special = false;
    params.print_timestamps = false;
    params.no_timestamps = true;
    params.translate = options.translateToEnglish;
    params.suppress_blank = true;
    params.suppress_nst = true;
    params.no_context = true;
    params.single_segment = false;

    std::string language = options.language.empty() ? "en" : options.language;
    if (! info.multilingual)
        language = "en";
    else if (language != "auto" && whisper_lang_id (language.c_str()) < 0)
        language = "auto";

    params.language = language.c_str();
    params.detect_language = false;

    if (! options.initialPrompt.empty())
        params.initial_prompt = options.initialPrompt.c_str();

    // Cancellation: checked before each encoder pass and inside ggml graph evaluation.
    if (cancelFlag != nullptr)
    {
        params.encoder_begin_callback = [] (whisper_context*, whisper_state*, void* user) -> bool
        {
            return ! static_cast<const std::atomic<bool>*> (user)->load (std::memory_order_relaxed);
        };
        params.encoder_begin_callback_user_data = const_cast<std::atomic<bool>*> (cancelFlag);

        params.abort_callback = [] (void* user) -> bool
        {
            return static_cast<const std::atomic<bool>*> (user)->load (std::memory_order_relaxed);
        };
        params.abort_callback_user_data = const_cast<std::atomic<bool>*> (cancelFlag);
    }

    const auto start = std::chrono::steady_clock::now();
    const int rc = whisper_full (context.get(), params, pcm.data(), static_cast<int> (pcm.size()));
    result.inferenceSeconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();

    if (cancelFlag != nullptr && cancelFlag->load())
    {
        result.status = Status::Cancelled;
        return result;
    }

    if (rc != 0)
    {
        result.error = "Speech recognition failed (whisper.cpp error " + std::to_string (rc) + ")";
        return result;
    }

    const int segments = whisper_full_n_segments (context.get());
    for (int i = 0; i < segments; ++i)
    {
        if (const char* segmentText = whisper_full_get_segment_text (context.get(), i))
        {
            if (! result.text.empty()) result.text += '\n';
            result.text += segmentText;
        }
    }

    const int langId = whisper_full_lang_id (context.get());
    if (const char* code = langId >= 0 ? whisper_lang_str (langId) : nullptr)
        result.language = code;
    else
        result.language = language;

    result.status = Status::Ok;
    return result;
}

} // namespace stilleto
