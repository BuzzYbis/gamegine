// EXPECT-ERROR: deleted
// A record carries an Error or a LogMessage, never a number.
#include "engine/core/core_log_record.h"

using engine::core::log::LogMessage;
using engine::core::log::LogRecord;
using engine::core::log::Severity;

#ifdef GOOD
[[maybe_unused]] constexpr LogRecord k_RECORD = LogRecord::make(Severity::e_WARN, 1, LogMessage::make(LogMessage::CoreMessage::e_UNKNOWN));
#else
[[maybe_unused]] constexpr LogRecord k_RECORD = LogRecord::make(Severity::e_WARN, 1, 42U);
#endif
