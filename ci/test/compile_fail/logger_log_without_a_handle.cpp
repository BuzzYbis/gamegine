// EXPECT-ERROR: no member named
// Logging without a registered thread does not compile: there is no call to
// make but through a handle.
#include "engine/core/core_log.h"

using engine::core::Result;
using engine::core::error::Error;
using engine::core::log::Logger;
using engine::core::log::Severity;

void use(Logger& logger, const Error error)
{
#ifdef GOOD
    Result<Logger::Handle> handle = logger.register_current_thread("t");
    if (handle) {
        handle->log_error(Severity::e_INFO, error);
    }
#else
    logger.log_error(Severity::e_INFO, error);
    Logger::log_error(Severity::e_INFO, error);
#endif
}
