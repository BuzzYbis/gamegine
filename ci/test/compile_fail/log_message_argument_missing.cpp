// EXPECT-ERROR: reject_argument
// A message that declares an argument is given none.
#include "probe.h"


using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(Probe::e_UNSIGNED, 5U);
#else
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(Probe::e_UNSIGNED);
#endif
