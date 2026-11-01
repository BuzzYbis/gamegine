// EXPECT-ERROR: reject_text
// A text for a message that does not exist.
#include "probe.h"
#include "engine/core/core_log_message_catalog.h"

using engine::core::log::LogMessage;
using engine::core::log::LogMessageText;

#ifdef GOOD
[[maybe_unused]] constexpr auto k_TEXTS = LogMessageText::table<LogMessage::CoreMessage>({"unknown"});
#else
[[maybe_unused]] constexpr auto k_TEXTS = LogMessageText::table<LogMessage::CoreMessage>({"unknown", "one too many"});
#endif
