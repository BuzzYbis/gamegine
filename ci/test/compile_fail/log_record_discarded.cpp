// EXPECT-ERROR: nodiscard
// A record is made to be pushed.
#include "engine/core/core_log_record.h"

using engine::core::error::Error;
using engine::core::log::LogRecord;
using engine::core::log::Severity;

#ifdef GOOD
[[maybe_unused]] void f(const Error e) { const LogRecord kept = LogRecord::make(Severity::e_WARN, 1, e); (void)kept; }
#else
[[maybe_unused]] void f(const Error e) { LogRecord::make(Severity::e_WARN, 1, e); }
#endif
