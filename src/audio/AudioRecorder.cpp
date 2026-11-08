#include "audio/AudioRecorder.h"

#include <cmath>

namespace stilleto
{

AudioRecorder::AudioRecorder() : juce::Thread ("Stilleto audio drain")
{
    // Create the platform device types without opening anything. WASAPI shared mode ("Windows
    // Audio") runs at the endpoint's native mix rate and coexists with other applications.
    const auto& types = deviceManager.getAvailableDeviceTypes();
    for (auto* type : types)
        if (type->getTypeName() == "Windows Audio")
            deviceManager.setCurrentAudioDeviceType (type->getTypeName(), false);

    startThread (juce::Thread::Priority::normal);
}

AudioRecorder::~AudioRecorder()
{
    shutdown();
    signalThreadShouldExit();
    notify();
    stopThread (2000);
}

void AudioRecorder::shutdown()
{
    recording = false;
    monitoring = false;

    if (deviceOpen)
    {
        deviceManager.removeAudioCallback (this);
        deviceManager.closeAudioDevice();
        deviceOpen = false;
    }
}

juce::StringArray AudioRecorder::inputDeviceNames()
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        return type->getDeviceNames (true);
    }
    return {};
}

juce::String AudioRecorder::defaultInputDeviceName()
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        const auto names = type->getDeviceNames (true);
        const int index = type->getDefaultDeviceIndex (true);
        if (juce::isPositiveAndBelow (index, names.size()))
            return names[index];
    }
    return {};
}

void AudioRecorder::setPreferredDevice (const juce::String& name)
{
    preferred = name;
}

juce::String AudioRecorder::currentDeviceName() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getName();
    return preferred;
}

bool AudioRecorder::openDevice (juce::String& error)
{
    if (deviceOpen)
        return true;

    const auto names = inputDeviceNames();
    if (names.isEmpty())
    {
        error = "No microphone was found. Connect a microphone and check that it is enabled in Windows Sound settings.";
        return false;
    }

    auto deviceName = preferred;
    if (deviceName.isEmpty() || ! names.contains (deviceName))
        deviceName = defaultInputDeviceName();
    if (deviceName.isEmpty())
        deviceName = names[0];

    // Request explicit channels. With useDefaultInputChannels, JUCE takes the count from
    // initialise(), which is deliberately never called (it would open the microphone at startup);
    // the count would be zero and setAudioDeviceSetup() would silently not open the device.
    int channelCount = 1;
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        if (std::unique_ptr<juce::AudioIODevice> probe { type->createDevice ({}, deviceName) })   // does not start a stream
            channelCount = juce::jlimit (1, 2, probe->getInputChannelNames().size());

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.inputDeviceName = deviceName;
    setup.outputDeviceName = {};
    setup.useDefaultInputChannels = false;
    setup.inputChannels.setRange (0, channelCount, true);
    setup.useDefaultOutputChannels = false;
    setup.sampleRate = 0;   // native rate
    setup.bufferSize = 0;

    deviceError.clear();
    const auto result = deviceManager.setAudioDeviceSetup (setup, true);

    if (result.isNotEmpty() || deviceManager.getCurrentAudioDevice() == nullptr)
    {
        error = "The microphone \"" + deviceName + "\" could not be opened"
              + (result.isNotEmpty() ? " (" + result + ")" : juce::String())
              + ". It may be in use exclusively by another application, or microphone access may be "
                "turned off in Windows Settings > Privacy & security > Microphone.";
        deviceManager.closeAudioDevice();
        return false;
    }

    deviceManager.addAudioCallback (this);
    deviceOpen = true;

    if (auto* device = deviceManager.getCurrentAudioDevice())
        juce::Logger::writeToLog ("[Stilleto] Audio device '" + device->getName() + "' type " + device->getTypeName()
                                  + " open=" + juce::String ((int) device->isOpen()) + " playing=" + juce::String ((int) device->isPlaying())
                                  + " rate=" + juce::String (device->getCurrentSampleRate()) + " block=" + juce::String (device->getCurrentBufferSizeSamples())
                                  + " inputs=" + device->getActiveInputChannels().toString (2)
                                  + (device->getLastError().isNotEmpty() ? " error=" + device->getLastError() : juce::String()));
    return true;
}

void AudioRecorder::closeDeviceIfUnused()
{
    if (deviceOpen && ! recording && ! monitoring)
    {
        deviceManager.removeAudioCallback (this);
        deviceManager.closeAudioDevice();
        deviceOpen = false;
        level = 0.0f;
    }
}

bool AudioRecorder::beginRecording (juce::String& error)
{
    if (recording)
        return true;

    if (! openDevice (error))
        return false;

    {
        // Discard leftovers by draining (resetting the FIFO could race with an in-flight callback).
        std::lock_guard lock (captureMutex);
        drainFifo();
        captured.clear();
        const double rate = sampleRate.load() > 0 ? sampleRate.load() : 48000.0;
        captured.reserve (static_cast<size_t> (rate * 60.0));   // one minute up front; grows off the audio thread
    }

    fifoOverflow = false;
    capturedSamples = 0;
    recording = true;
    notify();
    return true;
}

AudioRecorder::Recording AudioRecorder::endRecording()
{
    Recording result;
    recording = false;

    {
        std::lock_guard lock (captureMutex);
        drainFifo();
        result.samples = std::move (captured);
        captured = {};
    }

    result.sampleRate = sampleRate.load();
    result.overflowed = fifoOverflow.load();

    double sumSquares = 0.0;
    for (const float s : result.samples)
    {
        result.peak = juce::jmax (result.peak, std::abs (s));
        sumSquares += static_cast<double> (s) * s;
    }
    if (! result.samples.empty())
        result.rms = static_cast<float> (std::sqrt (sumSquares / static_cast<double> (result.samples.size())));

    closeDeviceIfUnused();
    return result;
}

void AudioRecorder::cancelRecording()
{
    recording = false;
    {
        std::lock_guard lock (captureMutex);
        drainFifo();
        captured.clear();
        captured.shrink_to_fit();
    }
    closeDeviceIfUnused();
}

bool AudioRecorder::startMonitoring (juce::String& error)
{
    if (! openDevice (error))
        return false;
    monitoring = true;
    return true;
}

void AudioRecorder::stopMonitoring()
{
    monitoring = false;
    closeDeviceIfUnused();
}

double AudioRecorder::elapsedSeconds() const noexcept
{
    const double rate = sampleRate.load();
    return rate > 0 ? static_cast<double> (capturedSamples.load()) / rate : 0.0;
}

void AudioRecorder::copyEnvelope (float* destination, int count) const noexcept
{
    count = juce::jmin (count, envelopeSize);
    const int write = envelopeWrite.load (std::memory_order_acquire);
    for (int i = 0; i < count; ++i)
    {
        const int index = (write - count + i + envelopeSize * 2) % envelopeSize;
        destination[i] = envelope[static_cast<size_t> (index)].load (std::memory_order_relaxed);
    }
}

//==============================================================================================
void AudioRecorder::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const double rate = device->getCurrentSampleRate();
    sampleRate = rate;
    envelopeChunk = juce::jmax (64, static_cast<int> (rate * 0.012));
    envelopeAccum = 0.0;
    envelopeCount = 0;
    mono.assign (static_cast<size_t> (juce::jmax (8192, device->getCurrentBufferSizeSamples() * 4)), 0.0f);

    for (auto& e : envelope) e.store (0.0f);
}

void AudioRecorder::audioDeviceStopped()
{
    level = 0.0f;
}

void AudioRecorder::audioDeviceError (const juce::String& message)
{
    juce::ignoreUnused (message);   // surfaced through silence/empty-recording handling
}

void AudioRecorder::audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs, float* const* outputs,
                                                      int numOutputs, int numSamples,
                                                      const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputs; ++ch)
        if (outputs[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputs[ch], numSamples);

    int offset = 0;
    float blockPeakSquares = 0.0f;

    while (offset < numSamples)
    {
        const int n = juce::jmin (numSamples - offset, static_cast<int> (mono.size()));
        float* m = mono.data();

        // Downmix the active input channels to mono.
        int active = 0;
        juce::FloatVectorOperations::clear (m, n);
        for (int ch = 0; ch < numInputs; ++ch)
        {
            if (inputs[ch] != nullptr)
            {
                juce::FloatVectorOperations::add (m, inputs[ch] + offset, n);
                ++active;
            }
        }
        if (active > 1)
            juce::FloatVectorOperations::multiply (m, 1.0f / static_cast<float> (active), n);

        // Envelope for the waveform (~12 ms per value) and level meter.
        for (int i = 0; i < n; ++i)
        {
            const float s = m[i];
            envelopeAccum += static_cast<double> (s) * s;
            blockPeakSquares = juce::jmax (blockPeakSquares, s * s);

            if (++envelopeCount >= envelopeChunk)
            {
                const float rms = static_cast<float> (std::sqrt (envelopeAccum / envelopeCount));
                const int w = envelopeWrite.load (std::memory_order_relaxed);
                envelope[static_cast<size_t> (w)].store (rms, std::memory_order_relaxed);
                envelopeWrite.store ((w + 1) % envelopeSize, std::memory_order_release);
                envelopeAccum = 0.0;
                envelopeCount = 0;
            }
        }

        if (recording.load (std::memory_order_acquire))
        {
            const auto scope = fifo.write (n);
            if (scope.blockSize1 + scope.blockSize2 < n)
                fifoOverflow.store (true, std::memory_order_relaxed);
            if (scope.blockSize1 > 0) std::copy (m, m + scope.blockSize1, fifoBuffer.data() + scope.startIndex1);
            if (scope.blockSize2 > 0) std::copy (m + scope.blockSize1, m + scope.blockSize1 + scope.blockSize2, fifoBuffer.data() + scope.startIndex2);
            capturedSamples.fetch_add (scope.blockSize1 + scope.blockSize2, std::memory_order_relaxed);
        }

        offset += n;
    }

    // Peak-ish meter with fast attack and slow release, scaled so speech sits mid-range.
    const float peak = std::sqrt (blockPeakSquares);
    const float display = juce::jlimit (0.0f, 1.0f, peak * 2.2f);
    const float previous = level.load (std::memory_order_relaxed);
    level.store (display > previous ? display : previous * 0.92f + display * 0.08f, std::memory_order_relaxed);
}

//==============================================================================================
void AudioRecorder::drainFifo()   // captureMutex held
{
    const int ready = fifo.getNumReady();
    if (ready <= 0) return;

    const auto scope = fifo.read (ready);
    captured.insert (captured.end(), fifoBuffer.data() + scope.startIndex1, fifoBuffer.data() + scope.startIndex1 + scope.blockSize1);
    if (scope.blockSize2 > 0)
        captured.insert (captured.end(), fifoBuffer.data() + scope.startIndex2, fifoBuffer.data() + scope.startIndex2 + scope.blockSize2);
}

void AudioRecorder::run()
{
    while (! threadShouldExit())
    {
        wait (recording.load() ? 15 : 250);

        if (fifo.getNumReady() > 0)
        {
            std::lock_guard lock (captureMutex);
            drainFifo();
        }
    }
}

} // namespace stilleto
