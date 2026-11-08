#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

namespace stilleto
{

/** Microphone capture.

    The device is opened only while recording or monitoring (so Windows' microphone-in-use
    indicator is not permanently lit) at the device's native sample rate. The audio callback is
    real-time safe: it downmixes to mono, updates lock-free level meters and writes into a
    preallocated lock-free FIFO; a background thread drains that FIFO into the growing recording
    buffer. Public functions are called on the message thread. */
class AudioRecorder : private juce::AudioIODeviceCallback,
                      private juce::Thread
{
public:
    struct Recording
    {
        std::vector<float> samples;   // mono, at sampleRate
        double sampleRate = 0.0;
        float peak = 0.0f;
        float rms = 0.0f;
        bool overflowed = false;      // the drain thread fell behind (should never happen)

        [[nodiscard]] double seconds() const noexcept { return sampleRate > 0 ? samples.size() / sampleRate : 0.0; }
    };

    AudioRecorder();
    ~AudioRecorder() override;

    /** Rescans and returns available microphones. */
    juce::StringArray inputDeviceNames();
    juce::String defaultInputDeviceName();

    /** Empty = system default. Takes effect the next time the device opens. */
    void setPreferredDevice (const juce::String& name);
    [[nodiscard]] const juce::String& preferredDevice() const noexcept { return preferred; }

    bool beginRecording (juce::String& error);
    Recording endRecording();
    void cancelRecording();

    /** Opens the device just for level metering (Settings > Audio). */
    bool startMonitoring (juce::String& error);
    void stopMonitoring();

    [[nodiscard]] bool isRecording() const noexcept { return recording.load(); }
    [[nodiscard]] bool isDeviceOpen() const noexcept { return deviceOpen; }
    [[nodiscard]] double elapsedSeconds() const noexcept;
    [[nodiscard]] juce::String currentDeviceName() const;
    [[nodiscard]] double currentSampleRate() const noexcept { return sampleRate.load(); }

    /** Smoothed input level, 0..1 (linear RMS scaled for display). */
    [[nodiscard]] float currentLevel() const noexcept { return level.load (std::memory_order_relaxed); }

    /** Copies the most recent `count` envelope values (one per ~12 ms, oldest first). */
    void copyEnvelope (float* destination, int count) const noexcept;

    /** Releases the audio device (called at shutdown). */
    void shutdown();

    static constexpr int envelopeSize = 256;

private:
    bool openDevice (juce::String& error);
    void closeDeviceIfUnused();
    void drainFifo();

    // AudioIODeviceCallback (audio thread)
    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs, float* const* outputs,
                                           int numOutputs, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String& message) override;

    // Thread (drain)
    void run() override;

    juce::AudioDeviceManager deviceManager;
    juce::String preferred;
    bool deviceOpen = false;
    bool monitoring = false;

    // Shared with the audio thread.
    std::atomic<bool> recording { false };
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<float> level { 0.0f };
    std::atomic<bool> fifoOverflow { false };
    std::atomic<juce::int64> capturedSamples { 0 };

    std::array<std::atomic<float>, envelopeSize> envelope {};
    std::atomic<int> envelopeWrite { 0 };
    double envelopeAccum = 0.0;   // audio thread only
    int envelopeCount = 0;        // audio thread only
    int envelopeChunk = 512;      // audio thread only

    std::vector<float> mono;      // audio thread scratch, sized in audioDeviceAboutToStart

    static constexpr int fifoCapacity = 1 << 19;   // ~10.9 s at 48 kHz of drain headroom
    juce::AbstractFifo fifo { fifoCapacity };
    std::vector<float> fifoBuffer = std::vector<float> (fifoCapacity);

    // Drain thread / message thread.
    std::mutex captureMutex;
    std::vector<float> captured;
    juce::String deviceError;
};

} // namespace stilleto
