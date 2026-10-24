// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_spsc_ring.t.cpp                                               -*-C++-*-

//@PURPOSE: Verify that 'SpscRing' hands every value from its producer to its
// consumer once and in order, refuses exactly when it is full or empty, and
// keeps its two counters on separate cache lines.
//
//@DESCRIPTION: Every thread that logs pushes into one of these rings, and the
// logger thread pops from all of them. Five properties have to hold:
//
//: o It is a queue. Values come out in the order they went in, a full ring
//:   refuses the next one and changes nothing, an empty ring has nothing to
//:   give, and the slots wrap around many times over. A random mix of
//:   single pushes, batch pushes and pops is checked against 'std::deque',
//:   in phases that keep pushing past full and popping past empty, so
//:   every refusal is checked against the model.
//:
//: o A batch is all or nothing: one that does not fit appends none of its
//:   values, and one that fits wraps across the end of the slots intact.
//:
//: o It takes what it promises to: any trivially copyable type, with or
//:   without a default constructor, with padding, at any size. The
//:   concept that decides is tested for every kind of argument.
//:
//: o The layout keeps the producer's counter and the consumer's on lines of
//:   their own, so that the two threads do not slow each other down. The
//:   cache line constant is checked here, in a '.cpp' file, where GCC does
//:   not warn about the standard constant it is compared with.
//:
//: o Two threads lose nothing and see nothing torn. A producer and a
//:   consumer, spinning, move every value across once and in order, with
//:   records big enough that a missing release or acquire shows as a
//:   half-written record. Run under ThreadSanitizer, a data race is a
//:   failure. A ring of two slots forces the full and empty paths on nearly
//:   every call.

#include <engine/core/core_spsc_ring.h>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using engine::core::spsc::k_CACHE_LINE_SIZE;
using engine::core::spsc::SpscRing;
using engine::core::spsc::SpscRingParameters;

/// A trivially copyable record of 't_WORDS' 64-bit words, each one derived
/// from the first, so that a record written half-way is detectable.
template <std::size_t t_WORDS> struct Item {
    std::array<std::uint64_t, t_WORDS> words;

    friend bool operator==(const Item&, const Item&) = default;
};

/// Return the item made from the specified 'value'.
template <class t_ITEM> t_ITEM make_item(const std::uint64_t value)
{
    t_ITEM item{};
    for (std::size_t i = 0; i < item.words.size(); ++i) {
        item.words[i] = value + (~value * i);
    }
    return item;
}

/// Return 'count' items made from the values 'first', 'first' + 1, ...
template <class t_ITEM>
std::vector<t_ITEM> make_items(const std::uint64_t first,
                               const std::size_t   count)
{
    std::vector<t_ITEM> items;
    items.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        items.push_back(make_item<t_ITEM>(first + i));
    }
    return items;
}

/// Return the value 'item' was made from.
template <class t_ITEM> std::uint64_t value_of(const t_ITEM& item)
{
    return item.words[0];
}

/// A trivially copyable type with no default constructor, like 'Error'.
class Opaque {
  private:
    std::uint64_t d_value;

  public:
    explicit Opaque(const std::uint64_t value)
    : d_value(value)
    {
    }

    [[nodiscard]] std::uint64_t value() const
    {
        return d_value;
    }
};

/// A trivially copyable type with padding between and after its members.
struct Padded {
    std::uint8_t  small;
    std::uint64_t large;
    std::uint16_t medium;
};

/// A small pseudo-random sequence that is the same on every run, so that a
/// failure reproduces.
class Sequence {
  private:
    std::uint64_t d_state = 0x9E37'79B9'7F4A'7C15U;

  public:
    std::uint64_t next()
    {
        d_state ^= d_state << 13U;
        d_state ^= d_state >> 7U;
        d_state ^= d_state << 17U;
        return d_state;
    }
};

using Record = Item<2>;  // the logger's record: 16 bytes

// ------------------------------------------------------------- parameters --

TEST(CoreSpscRingParameters, AcceptsATrivialTypeAndAPowerOfTwo)
{
    static_assert(SpscRingParameters<std::int32_t, 1>);
    static_assert(SpscRingParameters<std::int32_t, 2>);
    static_assert(SpscRingParameters<std::int32_t, 4096>);
    static_assert(SpscRingParameters<Record, 8>);
    static_assert(SpscRingParameters<Opaque, 8>);
    static_assert(SpscRingParameters<Padded, 8>);
    static_assert(SpscRingParameters<std::byte, std::size_t{1} << 20U>);

    SUCCEED();
}

TEST(CoreSpscRingParameters, RejectsACapacityThatIsNotAPowerOfTwo)
{
    static_assert(!SpscRingParameters<std::int32_t, 0>);
    static_assert(!SpscRingParameters<std::int32_t, 3>);
    static_assert(!SpscRingParameters<std::int32_t, 6>);
    static_assert(!SpscRingParameters<std::int32_t, 1000>);
    static_assert(!SpscRingParameters<std::int32_t, 4095>);

    SUCCEED();
}

TEST(CoreSpscRingParameters, RejectsATypeThatIsNotTriviallyCopyable)
{
    // Slots are raw bytes copied in and out: a type with a destructor or a
    // copy constructor of its own would be bypassed.
    static_assert(!SpscRingParameters<std::string, 8>);
    static_assert(!SpscRingParameters<std::vector<std::int32_t>, 8>);
    static_assert(!SpscRingParameters<std::unique_ptr<std::int32_t>, 8>);

    SUCCEED();
}

// -------------------------------------------------------------- constants --

TEST(CoreSpscRingConstants, CacheLineSizeIsAPowerOfTwo)
{
    EXPECT_TRUE(std::has_single_bit(k_CACHE_LINE_SIZE));
}

TEST(CoreSpscRingConstants, CacheLineSizeCoversTheStandardConstructiveSize)
{
    // The standard constants are not used to lay the ring out (see the
    // constant's documentation), but a bad edit of ours has to fail here:
    // two things written by different threads closer than the smallest line
    // there is would share one.
    EXPECT_GE(k_CACHE_LINE_SIZE, std::hardware_constructive_interference_size);
}

// ----------------------------------------------------------------- layout --

TEST(CoreSpscRingLayout, StartsOnACacheLine)
{
    static_assert(alignof(SpscRing<Record, 8>) >= k_CACHE_LINE_SIZE);
    static_assert(alignof(SpscRing<std::byte, 1>) >= k_CACHE_LINE_SIZE);

    SUCCEED();
}

TEST(CoreSpscRingLayout, GivesEachCounterAndTheSlotsALineOfTheirOwn)
{
    // The producer's counter and its copy of the consumer's, then the
    // consumer's counter and its copy, then the slots: three blocks, each
    // starting on a line, so 'sizeof' is exactly three lines for slots that
    // fit in one. A counter that slipped onto its neighbour's line would
    // make the object smaller.
    using Ring = SpscRing<Record, 8>;

    static_assert(8 * sizeof(Record) <= k_CACHE_LINE_SIZE);
    static_assert(sizeof(Ring) == 3 * k_CACHE_LINE_SIZE);

    // Slots that spill over a line are rounded up to the next one.
    using Big = SpscRing<Record, 64>;
    static_assert(64 * sizeof(Record) == 8 * k_CACHE_LINE_SIZE);
    static_assert(sizeof(Big) == (2 + 8) * k_CACHE_LINE_SIZE);

    SUCCEED();
}

TEST(CoreSpscRingLayout, AHeapAllocatedRingIsAlignedToo)
{
    // The alignment survives the allocation, which would not be the case if
    // it came from 'malloc' or from a buffer of 'char'.
    const auto ring = std::make_unique<SpscRing<Record, 8>>();

    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(ring.get()) % k_CACHE_LINE_SIZE,
              0U);
}

// -------------------------------------------------------------- interface --

TEST(CoreSpscRing, CapacityIsTheTemplateArgument)
{
    static_assert(SpscRing<Record, 1>::capacity() == 1);
    static_assert(SpscRing<Record, 8>::capacity() == 8);
    static_assert(SpscRing<Record, 4096>::capacity() == 4096);

    SUCCEED();
}

TEST(CoreSpscRing, IsNotCopiedOrMoved)
{
    // One address, which both threads hold: a copy would split the counters
    // from the slots they describe.
    using Ring = SpscRing<Record, 8>;

    static_assert(!std::is_copy_constructible_v<Ring>);
    static_assert(!std::is_copy_assignable_v<Ring>);
    static_assert(!std::is_move_constructible_v<Ring>);
    static_assert(!std::is_move_assignable_v<Ring>);

    SUCCEED();
}

TEST(CoreSpscRing, CannotFail)
{
    // Both threads call these on every record. One that could throw -- or,
    // with exceptions off, abort -- would take the logger down.
    using Ring = SpscRing<Record, 8>;

    static_assert(noexcept(std::declval<Ring&>().try_push(Record{})));
    static_assert(
        noexcept(std::declval<Ring&>().try_push(std::span<const Record>{})));
    static_assert(noexcept(std::declval<Ring&>().try_pop()));
    static_assert(std::atomic_uint64_t::is_always_lock_free);

    SUCCEED();
}

// ------------------------------------------------------------------ queue --

/// The same properties hold at every size of record.
template <class t_ITEM> class CoreSpscRingQueue : public testing::Test {};

using ItemTypes = testing::Types<Item<1>, Item<2>, Item<3>, Item<8>>;
TYPED_TEST_SUITE(CoreSpscRingQueue, ItemTypes);

TYPED_TEST(CoreSpscRingQueue, EmptyRingHasNothingToGive)
{
    const auto ring = std::make_unique<SpscRing<TypeParam, 8>>();

    EXPECT_FALSE(ring->try_pop().has_value());
    EXPECT_FALSE(ring->try_pop().has_value());
}

TYPED_TEST(CoreSpscRingQueue, GivesBackWhatWasPushedInOrder)
{
    const auto ring = std::make_unique<SpscRing<TypeParam, 8>>();

    for (std::uint64_t i = 0; i < 5; ++i) {
        EXPECT_TRUE(ring->try_push(make_item<TypeParam>(i)));
    }
    for (std::uint64_t i = 0; i < 5; ++i) {
        const std::optional<TypeParam> item = ring->try_pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(*item, make_item<TypeParam>(i));
    }
    EXPECT_FALSE(ring->try_pop().has_value());
}

TYPED_TEST(CoreSpscRingQueue, FullRingRefusesAndChangesNothing)
{
    const auto ring = std::make_unique<SpscRing<TypeParam, 8>>();

    for (std::uint64_t i = 0; i < 8; ++i) {
        ASSERT_TRUE(ring->try_push(make_item<TypeParam>(i)));
    }
    EXPECT_FALSE(ring->try_push(make_item<TypeParam>(100)));
    EXPECT_FALSE(ring->try_push(make_item<TypeParam>(101)));

    // The eight come out in order, and the refused two are not among them.
    for (std::uint64_t i = 0; i < 8; ++i) {
        const std::optional<TypeParam> item = ring->try_pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(*item, make_item<TypeParam>(i));
    }
    EXPECT_FALSE(ring->try_pop().has_value());
}

TYPED_TEST(CoreSpscRingQueue, OnePopMakesRoomForExactlyOnePush)
{
    const auto ring = std::make_unique<SpscRing<TypeParam, 8>>();
    for (std::uint64_t i = 0; i < 8; ++i) {
        ASSERT_TRUE(ring->try_push(make_item<TypeParam>(i)));
    }

    // The producer has seen a full ring and holds that view: popping must
    // make the next push succeed, which means it looks again.
    ASSERT_TRUE(ring->try_pop().has_value());
    EXPECT_TRUE(ring->try_push(make_item<TypeParam>(8)));
    EXPECT_FALSE(ring->try_push(make_item<TypeParam>(9)));
}

TYPED_TEST(CoreSpscRingQueue, DrainedRingTakesAFullLoadAgain)
{
    // The consumer has seen an empty ring and holds that view, and the
    // producer a full one: both have to look again, over and over, while the
    // counters pass the capacity many times.
    const auto ring = std::make_unique<SpscRing<TypeParam, 8>>();

    std::uint64_t next     = 0;
    std::uint64_t expected = 0;
    for (std::int32_t round = 0; round < 50; ++round) {
        for (std::int32_t i = 0; i < 8; ++i) {
            ASSERT_TRUE(ring->try_push(make_item<TypeParam>(next++)));
        }
        ASSERT_FALSE(ring->try_push(make_item<TypeParam>(next)));
        for (std::int32_t i = 0; i < 8; ++i) {
            const std::optional<TypeParam> item = ring->try_pop();
            ASSERT_TRUE(item.has_value());
            ASSERT_EQ(*item, make_item<TypeParam>(expected++));
        }
        ASSERT_FALSE(ring->try_pop().has_value());
    }
}

TYPED_TEST(CoreSpscRingQueue, AgreesWithAStandardQueueOnRandomOperations)
{
    // Single pushes, batch pushes and pops in a random mix. The mix leans
    // towards pushing for a stretch, then towards popping, so that the ring
    // keeps hitting full and empty. The model says what each call must
    // answer, so every refusal is checked, and what comes out is checked
    // against what it holds.
    constexpr std::size_t k_CAPACITY   = 16;
    constexpr std::size_t k_OPERATIONS = 200'000;
    constexpr std::size_t k_PHASE      = 4096;

    const auto ring = std::make_unique<SpscRing<TypeParam, k_CAPACITY>>();
    std::deque<std::uint64_t> model;
    Sequence                  random;
    std::uint64_t             next = 0;

    for (std::size_t op = 0; op < k_OPERATIONS; ++op) {
        const bool          pushing_phase = (op / k_PHASE) % 2 == 0;
        const std::uint64_t pick          = random.next() % 100;
        const std::uint64_t push_share    = pushing_phase ? 70 : 30;

        if (pick >= push_share) {
            const std::optional<TypeParam> item = ring->try_pop();
            ASSERT_EQ(item.has_value(), !model.empty()) << "op " << op;
            if (item.has_value()) {
                ASSERT_EQ(*item, make_item<TypeParam>(model.front()))
                    << "op " << op;
                model.pop_front();
            }
        } else if (pick % 4 == 0) {
            const std::size_t            count = 1 + (random.next() % 6);
            const std::vector<TypeParam> batch = make_items<TypeParam>(next,
                                                                       count);
            const bool fits = model.size() + count <= k_CAPACITY;
            ASSERT_EQ(ring->try_push(std::span<const TypeParam>(batch)), fits)
                << "op " << op << ", batch of " << count;
            if (fits) {
                for (std::size_t i = 0; i < count; ++i) {
                    model.push_back(next++);
                }
            }
        } else {
            const bool fits = model.size() < k_CAPACITY;
            ASSERT_EQ(ring->try_push(make_item<TypeParam>(next)), fits)
                << "op " << op;
            if (fits) {
                model.push_back(next++);
            }
        }
    }

    // Whatever is left comes out, in order.
    while (!model.empty()) {
        const std::optional<TypeParam> item = ring->try_pop();
        ASSERT_TRUE(item.has_value());
        ASSERT_EQ(*item, make_item<TypeParam>(model.front()));
        model.pop_front();
    }
    EXPECT_FALSE(ring->try_pop().has_value());
}

TEST(CoreSpscRing, OneSlotRingAlternatesFullAndEmpty)
{
    const auto ring = std::make_unique<SpscRing<Record, 1>>();

    for (std::uint64_t i = 0; i < 1000; ++i) {
        ASSERT_FALSE(ring->try_pop().has_value());
        ASSERT_TRUE(ring->try_push(make_item<Record>(i)));
        ASSERT_FALSE(ring->try_push(make_item<Record>(i + 1)));
        const std::optional<Record> item = ring->try_pop();
        ASSERT_TRUE(item.has_value());
        ASSERT_EQ(*item, make_item<Record>(i));
    }
}

// ------------------------------------------------------------------ batch --

TEST(CoreSpscRingBatch, AppendsEveryValueInOrder)
{
    const auto                ring  = std::make_unique<SpscRing<Record, 8>>();
    const std::vector<Record> batch = make_items<Record>(0, 5);

    ASSERT_TRUE(ring->try_push(std::span<const Record>(batch)));

    for (std::uint64_t i = 0; i < 5; ++i) {
        const std::optional<Record> item = ring->try_pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(*item, make_item<Record>(i));
    }
    EXPECT_FALSE(ring->try_pop().has_value());
}

TEST(CoreSpscRingBatch, IsAllOrNothing)
{
    const auto                ring = std::make_unique<SpscRing<Record, 8>>();
    const std::vector<Record> five = make_items<Record>(0, 5);

    ASSERT_TRUE(ring->try_push(std::span<const Record>(five)));
    // Three slots are left, five do not fit: not even three go in.
    EXPECT_FALSE(ring->try_push(std::span<const Record>(five)));

    for (std::uint64_t i = 0; i < 5; ++i) {
        const std::optional<Record> item = ring->try_pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(*item, make_item<Record>(i));
    }
    EXPECT_FALSE(ring->try_pop().has_value());
}

TEST(CoreSpscRingBatch, ExactlyTheRoomLeftIsAccepted)
{
    const auto                ring  = std::make_unique<SpscRing<Record, 8>>();
    const std::vector<Record> three = make_items<Record>(0, 3);
    const std::vector<Record> five  = make_items<Record>(3, 5);

    ASSERT_TRUE(ring->try_push(std::span<const Record>(three)));
    ASSERT_TRUE(ring->try_push(std::span<const Record>(five)));
    EXPECT_FALSE(ring->try_push(make_item<Record>(8)));
}

TEST(CoreSpscRingBatch, LargerThanTheCapacityIsRefusedWhateverIsInTheRing)
{
    const auto          ring = std::make_unique<SpscRing<Record, 8>>();
    std::vector<Record> nine = make_items<Record>(0, 9);

    EXPECT_FALSE(ring->try_push(std::span<const Record>(nine)));
    EXPECT_FALSE(ring->try_pop().has_value());

    // A batch of exactly the capacity is the largest there is.
    nine.pop_back();
    EXPECT_TRUE(ring->try_push(std::span<const Record>(nine)));
}

TEST(CoreSpscRingBatch, EmptyBatchIsAcceptedAndChangesNothing)
{
    const auto ring = std::make_unique<SpscRing<Record, 8>>();

    EXPECT_TRUE(ring->try_push(std::span<const Record>{}));
    EXPECT_FALSE(ring->try_pop().has_value());

    for (std::uint64_t i = 0; i < 8; ++i) {
        ASSERT_TRUE(ring->try_push(make_item<Record>(i)));
    }
    EXPECT_TRUE(ring->try_push(std::span<const Record>{}));
    for (std::uint64_t i = 0; i < 8; ++i) {
        const std::optional<Record> item = ring->try_pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(*item, make_item<Record>(i));
    }
}

TEST(CoreSpscRingBatch, WrapsAcrossTheEndOfTheSlots)
{
    // Every offset of a batch of five in a ring of eight, so that the batch
    // starts at each slot in turn and crosses the end from each.
    for (std::uint64_t offset = 0; offset < 8; ++offset) {
        const auto ring = std::make_unique<SpscRing<Record, 8>>();
        for (std::uint64_t i = 0; i < offset; ++i) {
            ASSERT_TRUE(ring->try_push(make_item<Record>(1000 + i)));
            ASSERT_TRUE(ring->try_pop().has_value());
        }

        const std::vector<Record> batch = make_items<Record>(0, 5);
        ASSERT_TRUE(ring->try_push(std::span<const Record>(batch)))
            << "offset " << offset;

        for (std::uint64_t i = 0; i < 5; ++i) {
            const std::optional<Record> item = ring->try_pop();
            ASSERT_TRUE(item.has_value()) << "offset " << offset;
            ASSERT_EQ(*item, make_item<Record>(i)) << "offset " << offset;
        }
        EXPECT_FALSE(ring->try_pop().has_value());
    }
}

// ------------------------------------------------------------------ types --

TEST(CoreSpscRingTypes, HoldsATypeWithNoDefaultConstructor)
{
    static_assert(!std::is_default_constructible_v<Opaque>);
    const auto ring = std::make_unique<SpscRing<Opaque, 4>>();

    ASSERT_TRUE(ring->try_push(Opaque(42)));
    ASSERT_TRUE(ring->try_push(Opaque(43)));

    const std::optional<Opaque> first  = ring->try_pop();
    const std::optional<Opaque> second = ring->try_pop();
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->value(), 42U);
    EXPECT_EQ(second->value(), 43U);
}

TEST(CoreSpscRingTypes, HoldsATypeWithPadding)
{
    const auto ring = std::make_unique<SpscRing<Padded, 4>>();

    ASSERT_TRUE(
        ring->try_push(Padded{.small = 7, .large = ~0ULL, .medium = 0xBEEF}));

    const std::optional<Padded> item = ring->try_pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->small, 7U);
    EXPECT_EQ(item->large, ~0ULL);
    EXPECT_EQ(item->medium, 0xBEEFU);
}

TEST(CoreSpscRingTypes, HoldsOneByte)
{
    const auto ring = std::make_unique<SpscRing<std::uint8_t, 256>>();

    for (std::uint32_t i = 0; i < 256; ++i) {
        ASSERT_TRUE(ring->try_push(static_cast<std::uint8_t>(i)));
    }
    EXPECT_FALSE(ring->try_push(std::uint8_t{0}));
    for (std::uint32_t i = 0; i < 256; ++i) {
        const std::optional<std::uint8_t> item = ring->try_pop();
        ASSERT_TRUE(item.has_value());
        ASSERT_EQ(*item, static_cast<std::uint8_t>(i));
    }
}

// ---------------------------------------------------------------- threads --

/// How long a transfer may take before it is given up as stuck. Generous: a
/// transfer takes about a second under a sanitizer. A ring that stops
/// handing values over (a lost wake-up of one thread by the other) would
/// otherwise keep both threads yielding for ever and hang the test run.
constexpr std::chrono::seconds k_TRANSFER_DEADLINE{60};

/// Move 'count' values from one thread to another through a ring and expect
/// every one to arrive once, whole, and in order. The producer sends batches
/// of 'batch' values, and single values between them if 'alternate' is
/// 'true'. Batches only is the case that matters for the batch push's own
/// release: between batches a single push would publish them anyway.
template <class t_ITEM, std::size_t t_CAPACITY>
void expect_transfer(const std::uint64_t count,
                     const std::size_t   batch,
                     const bool          alternate = true)
{
    const auto        ring = std::make_unique<SpscRing<t_ITEM, t_CAPACITY>>();
    const auto        deadline = std::chrono::steady_clock::now() +
                                 k_TRANSFER_DEADLINE;
    std::atomic<bool> stuck{false};

    std::thread producer([&ring, &stuck, deadline, count, batch, alternate] {
        std::vector<t_ITEM> staged(batch);
        std::uint64_t       next   = 0;
        bool                single = alternate;
        while (next < count && !stuck.load(std::memory_order_relaxed)) {
            const std::uint64_t left   = count - next;
            const std::size_t   size   = (single || left < batch) ? 1 : batch;
            bool                pushed = false;
            if (size == 1) {
                pushed = ring->try_push(make_item<t_ITEM>(next));
            } else {
                for (std::size_t i = 0; i < size; ++i) {
                    staged[i] = make_item<t_ITEM>(next + i);
                }
                pushed = ring->try_push(std::span<const t_ITEM>(staged.data(),
                                                                size));
            }
            if (pushed) {
                next += size;
                single = alternate && !single;
            } else {
                if (std::chrono::steady_clock::now() > deadline) {
                    stuck.store(true, std::memory_order_relaxed);
                }
                std::this_thread::yield();
            }
        }
    });

    std::uint64_t expected  = 0;
    std::uint64_t torn      = 0;
    std::uint64_t reordered = 0;
    while (expected < count && !stuck.load(std::memory_order_relaxed)) {
        const std::optional<t_ITEM> item = ring->try_pop();
        if (!item.has_value()) {
            if (std::chrono::steady_clock::now() > deadline) {
                stuck.store(true, std::memory_order_relaxed);
            }
            std::this_thread::yield();
            continue;
        }
        if (*item != make_item<t_ITEM>(value_of(*item))) {
            ++torn;
        }
        if (value_of(*item) != expected) {
            ++reordered;
        }
        ++expected;
    }
    producer.join();

    EXPECT_FALSE(stuck.load()) << "no progress for " << k_TRANSFER_DEADLINE
                               << " after " << expected << " of " << count;
    EXPECT_EQ(torn, 0U) << "records that were not whole";
    EXPECT_EQ(reordered, 0U) << "records out of order";
    if (!stuck.load()) {
        EXPECT_FALSE(ring->try_pop().has_value());
    }
}

TEST(CoreSpscRingThreads, MovesEveryValueOnceInOrder)
{
    expect_transfer<Record, 1024>(500'000, 8);
}

TEST(CoreSpscRingThreads, MovesEightByteValuesOnceInOrder)
{
    // ThreadSanitizer follows accesses of up to 8 bytes: a missing release or
    // acquire on a wider slot, which the compiler copies with vector moves,
    // goes unreported by it. Ordering is therefore checked here on 8-byte
    // values, and the wider records above check that none is torn.
    expect_transfer<Item<1>, 256>(500'000, 8);
}

TEST(CoreSpscRingThreads, MovesBatchesAloneOnceInOrder)
{
    // Nothing but batches: the consumer can only learn of a batch from the
    // batch's own release, never from a single push after it. 8-byte values,
    // for the reason given above.
    expect_transfer<Item<1>, 256>(500'000, 8, false);
}

TEST(CoreSpscRingThreads, NeverShowsAHalfWrittenRecord)
{
    // 64-byte records: a consumer that read a slot before the producer's
    // release would see some words of the new record and some of the old.
    expect_transfer<Item<8>, 256>(500'000, 4);
}

TEST(CoreSpscRingThreads, SurvivesAlwaysBeingFullOrEmpty)
{
    // Two slots: nearly every call finds the ring full or empty and takes
    // the path that looks at the other thread's counter.
    expect_transfer<Record, 2>(200'000, 2);
}

}  // close unnamed namespace
