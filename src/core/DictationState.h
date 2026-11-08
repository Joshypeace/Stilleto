#pragma once

#include <string_view>

namespace stilleto
{

/** The dictation workflow. Every change goes through isValidTransition() so the controller can
    never, for example, start a second recording while a transcription job is still running. */
enum class DictationState
{
    Idle,
    Recording,
    Processing,
    Inserting,
    Completed,
    Error
};

/** True for the states from which a new recording may begin. */
[[nodiscard]] constexpr bool canStartRecording (DictationState s) noexcept
{
    return s == DictationState::Idle || s == DictationState::Completed || s == DictationState::Error;
}

/** True while background work (inference or insertion) owns the pipeline. */
[[nodiscard]] constexpr bool isBusy (DictationState s) noexcept
{
    return s == DictationState::Processing || s == DictationState::Inserting;
}

[[nodiscard]] bool isValidTransition (DictationState from, DictationState to) noexcept;

[[nodiscard]] std::string_view toString (DictationState s) noexcept;

} // namespace stilleto
