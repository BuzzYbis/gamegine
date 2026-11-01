// EXPECT-ERROR: ambiguous
// A double is given where a float is declared: no overload takes it.
#include "probe.h"


using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(Probe::e_FLOAT, 1.5F);
#else
[[maybe_unused]] constexpr LogMessage k_MESSAGE = LogMessage::make(Probe::e_FLOAT, 1.5);
#endif
