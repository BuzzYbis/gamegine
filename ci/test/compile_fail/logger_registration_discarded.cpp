// EXPECT-ERROR: nodiscard
// A registration that is not looked at is a handle closed at once, or an
// error nobody saw.
#include "engine/core/core_log.h"

using engine::core::Result;
using engine::core::error::Error;
using engine::core::log::Logger;
using engine::core::log::Severity;

void use(Logger& logger)
{
#ifdef GOOD
    Result<Logger::Handle> handle = logger.register_current_thread("t");
    (void)handle;
#else
    logger.register_current_thread("t");
#endif
}
