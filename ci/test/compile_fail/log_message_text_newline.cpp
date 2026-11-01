// EXPECT-ERROR: reject_text
// A text with a newline would split a log line in two.
#include "probe.h"
#include "engine/core/core_log_message_catalog.h"

using engine::core::log::LogMessage;
using engine::core::log::LogMessageText;

#ifdef GOOD
[[maybe_unused]] constexpr LogMessageText k_TEXT("two lines", LogMessage::Argument::e_NONE);
#else
[[maybe_unused]] constexpr LogMessageText k_TEXT("two\nlines", LogMessage::Argument::e_NONE);
#endif
