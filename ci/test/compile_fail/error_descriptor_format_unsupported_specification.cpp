// EXPECT-ERROR: reject_format_spec
// A descriptor has no raw value, so "{:#}" means nothing for it.
#include "engine/core/core_error_format.h"

#include <format>
#include <string>

using engine::core::error::ErrorDescriptor;

#ifdef GOOD
[[maybe_unused]] std::string f(const ErrorDescriptor d) { return std::format("{}", d); }
#else
[[maybe_unused]] std::string f(const ErrorDescriptor d) { return std::format("{:#}", d); }
#endif
