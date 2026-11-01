// EXPECT-ERROR: reject_text
// Two placeholders for one argument.
#include "probe.h"
#include "engine/core/core_log_message_catalog.h"

using engine::core::log::LogMessage;
using engine::core::log::LogMessageText;

#ifdef GOOD
[[maybe_unused]] constexpr LogMessageText k_TEXT("a {} b", LogMessage::Argument::e_UNSIGNED);
#else
[[maybe_unused]] constexpr LogMessageText k_TEXT("a {} b {}", LogMessage::Argument::e_UNSIGNED);
#endif
