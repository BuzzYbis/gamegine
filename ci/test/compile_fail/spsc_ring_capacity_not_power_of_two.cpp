// EXPECT-ERROR: SpscRingParameters
// A ring whose capacity is not a power of two.
#include "probe.h"
#include "engine/core/core_spsc_ring.h"

using engine::core::log::LogMessage;

#ifdef GOOD
static_assert(sizeof(engine::core::spsc::SpscRing<int, 8>) > 0);
#else
static_assert(sizeof(engine::core::spsc::SpscRing<int, 6>) > 0);
#endif
