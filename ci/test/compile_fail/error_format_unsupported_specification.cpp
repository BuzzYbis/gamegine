// EXPECT-ERROR: reject_format_spec
// A literal format specification an Error does not support.
#include "probe.h"
#include "engine/core/core_error_format.h"
#include <format>
#include <string>

using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] std::string f(const engine::core::error::Error e) { return std::format("{:#}", e); }
#else
[[maybe_unused]] std::string f(const engine::core::error::Error e) { return std::format("{:x}", e); }
#endif
