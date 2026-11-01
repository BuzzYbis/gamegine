// EXPECT-ERROR: reject_format_spec
// A literal format specification a LogMessage does not support.
#include "probe.h"
#include "engine/core/core_log_message_format.h"
#include <format>
#include <string>

using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] std::string f(const LogMessage m) { return std::format("{:#}", m); }
#else
[[maybe_unused]] std::string f(const LogMessage m) { return std::format("{:x}", m); }
#endif
