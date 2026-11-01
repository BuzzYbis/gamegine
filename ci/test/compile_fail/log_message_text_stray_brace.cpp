// EXPECT-ERROR: reject_text
// A brace that is not the placeholder.
#include "probe.h"
#include "engine/core/core_log_message_catalog.h"

using engine::core::log::LogMessage;
using engine::core::log::LogMessageText;

#ifdef GOOD
[[maybe_unused]] constexpr LogMessageText k_TEXT("a b", LogMessage::Argument::e_NONE);
#else
[[maybe_unused]] constexpr LogMessageText k_TEXT("a { b", LogMessage::Argument::e_NONE);
#endif
