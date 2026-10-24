// EXPECT-ERROR: reject_text
// An argument with no placeholder to show it.
#include "probe.h"
#include "engine/core/core_log_message_catalog.h"

using engine::core::log::LogMessage;
using engine::core::log::LogMessageText;

#ifdef GOOD
[[maybe_unused]] constexpr LogMessageText k_TEXT("streamed {} pages", LogMessage::Argument::e_UNSIGNED);
#else
[[maybe_unused]] constexpr LogMessageText k_TEXT("streamed pages", LogMessage::Argument::e_UNSIGNED);
#endif
