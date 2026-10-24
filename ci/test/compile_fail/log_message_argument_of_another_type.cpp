// EXPECT-ERROR: reject_argument
// A signed number is given to a message that declares an unsigned one.
#include "probe.h"


using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(Probe::e_SIGNED, -1);
#else
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(Probe::e_UNSIGNED, -1);
#endif
