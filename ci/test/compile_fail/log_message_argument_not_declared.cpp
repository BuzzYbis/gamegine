// EXPECT-ERROR: reject_argument
// A message that declares no argument is given one.
#include "probe.h"


using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(LogMessage::CoreMessage::e_UNKNOWN);
#else
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(LogMessage::CoreMessage::e_UNKNOWN, 7U);
#endif
