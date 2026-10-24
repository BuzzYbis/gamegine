// EXPECT-ERROR: reject_argument
// The e_COUNT sentinel is not a message.
#include "probe.h"


using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(Probe::e_NONE);
#else
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(Probe::e_COUNT);
#endif
