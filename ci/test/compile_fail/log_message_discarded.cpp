// EXPECT-ERROR: discarded
// A message is made and dropped.
#include "probe.h"


using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] const LogMessage kept = LogMessage::make(LogMessage::CoreMessage::e_UNKNOWN);
#else
void f() { LogMessage::make(LogMessage::CoreMessage::e_UNKNOWN); }
#endif
