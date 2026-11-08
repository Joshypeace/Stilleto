#include "core/DictationState.h"

namespace stilleto
{

bool isValidTransition (DictationState from, DictationState to) noexcept
{
    using S = DictationState;

    switch (from)
    {
        case S::Idle:       return to == S::Recording || to == S::Inserting || to == S::Error;   // Inserting = re-insert from history
        case S::Recording:  return to == S::Processing || to == S::Idle || to == S::Error;   // Idle = cancelled
        case S::Processing: return to == S::Inserting || to == S::Completed || to == S::Idle || to == S::Error;
        case S::Inserting:  return to == S::Completed || to == S::Error;
        case S::Completed:  return to == S::Idle || to == S::Recording || to == S::Inserting || to == S::Error;
        case S::Error:      return to == S::Idle || to == S::Recording || to == S::Inserting || to == S::Error;
    }

    return false;
}

std::string_view toString (DictationState s) noexcept
{
    switch (s)
    {
        case DictationState::Idle:       return "Idle";
        case DictationState::Recording:  return "Recording";
        case DictationState::Processing: return "Processing";
        case DictationState::Inserting:  return "Inserting";
        case DictationState::Completed:  return "Completed";
        case DictationState::Error:      return "Error";
    }

    return "Unknown";
}

} // namespace stilleto
