// EXPECT-ERROR: discarded
// An error is made and dropped.
#include "probe.h"
#include "engine/core/core_error.h"

using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] const auto kept = engine::core::error::Error::make(engine::core::error::Error::Kind::e_UNKNOWN, engine::core::error::Error::CoreReason::e_UNKNOWN);
#else
void f() { engine::core::error::Error::make(engine::core::error::Error::Kind::e_UNKNOWN, engine::core::error::Error::CoreReason::e_UNKNOWN); }
#endif
