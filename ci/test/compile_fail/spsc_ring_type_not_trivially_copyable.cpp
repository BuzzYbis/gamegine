// EXPECT-ERROR: SpscRingParameters
// A ring of a type that is copied byte by byte but cannot be.
#include "probe.h"
#include "engine/core/core_spsc_ring.h"
#include <string>

using engine::core::log::LogMessage;

#ifdef GOOD
static_assert(sizeof(engine::core::spsc::SpscRing<int, 8>) > 0);
#else
static_assert(sizeof(engine::core::spsc::SpscRing<std::string, 8>) > 0);
#endif
