// EXPECT-ERROR: no matching function
// The type of a record is set by the factory, so a caller cannot give one
// that disagrees with the payload.
#include "engine/core/core_log_record.h"

using engine::core::error::Error;
using engine::core::log::LogRecord;
using engine::core::log::LogRecordType;
using engine::core::log::Severity;

#ifdef GOOD
[[maybe_unused]] LogRecord f(const Error e) { return LogRecord::make(Severity::e_WARN, 1, e); }
#else
[[maybe_unused]] LogRecord f(const Error e) { return LogRecord::make(LogRecordType::e_MESSAGE, Severity::e_WARN, 1, e); }
#endif
