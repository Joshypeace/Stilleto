#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <cmath>
#include <functional>

namespace stilleto::ui
{

/** Global motion preference: the user's reduced-motion setting combined with Windows'
    "Show animations" accessibility option. Read on the message thread when animations start. */
struct Motion
{
    static std::atomic<bool>& reducedFlag() { static std::atomic<bool> flag { false }; return flag; }
    static bool reduced() { return reducedFlag().load(); }
    static void setReduced (bool r) { reducedFlag() = r; }
};

namespace Ease
{
    inline float outCubic (float t)   { t = juce::jlimit (0.0f, 1.0f, t); return 1.0f - std::pow (1.0f - t, 3.0f); }
    inline float inOutCubic (float t) { t = juce::jlimit (0.0f, 1.0f, t); return t < 0.5f ? 4 * t * t * t : 1 - std::pow (-2 * t + 2, 3.0f) / 2; }
    inline float outQuint (float t)   { t = juce::jlimit (0.0f, 1.0f, t); return 1.0f - std::pow (1.0f - t, 5.0f); }
}

/** A value that eases exponentially towards a target (frame-rate independent). */
class AnimatedValue
{
public:
    explicit AnimatedValue (float initial = 0.0f, float timeConstantSeconds = 0.09f)
        : current (initial), target (initial), tau (timeConstantSeconds) {}

    void setTarget (float t) noexcept
    {
        target = t;
        if (Motion::reduced()) current = target;
    }

    void snapTo (float v) noexcept { current = target = v; }
    void setTimeConstant (float seconds) noexcept { tau = seconds; }

    /** Advances by dt seconds; returns true while still moving. */
    bool advance (float dt) noexcept
    {
        if (isSettled()) { current = target; return false; }
        const float k = 1.0f - std::exp (-dt / juce::jmax (0.001f, tau));
        current += (target - current) * k;
        return ! isSettled();
    }

    [[nodiscard]] bool isSettled() const noexcept { return std::abs (target - current) < 0.0015f; }
    [[nodiscard]] float get() const noexcept { return current; }
    [[nodiscard]] float getTarget() const noexcept { return target; }

private:
    float current, target, tau;
};

/** Drives per-frame updates only while something is animating, so idle UI costs no CPU.
    onFrame receives the elapsed seconds and returns true to keep running. */
class FrameDriver : private juce::Timer
{
public:
    std::function<bool (float)> onFrame;

    void kick()
    {
        if (! isTimerRunning())
        {
            lastTime = juce::Time::getMillisecondCounterHiRes();
            startTimerHz (60);
        }
    }

    void stop() { stopTimer(); }
    [[nodiscard]] bool isRunning() const { return isTimerRunning(); }

private:
    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float dt = static_cast<float> (juce::jlimit (0.0, 0.1, (now - lastTime) / 1000.0));
        lastTime = now;

        if (! onFrame || ! onFrame (dt))
            stopTimer();
    }

    double lastTime = 0.0;
};

} // namespace stilleto::ui
