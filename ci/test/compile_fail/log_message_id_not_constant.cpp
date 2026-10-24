// EXPECT-ERROR: constant
// The id is a run-time value, so it cannot be checked when the call is compiled.
#include "probe.h"


using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] LogMessage f() { return LogMessage::make(Probe::e_UNSIGNED, 1U); }
#else
[[maybe_unused]] LogMessage f(const Probe id) { return LogMessage::make(id, 1U); }
#endif
