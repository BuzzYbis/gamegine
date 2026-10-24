// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_spsc_ring.b.cpp                                               -*-C++-*-

//@PURPOSE: Measure how many values per second 'SpscRing' sustains.
//
//@DESCRIPTION: Every log call pushes into one of these rings on the thread
// that logs, and the logger thread pops from all of them. Three things are
// measured on one ring, at three sizes of value (8 bytes, like an 'Error'; 16,
// the log record; 64) in the ring of 4096 slots a thread logs into, then two
// on many rings, with the 16-byte record:
//
//: o 'PushPopPair': one thread pushes a value and pops it. The cost of the
//:   calls alone, nothing contended.
//:
//: o 'FillThenDrain': one thread pushes 4096 values, then pops 4096. The
//:   same, with the ring filling and emptying as a batch drain does.
//:
//: o 'Transfer': a producer thread pushes and this one pops, both spinning,
//:   so that the ring is as busy as it gets and each value crosses between
//:   two cores. The sustained rate of a ring that is never idle; the real
//:   ring idles between drains. Run for a producer pushing single values and
//:   one pushing batches of 8, and for each placement of the two threads
//:   that the platform can ask for.
//:
//: o 'FanInSpin': 1, 2, 3 or 7 producer threads, each with a ring of its own,
//:   and this thread popping them round robin, at most a ring's worth from
//:   each in turn so that a fast producer cannot hold the others back. A
//:   producer whose ring is full spins until it has room, so no value is
//:   lost and the rate is the most the consumer takes in from all of them.
//:   The count that leaves a core for the consumer (3, on 4 performance
//:   cores) and the one that uses every core (7) are in the list.
//:
//: o 'FanInDrop': the same threads behaving as the logger does. A producer
//:   never waits: a push that finds the ring full is counted as refused and
//:   the producer moves on. The consumer sleeps a millisecond between passes
//:   and each iteration runs for 100 milliseconds. 'items_per_second' is
//:   what the consumer received, 'attempts' what the producers tried to push
//:   (they push far faster than the consumer pops, so most are refused),
//:   'refused_%' the share that found a full ring, and 'passes' the passes
//:   the consumer made (a sleep of a millisecond lasts longer, and longer
//:   still when every core is busy). Every value pushed and accepted must
//:   come out, once and in order: only the refused are lost.
//
// Each benchmark reports 'items_per_second', the values that went through the
// ring, each pushed once and popped once, and 'ops', the pushes plus the pops
// (twice as many). Build with the release flags ('-O3 -DNDEBUG') and run
// with repetitions, for a median:
// ```
//  xmake f -m release && xmake build benchmarks
//  xmake run benchmarks --benchmark_repetitions=5
// ```
// ('--benchmark_report_aggregates_only=true' keeps just the median and the
// mean.)
//
// On macOS the two threads are placed by asking for a quality of service,
// which only biases the choice of core, and the OS does not say where a
// thread ran: the label says what was asked for ('asked P + E'), not what
// happened. Each iteration of a 'Transfer' starts the producer thread and
// joins it, inside the timed region; that costs tens of microseconds in
// about 25 milliseconds. Every benchmark checks that the values came back
// right, and skips itself with an error if they did not. TODO(Profile L): pin
// the threads to chosen cores with 'pthread_setaffinity_np' (both on P-cores,
// one on each kind, both on E-cores, and both on one core's two threads), and
// measure at the same time how far apart in bytes two counters must be (see
// 'k_CACHE_LINE_SIZE').

#include <engine/core/core_spsc_ring.h>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#ifdef __APPLE__
#include <pthread.h>
#include <pthread/qos.h>
#endif

namespace {

using engine::core::spsc::SpscRing;

/// The slots a thread logs into.
constexpr std::size_t k_CAPACITY = 4096;

/// Values moved by one iteration of a 'Transfer' benchmark.
constexpr std::uint64_t k_TRANSFERRED = std::uint64_t{1} << 20U;

/// Values each producer moves in one iteration of a 'FanInSpin' benchmark.
constexpr std::uint64_t k_PER_PRODUCER = std::uint64_t{1} << 18U;

/// How long a 'FanInSpin' consumer waits for values that do not come before
/// it gives up: a ring that loses one would otherwise leave it waiting for
/// ever.
constexpr std::chrono::seconds k_SPIN_TIMEOUT{60};

/// How long one iteration of a 'FanInDrop' benchmark runs.
constexpr std::chrono::milliseconds k_DROP_DURATION{100};

/// What the consumer of a 'FanInDrop' sleeps between two passes, as the
/// logger thread does ('Logger::k_DRAIN_INTERVAL').
constexpr std::chrono::milliseconds k_PASS_INTERVAL{1};

/// A trivially copyable value of 't_WORDS' 64-bit words.
template <std::size_t t_WORDS> struct Item {
    std::array<std::uint64_t, t_WORDS> d_words;
};

/// Return the item made from the specified 'value'.
template <class t_ITEM> t_ITEM make_item(const std::uint64_t value)
{
    t_ITEM item{};
    for (std::size_t i = 0; i < item.d_words.size(); ++i) {
        item.d_words[i] = value + i;
    }
    return item;
}

/// Enumeration used to choose the kind of core a thread asks to run on.
enum class Core : std::uint8_t {
    e_PERFORMANCE,  // a fast core
    e_EFFICIENCY,   // a slow, low-power core
};

/// Ask the OS to run the calling thread on the specified 'core' kind, as far
/// as the platform lets a thread say so: a quality of service on macOS,
/// nothing elsewhere.
void prefer(const Core core)
{
#ifdef __APPLE__
    (void)pthread_set_qos_class_self_np(core == Core::e_PERFORMANCE
                                            ? QOS_CLASS_USER_INTERACTIVE
                                            : QOS_CLASS_BACKGROUND,
                                        0);
#else
    (void)core;
#endif
}

/// Where the producer and the consumer thread of a 'Transfer' run.
struct Placement {
    Core        d_producer;
    Core        d_consumer;
    const char* d_label_p;
};

constexpr std::array<Placement, 3> k_PLACEMENTS = {
    Placement{Core::e_PERFORMANCE, Core::e_PERFORMANCE, "asked P + P"},
    Placement{Core::e_PERFORMANCE, Core::e_EFFICIENCY, "asked P + E"},
    Placement{Core::e_EFFICIENCY, Core::e_EFFICIENCY, "asked E + E"},
};

/// Make the specified 'benchmark' run for each placement the platform can
/// ask for: all of them on macOS, the default one elsewhere.
void for_each_placement(benchmark::Benchmark* benchmark)
{
    benchmark->Arg(0);
#ifdef __APPLE__
    benchmark->Arg(1)->Arg(2);
#endif
}

/// Return the number of iterations 'state' has run, as an unsigned number.
std::uint64_t iterations_of(const benchmark::State& state)
{
    return static_cast<std::uint64_t>(state.iterations());
}

/// Report the specified 'values' that went through the ring in 'state': each
/// was pushed once and popped once.
void report(benchmark::State& state, const std::uint64_t values)
{
    state.SetItemsProcessed(static_cast<std::int64_t>(values));
    state.counters["ops"] = benchmark::Counter(static_cast<double>(2 * values),
                                               benchmark::Counter::kIsRate);
}

// ---------------------------------------------------------- one thread -----

template <class t_ITEM> void push_pop_pair(benchmark::State& state)
{
    prefer(Core::e_PERFORMANCE);
    const auto ring = std::make_unique<SpscRing<t_ITEM, k_CAPACITY>>();

    std::uint64_t value    = 0;
    std::uint64_t checksum = 0;
    for ([[maybe_unused]] const auto _ : state) {
        t_ITEM item = make_item<t_ITEM>(value++);
        benchmark::DoNotOptimize(item);
        if (!ring->try_push(item)) {
            state.SkipWithError("a push was refused");
            return;
        }
        const std::optional<t_ITEM> popped = ring->try_pop();
        if (!popped.has_value()) {
            state.SkipWithError("a pop found nothing");
            return;
        }
        checksum += popped->d_words[0];
    }

    // What came out must be what went in: the values 0, 1, 2, ... in turn.
    const std::uint64_t count = iterations_of(state);
    if (checksum != count * (count - 1) / 2) {
        state.SkipWithError("a value came back wrong");
        return;
    }
    report(state, count);
}

template <class t_ITEM> void fill_then_drain(benchmark::State& state)
{
    prefer(Core::e_PERFORMANCE);
    const auto ring = std::make_unique<SpscRing<t_ITEM, k_CAPACITY>>();

    t_ITEM        item     = make_item<t_ITEM>(1);
    std::uint64_t checksum = 0;
    for ([[maybe_unused]] const auto _ : state) {
        benchmark::DoNotOptimize(item);
        for (std::size_t i = 0; i < k_CAPACITY; ++i) {
            if (!ring->try_push(item)) {
                state.SkipWithError("a push was refused");
                return;
            }
        }
        for (std::size_t i = 0; i < k_CAPACITY; ++i) {
            const std::optional<t_ITEM> popped = ring->try_pop();
            if (!popped.has_value()) {
                state.SkipWithError("a pop found nothing");
                return;
            }
            checksum += popped->d_words[0];
        }
    }

    // Every value pushed was a 1, and every one must have come back.
    const std::uint64_t count = iterations_of(state) * k_CAPACITY;
    if (checksum != count) {
        state.SkipWithError("a value came back wrong");
        return;
    }
    report(state, count);
}

// --------------------------------------------------------- two threads -----

template <class t_ITEM, std::size_t t_BATCH>
void transfer(benchmark::State& state)
{
    const Placement& placement =
        k_PLACEMENTS[static_cast<std::size_t>(state.range(0))];
    state.SetLabel(placement.d_label_p);
    prefer(placement.d_consumer);

    const auto ring = std::make_unique<SpscRing<t_ITEM, k_CAPACITY>>();

    for ([[maybe_unused]] const auto _ : state) {
        std::thread producer([&ring, &placement] {
            prefer(placement.d_producer);
            for (std::uint64_t next = 0; next < k_TRANSFERRED;
                 next += t_BATCH) {
                // Built once, then offered until the ring takes it.
                std::array<t_ITEM, t_BATCH> batch;
                for (std::size_t i = 0; i < t_BATCH; ++i) {
                    batch[i] = make_item<t_ITEM>(next + i);
                }
                if constexpr (t_BATCH == 1) {
                    while (!ring->try_push(batch[0])) {
                    }
                } else {
                    while (!ring->try_push(std::span<const t_ITEM>(batch))) {
                    }
                }
            }
        });

        std::uint64_t out_of_order = 0;
        for (std::uint64_t received = 0; received < k_TRANSFERRED;) {
            const std::optional<t_ITEM> item = ring->try_pop();
            if (item.has_value()) {
                out_of_order += (item->d_words[0] != received) ? 1U : 0U;
                ++received;
            }
        }
        producer.join();

        // A rate means nothing if the values did not arrive: every one, once,
        // in order, is checked on every iteration.
        if (out_of_order != 0) {
            state.SkipWithError("values were lost, repeated or reordered");
            break;
        }
    }
    report(state, iterations_of(state) * k_TRANSFERRED);
    prefer(Core::e_PERFORMANCE);
}

// ----------------------------------------------------------- many rings -----

/// The value the many-ring benchmarks move: 16 bytes, like a log record.
using Record = Item<2>;

/// The ring each producer thread pushes into.
using RecordRing = SpscRing<Record, k_CAPACITY>;

/// What one pass over the rings found.
struct Drained {
    std::uint64_t d_popped;  // values taken out
    std::uint64_t d_wrong;   // values that were not the next one of their ring
};

/// Return the specified 'count' rings, each on the heap as the ring wants.
std::vector<std::unique_ptr<RecordRing>> make_rings(const std::size_t count)
{
    std::vector<std::unique_ptr<RecordRing>> rings;
    rings.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        rings.push_back(std::make_unique<RecordRing>());
    }
    return rings;
}

/// Pop from each of the specified 'rings' in turn, at most a ring's worth from
/// each, so that a producer which refills its ring as fast as it is emptied
/// cannot keep the others waiting. 'expected[i]' is the value ring 'i' must
/// give next, and is moved on past whatever the ring did give, so that one
/// lost value is counted once and not on every value after it. Return what
/// was found.
Drained drain_once(const std::span<const std::unique_ptr<RecordRing>> rings,
                   const std::span<std::uint64_t>                     expected)
{
    Drained found{0, 0};
    for (std::size_t i = 0; i < rings.size(); ++i) {
        for (std::size_t taken = 0; taken < k_CAPACITY; ++taken) {
            const std::optional<Record> record = rings[i]->try_pop();
            if (!record.has_value()) {
                break;
            }
            // Both words are checked, so half a record is not a record.
            const bool right = record->d_words[0] == expected[i] &&
                               record->d_words[1] == expected[i] + 1;
            found.d_wrong += right ? 0U : 1U;
            expected[i] = record->d_words[0] + 1;
            ++found.d_popped;
        }
    }
    return found;
}

/// Make the specified 'benchmark' run for each producer count that leaves a
/// core to the consumer: 1, 2, 3 (the most that leaves a performance core
/// of the M2 to the consumer), and 7 (every core of the M2).
/// TODO(Profile L): choose the counts for the Core Ultra 9 185H.
void for_each_producer_count(benchmark::Benchmark* benchmark)
{
    benchmark->ArgName("producers");
    const std::uint32_t cores = std::thread::hardware_concurrency();
    for (const std::uint32_t producers : {1U, 2U, 3U, 7U}) {
        if (producers == 1U || producers < cores) {
            benchmark->Arg(producers);
        }
    }
}

void fan_in_spin(benchmark::State& state)
{
    const auto producers = static_cast<std::size_t>(state.range(0));
    state.SetLabel("asked P for all");
    prefer(Core::e_PERFORMANCE);

    const auto                 rings = make_rings(producers);
    std::vector<std::uint64_t> expected(producers);
    std::vector<std::thread>   threads;
    threads.reserve(producers);

    const std::uint64_t total = producers * k_PER_PRODUCER;
    for ([[maybe_unused]] const auto _ : state) {
        std::ranges::fill(expected, 0U);
        std::atomic<bool> give_up{false};
        for (std::size_t p = 0; p < producers; ++p) {
            threads.emplace_back([&ring = *rings[p], &give_up] {
                prefer(Core::e_PERFORMANCE);
                for (std::uint64_t next = 0; next < k_PER_PRODUCER; ++next) {
                    const Record record = make_item<Record>(next);
                    while (!ring.try_push(record)) {
                        if (give_up.load(std::memory_order_relaxed)) {
                            return;
                        }
                    }
                }
            });
        }

        std::uint64_t popped = 0;
        std::uint64_t wrong  = 0;
        std::uint64_t idle   = 0;
        const auto limit = std::chrono::steady_clock::now() + k_SPIN_TIMEOUT;
        while (popped < total) {
            const Drained found = drain_once(rings, expected);
            popped += found.d_popped;
            wrong += found.d_wrong;

            // The clock is read once in a long while, and only when nothing
            // came, so that waiting for a value costs what it costs.
            idle = (found.d_popped == 0) ? idle + 1 : 0;
            if (idle == (std::uint64_t{1} << 16U)) {
                idle = 0;
                if (std::chrono::steady_clock::now() > limit) {
                    give_up.store(true, std::memory_order_relaxed);
                    break;
                }
            }
        }
        for (std::thread& thread : threads) {
            thread.join();
        }
        threads.clear();

        // A rate means nothing if the values did not arrive: every one, once,
        // in order within its ring, is checked on every iteration.
        if (wrong != 0 || popped != total) {
            state.SkipWithError("values were lost, repeated or reordered");
            break;
        }
    }

    const std::uint64_t values = iterations_of(state) * total;
    report(state, values);
    state.counters["per_producer"] = benchmark::Counter(
        static_cast<double>(values) / static_cast<double>(producers),
        benchmark::Counter::kIsRate);
}

/// What a producer thread of a 'FanInDrop' did: pushes the ring took, and
/// pushes it refused.
struct Tally {
    std::uint64_t d_accepted;
    std::uint64_t d_refused;
};

void fan_in_drop(benchmark::State& state)
{
    const auto producers = static_cast<std::size_t>(state.range(0));
    state.SetLabel("asked P for all");
    prefer(Core::e_PERFORMANCE);

    const auto                 rings = make_rings(producers);
    std::vector<std::uint64_t> expected(producers);
    std::vector<Tally>         tallies(producers);
    std::vector<std::thread>   threads;
    threads.reserve(producers);

    std::uint64_t delivered = 0;
    std::uint64_t attempted = 0;
    std::uint64_t refused   = 0;
    std::uint64_t passes    = 0;
    for ([[maybe_unused]] const auto _ : state) {
        std::ranges::fill(expected, 0U);
        std::atomic<bool> stop{false};
        for (std::size_t p = 0; p < producers; ++p) {
            threads.emplace_back([&ring  = *rings[p],
                                  &tally = tallies[p],
                                  &stop] {
                prefer(Core::e_PERFORMANCE);
                Tally mine{0, 0};
                while (!stop.load(std::memory_order_relaxed)) {
                    // The value is the count of pushes accepted so far, so a
                    // value lost after it was accepted leaves a gap, while a
                    // refused push leaves none.
                    const Record record = make_item<Record>(mine.d_accepted);
                    if (ring.try_push(record)) {
                        ++mine.d_accepted;
                    } else {
                        ++mine.d_refused;
                    }
                }
                tally = mine;
            });
        }

        std::uint64_t wrong    = 0;
        std::uint64_t received = 0;
        const auto    deadline = std::chrono::steady_clock::now() +
                                 k_DROP_DURATION;
        while (std::chrono::steady_clock::now() < deadline) {
            const Drained found = drain_once(rings, expected);
            received += found.d_popped;
            wrong += found.d_wrong;
            ++passes;
            std::this_thread::sleep_for(k_PASS_INTERVAL);
        }
        stop.store(true, std::memory_order_relaxed);
        for (std::thread& thread : threads) {
            thread.join();
        }
        threads.clear();
        // The producers are gone: what is left in the rings is all there is.
        for (Drained found = drain_once(rings, expected); found.d_popped != 0;
             found         = drain_once(rings, expected)) {
            received += found.d_popped;
            wrong += found.d_wrong;
        }

        // Every push the rings accepted must have come out, and no other.
        std::uint64_t accepted = 0;
        std::uint64_t declined = 0;
        for (std::size_t p = 0; p < producers; ++p) {
            wrong += (expected[p] == tallies[p].d_accepted) ? 0U : 1U;
            accepted += tallies[p].d_accepted;
            declined += tallies[p].d_refused;
        }
        if (wrong != 0 || received != accepted) {
            state.SkipWithError("accepted values were lost, repeated or "
                                "reordered");
            break;
        }
        delivered += received;
        refused += declined;
        attempted += accepted + declined;
    }

    report(state, delivered);
    state.counters["attempts"] = benchmark::Counter(
        static_cast<double>(attempted), benchmark::Counter::kIsRate);
    state.counters["passes"] = benchmark::Counter(static_cast<double>(passes),
                                                  benchmark::Counter::kIsRate);
    state.counters["refused_%"] = benchmark::Counter(
        attempted == 0 ? 0.0
                       : 100.0 * static_cast<double>(refused) /
                             static_cast<double>(attempted));
}

BENCHMARK(push_pop_pair<Item<1>>)->Name("PushPopPair/8B");
BENCHMARK(push_pop_pair<Item<2>>)->Name("PushPopPair/16B");
BENCHMARK(push_pop_pair<Item<8>>)->Name("PushPopPair/64B");

BENCHMARK(fill_then_drain<Item<1>>)->Name("FillThenDrain/8B");
BENCHMARK(fill_then_drain<Item<2>>)->Name("FillThenDrain/16B");
BENCHMARK(fill_then_drain<Item<8>>)->Name("FillThenDrain/64B");

BENCHMARK(transfer<Item<1>, 1>)
    ->Name("Transfer/8B")
    ->Apply(for_each_placement)
    ->UseRealTime();
BENCHMARK(transfer<Item<2>, 1>)
    ->Name("Transfer/16B")
    ->Apply(for_each_placement)
    ->UseRealTime();
BENCHMARK(transfer<Item<8>, 1>)
    ->Name("Transfer/64B")
    ->Apply(for_each_placement)
    ->UseRealTime();
BENCHMARK(transfer<Item<2>, 8>)
    ->Name("Transfer/16B/batch of 8")
    ->Apply(for_each_placement)
    ->UseRealTime();

BENCHMARK(fan_in_spin)
    ->Name("FanInSpin/16B")
    ->Apply(for_each_producer_count)
    ->UseRealTime();
BENCHMARK(fan_in_drop)
    ->Name("FanInDrop/16B")
    ->Apply(for_each_producer_count)
    ->UseRealTime();

}  // close unnamed namespace
