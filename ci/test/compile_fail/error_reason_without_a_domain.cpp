// EXPECT-ERROR: ErrorReasonType
// A reason enumeration that no domain claims.
#include "probe.h"
#include "engine/core/core_error.h"

using engine::core::log::LogMessage;

#ifdef GOOD
[[maybe_unused]] constexpr engine::core::error::Error k_ERROR = engine::core::error::Error::make(engine::core::error::Error::Kind::e_UNKNOWN, engine::core::error::Error::CoreReason::e_UNKNOWN);
#else
enum class Stray : std::uint16_t { e_ANY };
[[maybe_unused]] constexpr engine::core::error::Error k_ERROR = engine::core::error::Error::make(engine::core::error::Error::Kind::e_UNKNOWN, Stray::e_ANY);
#endif
