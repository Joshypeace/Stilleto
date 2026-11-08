#include "transcription/WhisperTranscriber.h"

#include "audio/AudioResampler.h"
#include "core/SpeechDetection.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace stilleto
{

WhisperTranscriber::WhisperTranscriber() : juce::Thread ("Stilleto transcription")
{
    startThread (juce::Thread::Priority::normal);
}

WhisperTranscriber::~WhisperTranscriber()
{
    shutdown();
}

void WhisperTranscriber::shutdown()
{
    alive->store (false);   // callbacks already queued on the message thread become no-ops
    cancelRequested = true;
    {
        std::lock_guard lock (queueMutex);
        pendingLoad.reset();
        pendingJob.reset();
    }
    signalThreadShouldExit();
    wake.signal();
    stopThread (10000);     // inference polls the cancel flag, so this returns promptly
}

void WhisperTranscriber::post (std::function<void()> fn)
{
    juce::MessageManager::callAsync ([token = alive, fn = std::move (fn)]
    {
        if (token->load())
            fn();
    });
}

void WhisperTranscriber::loadModel (const juce::File& file, WhisperEngine::LoadOptions options, LoadCallback onDone)
{
    {
        std::lock_guard lock (queueMutex);
        pendingLoad = LoadRequest { file, options, std::move (onDone) };
    }
    state = ModelState::Loading;
    wake.signal();
}

bool WhisperTranscriber::transcribe (Job job, ResultCallback onDone)
{
    {
        std::lock_guard lock (queueMutex);
        if (busy.load() || pendingJob.has_value())
            return false;
        pendingJob = JobRequest { std::move (job), std::move (onDone) };
        cancelRequested = false;
        busy = true;
    }
    wake.signal();
    return true;
}

void WhisperTranscriber::cancelTranscription()
{
    cancelRequested = true;
}

juce::String WhisperTranscriber::modelError() const
{
    std::lock_guard lock (infoMutex);
    return lastModelError;
}

WhisperEngine::ModelInfo WhisperTranscriber::modelInfo() const
{
    std::lock_guard lock (infoMutex);
    return info;
}

void WhisperTranscriber::run()
{
    while (! threadShouldExit())
    {
        wake.wait (-1);

        for (;;)
        {
            if (threadShouldExit())
                break;

            std::optional<LoadRequest> load;
            std::optional<JobRequest> job;
            {
                std::lock_guard lock (queueMutex);
                // Loads first: a job queued after a model change should use the new model.
                if (pendingLoad.has_value()) load.swap (pendingLoad);
                else if (pendingJob.has_value()) job.swap (pendingJob);
            }

            if (load.has_value())
            {
                // Wide path: std::filesystem would otherwise interpret narrow strings in the ANSI code page.
                const std::filesystem::path path (load->file.getFullPathName().toWideCharPointer());
                const auto result = engine.load (path, load->options);
                {
                    std::lock_guard lock (infoMutex);
                    if (result.ok) { info = result.info; lastModelError.clear(); }
                    else lastModelError = result.error;
                }

                // A failed load keeps any previous model (WhisperEngine only replaces on success);
                // the error is still reported through the callback and modelError().
                state = engine.isLoaded() ? ModelState::Ready : ModelState::Failed;

                if (load->callback)
                    post ([cb = std::move (load->callback), result] { cb (result); });
                continue;
            }

            if (job.has_value())
            {
                auto result = process (job->job);
                job->job.samples = {};   // release audio memory before notifying
                busy = false;

                if (job->callback)
                    post ([cb = std::move (job->callback), result = std::move (result)] { cb (result); });
                continue;
            }

            break;
        }
    }

    engine.unload();
}

WhisperTranscriber::Result WhisperTranscriber::process (Job& job)
{
    Result result;
    result.audioSeconds = job.sampleRate > 0 ? job.samples.size() / job.sampleRate : 0.0;

    if (job.saveWavTo)
    {
        // Opt-in only ("Save raw recordings"); written at the device rate before any processing.
        job.wavFile.getParentDirectory().createDirectory();
        juce::WavAudioFormat wav;
        if (auto stream = job.wavFile.createOutputStream())
        {
            std::unique_ptr<juce::OutputStream> out (stream.release());
            if (auto writer = wav.createWriterFor (out, juce::AudioFormatWriterOptions()
                                                            .withSampleRate (job.sampleRate)
                                                            .withNumChannels (1)
                                                            .withBitsPerSample (16)))
            {
                const float* channels[] = { job.samples.data() };
                writer->writeFromFloatArrays (channels, 1, static_cast<int> (job.samples.size()));
            }
        }
    }

    if (cancelRequested)
    {
        result.status = Result::Status::Cancelled;
        return result;
    }

    if (engine.isLoaded() == false)
    {
        result.error = "No speech model is loaded. Choose or download a model on the Models page.";
        return result;
    }

    // whisper.cpp expects 16 kHz mono; never relabel audio at another rate.
    auto pcm = AudioResampler::resample (job.samples, job.sampleRate, AudioResampler::whisperSampleRate);

    const auto analysis = analyseSpeech (pcm, AudioResampler::whisperSampleRate);
    if (analysis.isDigitalSilence())
    {
        result.status = Result::Status::NoSpeech;
        result.error = "The microphone delivered pure silence. It may be muted, or microphone access may be blocked in Windows privacy settings.";
        return result;
    }
    if (! analysis.hasSpeech())
    {
        result.status = Result::Status::NoSpeech;
        result.error = "No speech was detected in the recording.";
        return result;
    }

    // whisper processes 30 s windows; pad very short clips with a little silence so the
    // final words are not clipped by the decoder.
    if (pcm.size() < static_cast<size_t> (AudioResampler::whisperSampleRate * 1.2))
        pcm.resize (static_cast<size_t> (AudioResampler::whisperSampleRate * 1.2), 0.0f);

    const auto out = engine.transcribe (pcm, job.options, &cancelRequested);
    result.inferenceSeconds = out.inferenceSeconds;
    result.language = out.language;

    switch (out.status)
    {
        case WhisperEngine::Status::Cancelled:
            result.status = Result::Status::Cancelled;
            break;

        case WhisperEngine::Status::Failed:
            result.status = Result::Status::Failed;
            result.error = out.error;
            break;

        case WhisperEngine::Status::Ok:
            result.rawText = out.text;
            // Whisper's stock phrases on near-silence: only trusted when the speech was substantial.
            if (isCommonHallucination (out.text) && analysis.voicedSeconds < 0.6)
            {
                result.status = Result::Status::NoSpeech;
                result.error = "No speech was detected in the recording.";
            }
            else
            {
                result.status = Result::Status::Ok;
            }
            break;
    }

    return result;
}

} // namespace stilleto
