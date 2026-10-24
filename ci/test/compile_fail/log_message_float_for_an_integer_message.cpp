// EXPECT-ERROR: reject_argument
// A float is given to a message that declares an unsigned number.
#include "probe.h"

using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(Probe::e_FLOAT, 1.5F);
#else
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(Probe::e_UNSIGNED, 1.5F);
#endif
