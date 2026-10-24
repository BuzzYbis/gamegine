// EXPECT-ERROR: deleted
// A live handle cannot be assigned over: that would drop its thread's ring.
#include "engine/core/core_log.h"

#include <utility>

using engine::core::Result;
using engine::core::error::Error;
using engine::core::log::Logger;
using engine::core::log::Severity;

void use(Logger& logger)
{
    Result<Logger::Handle> first  = logger.register_current_thread("a");
    Result<Logger::Handle> second = logger.register_current_thread("b");
    if (!first || !second) {
        return;
    }
#ifdef GOOD
    Logger::Handle moved(std::move(*second));
    (void)moved;
#else
    *first = std::move(*second);
#endif
}
