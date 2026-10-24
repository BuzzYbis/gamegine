// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log.h                                                         -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_LOG_H
#define INCLUDED_ENGINE_CORE_LOG_H

//@PURPOSE: Provide the logger: wait-free on every thread that registered.
//
//@CLASSES:
//  engine::core::log::Logger: per-thread rings, one logger thread, one file
//  engine::core::log::Logger::Handle: the right of one thread to log
//
//@SEE_ALSO: core_log_record, core_log_format, core_spsc_ring, core_clock
//
//@DESCRIPTION: This component provides [Logger], which splits logging into a
// cheap half and an expensive half, on different threads:
// ```
//  any thread                          logger thread (every 1 ms)
//  ----------                          -------------
//  Handle::log_error(sev, error)       pop every ring
//    stamp Clock::now()                format each record (LogFormat)
//    push 16 bytes into ITS ring  -->  into a 64 KiB batch
//    ring full? count, and move on     write(2) the batch
// ```
// A thread that logs owns one [SpscRing]. The logging calls never lock,
// allocate, format or enter the kernel, and never wait for the logger thread:
// a full ring drops the record and counts it, and the logger thread reports
// the count ("render lost 1240 records") at its next pass. The logger thread
// may block, in [write(2)] or in [sleep_for], because nothing waits for it.
// Each pass takes at most a ring's worth from each ring, so a thread that
// logs faster than the logger thread formats cannot keep the others waiting,
// nor the destructor from finishing. After a pass the logger thread rests
// [k_DRAIN_INTERVAL], unless a ring gave a whole pass's worth: then it goes on
// at once, so a thread that floods is written as fast as the logger can format
// and write, and not at one ring a millisecond.
//
///Order
///-----
// The records of one thread come out in the order it logged them. The records
// of different threads come out grouped by thread within a pass, about a
// millisecond, and not sorted by time. Each line carries the time its record
// was made, so sort the lines by it to merge the threads.
//
///Registration
///------------
// A thread gets its ring by registering, and registering gives back a
// [Logger::Handle], the only way to log without waiting:
// ```
//  Result<Logger::Handle> handle = logger.register_current_thread("render");
//  ...
//  handle->log_error(Severity::e_WARN, error);
// ```
// Two mistakes cannot be written. Logging without having registered: there is
// no static call to make, and only [register_current_thread] makes a handle.
// Logging after the thread has stopped: a handle cannot be copied, and
// destroying it unregisters the thread, so once it is out of scope there is no
// name left to call it by. Two more the compiler cannot see, and a debug build
// asserts: a handle is used only by the thread that registered, because a ring
// has one producer, and every handle is destroyed before the [Logger] is. A
// handle can be passed down by reference, or kept in whatever each thread
// owns, such as a context handed to each job. Registration fails with
// [CoreReason::e_LOG_TOO_MANY_THREADS] when [k_MAX_THREADS] threads are
// registered at once.
//
///The Life of a Ring
///------------------
// A ring is free, live or closed, and each change has one agent:
// ```
//  free ---------> live ---------> closed ---------> free
//       register       ~Handle         logger thread
//       (any thread)   (its thread)    (drained and freed)
// ```
// The state is kept in what exists, not in a flag that could say the wrong
// thing. A free slot holds no producer. A producer holds its own phase, live
// or closed, so a slot cannot be closed and empty at once. A producer is made
// for each thread, so nothing of the previous one, its name or its counts, is
// there to reset or to forget. A slot goes from null to a producer only by the
// compare-exchange of [register_current_thread], which one thread wins, and
// back to null only when the logger thread frees it.
//
// A closed producer is not freed at once: the logger thread first takes its
// last records and reports its drops, so what a thread logged just before it
// finished is still written. It reads the phase before it pops, never after,
// so that a record pushed just before the close is not freed unread. The slot
// is then free again. [k_MAX_THREADS] therefore counts the threads registered
// at once, including those closed and not yet freed, which is one pass.
//
///Threads That Cannot Register
///----------------------------
// A driver or the operating system calls back on a thread of its own, which
// has no handle. [log_error_locked] and [log_message_locked] are called on the
// [Logger] itself, which a callback can reach through its user data, and push
// into one ring shared by all such threads, behind a mutex that makes them
// take turns. That is not wait-free, and not for a thread of ours: it is for
// those that have no other way in. Their records are named [k_SHARED_NAME].
//
///Text
///----
// The logger has no entry point for a string: a record is read up to a
// millisecond after it is made, and the text it would point to may be gone by
// then. An error or a message travels as a number, and the logger thread makes
// the text from it (see core_log_record).
//
///Shutdown
///--------
// The destructor stops the logger thread, joins it, and writes what is left in
// every ring from the thread that destroys the [Logger]. Destroy every handle
// and stop every foreign thread first: a debug build asserts on a live handle,
// and a call to [log_error_locked] during destruction is undefined. The file
// descriptor stays the caller's: the logger never closes it, and it must stay
// open until the [Logger] is gone. A [write(2)] that blocks, to a pipe nobody
// reads, blocks the logger thread and so the destructor; the rings fill and
// drop meanwhile, and no logging call waits.
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Logging from Two Threads
///- - - - - - - - - - - - - - - - - -
// Each thread registers on itself and logs through its own handle:
// ```
//  const std::int32_t fd = open_log_file();   // outlives the logger
//  {
//      Logger logger(fd);                     // starts the logger thread
//
//      std::thread render([&logger] {
//          Result<Logger::Handle> handle =
//              logger.register_current_thread("render");
//          if (handle) {
//              handle->log_error(Severity::e_WARN, error);
//          }
//      });                                    // the handle is destroyed here
//      render.join();
//  }                                          // the last records are written
//  ::close(fd);                               // only now
// ```
//
///Example 2: Logging from a Driver's Thread
///- - - - - - - - - - - - - - - - - - - - -
// A callback has no handle, but can reach the [Logger] through its user data:
// ```
//  void on_driver_message(void* user_data)
//  {
//      Logger& logger = *static_cast<Logger*>(user_data);
//      logger.log_message_locked(Severity::e_WARN, message);
//  }
// ```

// engine
#include "engine/core/core_clock.h"
#include "engine/core/core_error.h"
#include "engine/core/core_format.h"
#include "engine/core/core_log_format.h"
#include "engine/core/core_log_message.h"
#include "engine/core/core_log_record.h"
#include "engine/core/core_spsc_ring.h"

// std
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>
#include <thread>
#include <utility>

// POSIX
#include <unistd.h>

namespace engine::core {
namespace log {

                         // ============
                         // class Logger
                         // ============

/// This class moves log records from any number of registered threads to a
/// file descriptor, without ever making a registered thread wait. It owns a
/// logger thread, started by the constructor and stopped by the destructor.
/// A [Logger] holds its 64 KiB output batch, so it belongs in [main] or on the
/// heap, not on a small thread stack, and it cannot be copied.
class Logger final {
  public:
    // PUBLIC CLASS DATA

    /// Threads that can be registered at once, counting those closed but not
    /// yet freed by the logger thread.
    static constexpr std::size_t k_MAX_THREADS = 32;

    /// Records one thread can have in flight: 64 KiB of ring per thread. At a
    /// pass every millisecond or so, that is about 2 million lines a second
    /// from one thread without loss (measured), far above what an engine
    /// writes.
    static constexpr std::size_t k_RING_CAPACITY = 4096;

    /// Longest thread name kept; a longer one is cut.
    static constexpr std::size_t k_MAX_NAME_SIZE = 16;

    /// Size of the output batch handed to each [write(2)].
    static constexpr std::size_t k_BATCH_SIZE = std::size_t{64} * 1024;

    /// The batch is written out before a record when less than this is
    /// free, so a record is cut only if it is larger than this.
    static constexpr std::size_t k_FLUSH_THRESHOLD = std::size_t{8} * 1024;

    /// How long the logger thread rests between passes, unless a ring gave a
    /// whole pass's worth, when it goes on at once.
    static constexpr std::chrono::milliseconds k_DRAIN_INTERVAL{1};

    /// The name the records of [log_error_locked] and [log_message_locked]
    /// carry.
    static constexpr std::string_view k_SHARED_NAME = "shared";

    // A record is cut only if it is larger than the room a flush leaves, so
    // that room must exceed the longest line and the two bytes of the cut
    // mark; and the shared name is kept whole and shown whole.
    static_assert(k_FLUSH_THRESHOLD > LogFormat::k_MAX_LINE_SIZE + 2);
    static_assert(k_BATCH_SIZE >= 2 * k_FLUSH_THRESHOLD);
    static_assert(k_SHARED_NAME.size() <= k_MAX_NAME_SIZE);
    static_assert(k_SHARED_NAME.size() <= LogFormat::k_THREAD_WIDTH);

  private:
    // PRIVATE TYPES

    /// The ring one thread pushes its records into.
    using Ring = spsc::SpscRing<LogRecord, k_RING_CAPACITY>;

    /// Enumeration used to say whether the thread of a producer can still
    /// push. A producer is live from registration and closed once, when its
    /// [Handle] is destroyed; there is no way back.
    enum class Phase : std::uint8_t {
        e_LIVE = 0,
        e_CLOSED,
    };

    /// This struct is what one pass over a ring found: the phase it read, and
    /// whether it took a whole pass's worth of a live ring, which then may
    /// hold more.
    struct Taken {
        Phase d_phase;
        bool  d_full;
    };

    /// This class is everything one registered thread owns: a ring, its phase,
    /// what it has dropped and its name. Allocated at registration, on the
    /// heap, and freed by the logger thread once closed and drained; a new
    /// thread gets a new one, so nothing is carried over. Never moved, so a
    /// [Handle] can point to it. Its data is private, and each of the two
    /// sides touches it only through the functions made for it, which hold
    /// the memory orders it needs, so no caller chooses one.
    class Producer final {
      private:
        // DATA

        Ring                              d_ring;  // one pusher, one popper
        std::atomic<Phase>                d_phase{Phase::e_LIVE};
        std::atomic_uint64_t              d_dropped{0};    // pushing side
        std::uint64_t                     d_reported = 0;  // logger thread
        std::array<char, k_MAX_NAME_SIZE> d_name{};
        std::size_t                       d_name_size = 0;

      public:
        // CREATORS

        /// Create a [Producer] object labelled with the specified [name],
        /// cut to [k_MAX_NAME_SIZE], live, with an empty ring.
        explicit Producer(const std::string_view name) noexcept;

        // MANIPULATORS

        /// Push the specified [record], or count it as lost if the ring is
        /// full. One pusher at a time: a single thread, or a lock, keeps it
        /// so.
        void push(const LogRecord& record) noexcept;

        /// Close this producer: its thread will never push again. Release,
        /// so that every push before it is visible to whoever sees the phase.
        void close() noexcept;

        /// Return the oldest record in the ring and remove it, or nothing if
        /// the ring is empty.
        [[nodiscard]] std::optional<LogRecord> pop() noexcept;

        /// Return how many records were lost since the last call.
        [[nodiscard]] std::uint64_t take_dropped() noexcept;

        // ACCESSORS

        /// Return the phase of this producer. Acquire, so that every push
        /// before a close is visible to the caller that sees it closed.
        [[nodiscard]] Phase phase() const noexcept;

        /// Return the name of the thread this producer belongs to.
        [[nodiscard]] std::string_view name() const noexcept;
    };

  public:
    // PUBLIC TYPES

    /// This class is the right of one thread to log without waiting. Only
    /// [Logger::register_current_thread] makes one, it cannot be copied, and
    /// destroying it unregisters the thread: its producer is closed, and the
    /// logger thread writes what is left in it, then frees it. Use it only on
    /// the thread that registered.
    class Handle final {
        friend class Logger;

        // DATA

        Producer*       d_producer_p;  // null once moved from
        std::thread::id d_owner;       // the thread that registered

        // PRIVATE MANIPULATORS

        /// Push the specified [record] into this handle's ring, or count it
        /// as lost. The behavior is undefined unless the calling thread is
        /// the one that registered.
        void push(const LogRecord& record) noexcept;

        // PRIVATE CREATORS

        /// Create a [Handle] object for the calling thread, pushing into the
        /// ring of the specified [producer].
        explicit Handle(Producer& producer) noexcept;

      public:
        // CREATORS

        /// Create a [Handle] object having the value of the specified
        /// [original] object, which is left with no thread to log for. The
        /// new object still belongs to the thread that registered, wherever
        /// it is moved.
        Handle(Handle&& original) noexcept;

        /// Unregister the thread of this handle, if it has one: its producer
        /// is closed, and the logger thread writes what is left in it, then
        /// frees it. Records already pushed are still written.
        ~Handle();

        // MANIPULATORS

        /// Log the specified [error] with the specified [severity].
        /// Wait-free. The behavior is undefined unless called by the thread
        /// that registered.
        void log_error(const Severity     severity,
                       const error::Error error) noexcept;

        /// Log the specified [message] with the specified [severity].
        /// Wait-free. The behavior is undefined unless called by the thread
        /// that registered.
        void log_message(const Severity   severity,
                         const LogMessage message) noexcept;

        // NOT IMPLEMENTED

        Handle(const Handle&)            = delete;
        Handle& operator=(const Handle&) = delete;
        Handle& operator=(Handle&&)      = delete;
    };

  private:
    // DATA

    // One slot per thread: null when free, else the producer it owns. No
    // other flag says whether a slot is used.
    std::array<std::atomic<Producer*>, k_MAX_THREADS> d_slots{};
    std::unique_ptr<Producer>      d_shared_p;     // the locked callers' ring
    std::mutex                     d_shared_lock;  // one pusher at a time
    tic::TickConversion            d_clock;        // ticks to time since start
    std::int32_t                   d_fd;           // where the batch goes
    std::array<char, k_BATCH_SIZE> d_batch{};  // lines waiting for write(2)
    std::size_t                    d_batch_size = 0;  // bytes of it used

    // Last: constructed after everything it uses, joined before any of it is
    // destroyed. Owns the batch, [d_batch], while it runs.
    std::jthread d_consumer;

    // PRIVATE MANIPULATORS

    /// Push the specified [record] into the shared ring, or count it as
    /// lost.
    void push_locked(const LogRecord& record) noexcept;

    /// Run passes until the specified [stop] is requested.
    void consume(const std::stop_token& stop) noexcept;

    /// Pop every ring once, formatting everything found, report drops, and
    /// free the producers that are closed and empty. Return [true] if a live
    /// ring gave a whole pass's worth, so that it may hold more and the
    /// logger thread should not rest.
    bool drain() noexcept;

    /// Pop the ring of the specified [producer], formatting what is found,
    /// and report what it has dropped since the last time. Read the phase
    /// first, and take at most a ring's worth if it is live, everything if it
    /// is closed, since nothing refills it. Return the phase it read, closed
    /// meaning the ring is now empty and the producer can be freed, and
    /// whether it took a whole pass's worth of a live ring.
    Taken drain_one(Producer& producer) noexcept;

    /// Append the specified [record] of the specified [thread] to the batch.
    void write_record(const LogRecord&       record,
                      const std::string_view thread) noexcept;

    /// Append a line from the logger itself, saying the specified [count]
    /// records of the specified [thread] were lost, to the batch.
    void write_loss(const std::string_view thread,
                    const std::uint64_t    count) noexcept;

    /// Write the batch to the file descriptor, and empty it. A batch that
    /// cannot be written is lost: nothing is left to report a failing log to.
    void flush() noexcept;

    // NOT IMPLEMENTED

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

  public:
    // CREATORS

    /// Create a [Logger] object writing to the specified [fd], which must
    /// stay open until it is destroyed, and start its logger thread.
    explicit Logger(const std::int32_t fd) noexcept;

    /// Stop the logger thread, write every record still in a ring, and
    /// destroy this object. A debug build asserts that every [Handle] has
    /// been destroyed; a release build leaks the producer of one that was
    /// not, whose thread then logs into a ring nobody reads. The behavior is
    /// undefined if a thread is inside [log_error_locked] or
    /// [log_message_locked].
    ~Logger();

    // MANIPULATORS

    /// Return a [Handle] for the calling thread, labelled with the specified
    /// [name] (cut to [k_MAX_NAME_SIZE]), with which it can log. Fail with
    /// [CoreReason::e_LOG_TOO_MANY_THREADS] if [k_MAX_THREADS] threads are
    /// registered already; one is free again once its handle is destroyed and
    /// the logger thread has written what it logged.
    [[nodiscard]] Result<Handle>
    register_current_thread(const std::string_view name) noexcept;

    /// Log the specified [error] with the specified [severity] from a thread
    /// that has no [Handle]. Takes a lock, so not wait-free: for the threads
    /// of a driver or of the operating system, not for ours.
    void log_error_locked(const Severity     severity,
                          const error::Error error) noexcept;

    /// Log the specified [message] with the specified [severity] from a
    /// thread that has no [Handle]. Takes a lock, so not wait-free: for the
    /// threads of a driver or of the operating system, not for ours.
    void log_message_locked(const Severity   severity,
                            const LogMessage message) noexcept;
};

// ============================================================================
//                          INLINE DEFINITIONS
// ============================================================================

                         // ----------------------
                         // class Logger::Producer
                         // ----------------------

// CREATORS

inline Logger::Producer::Producer(const std::string_view name) noexcept
{
    d_name_size = std::min(name.size(), k_MAX_NAME_SIZE);
    if (d_name_size != 0) {
        std::memcpy(d_name.data(), name.data(), d_name_size);
    }
}

// MANIPULATORS

inline void Logger::Producer::push(const LogRecord& record) noexcept
{
    if (!d_ring.try_push(record)) {
        d_dropped.store(d_dropped.load(std::memory_order_relaxed) + 1,
                        std::memory_order_relaxed);
    }
}

inline void Logger::Producer::close() noexcept
{
    d_phase.store(Phase::e_CLOSED, std::memory_order_release);
}

inline std::optional<LogRecord> Logger::Producer::pop() noexcept
{
    return d_ring.try_pop();
}

inline std::uint64_t Logger::Producer::take_dropped() noexcept
{
    const std::uint64_t dropped = d_dropped.load(std::memory_order_relaxed);
    const std::uint64_t news    = dropped - d_reported;
    d_reported                  = dropped;
    return news;
}

// ACCESSORS

inline Logger::Phase Logger::Producer::phase() const noexcept
{
    return d_phase.load(std::memory_order_acquire);
}

inline std::string_view Logger::Producer::name() const noexcept
{
    return std::string_view(d_name.data(), d_name_size);
}

                         // --------------------
                         // class Logger::Handle
                         // --------------------

// PRIVATE MANIPULATORS

inline void Logger::Handle::push(const LogRecord& record) noexcept
{
    assert(d_producer_p != nullptr);
    assert(d_owner == std::this_thread::get_id());

    d_producer_p->push(record);
}

// PRIVATE CREATORS

inline Logger::Handle::Handle(Producer& producer) noexcept
: d_producer_p(&producer)
, d_owner(std::this_thread::get_id())
{
}

// CREATORS

inline Logger::Handle::Handle(Handle&& original) noexcept
: d_producer_p(std::exchange(original.d_producer_p, nullptr))
, d_owner(original.d_owner)
{
}

inline Logger::Handle::~Handle()
{
    if (d_producer_p != nullptr) {
        d_producer_p->close();
    }
}

// MANIPULATORS

inline void Logger::Handle::log_error(const Severity     severity,
                                      const error::Error error) noexcept
{
    push(LogRecord::make(severity, tic::Clock::now(), error));
}

inline void Logger::Handle::log_message(const Severity   severity,
                                        const LogMessage message) noexcept
{
    push(LogRecord::make(severity, tic::Clock::now(), message));
}

                         // ------------
                         // class Logger
                         // ------------

// PRIVATE MANIPULATORS

inline void Logger::push_locked(const LogRecord& record) noexcept
{
    const std::lock_guard lock(d_shared_lock);
    d_shared_p->push(record);
}

inline void Logger::consume(const std::stop_token& stop) noexcept
{
    while (!stop.stop_requested()) {
        const bool behind = drain();
        flush();
        if (!behind) {
            std::this_thread::sleep_for(k_DRAIN_INTERVAL);
        }
    }
}

inline bool Logger::drain() noexcept
{
    bool behind = false;
    for (std::atomic<Producer*>& slot : d_slots) {
        Producer* const producer = slot.load(std::memory_order_acquire);
        if (producer == nullptr) {
            continue;
        }

        const Taken taken = drain_one(*producer);
        behind            = behind || taken.d_full;
        if (taken.d_phase == Phase::e_CLOSED) {
            std::unique_ptr<Producer> owner(producer);
            owner.reset();
            slot.store(nullptr, std::memory_order_release);
        }
    }
    return drain_one(*d_shared_p).d_full || behind;
}

inline Logger::Taken Logger::drain_one(Producer& producer) noexcept
{
    const Phase            phase = producer.phase();
    const std::string_view name  = producer.name();

    const std::size_t limit = (phase == Phase::e_CLOSED)
                                  ? std::numeric_limits<std::size_t>::max()
                                  : k_RING_CAPACITY;
    std::size_t       count = 0;
    for (; count < limit; ++count) {
        const auto record = producer.pop();
        if (!record) {
            break;
        }
        write_record(*record, name);
    }

    const std::uint64_t dropped = producer.take_dropped();
    if (dropped != 0) {
        write_loss(name, dropped);
    }

    return Taken{phase, count == limit};
}

inline void Logger::write_record(const LogRecord&       record,
                                 const std::string_view thread) noexcept
{
    if (d_batch.size() - d_batch_size < k_FLUSH_THRESHOLD) {
        flush();
    }

    const std::span<char> free = std::span(d_batch).subspan(d_batch_size);
    format::LineWriter    writer(free.first(free.size() - 2));
    LogFormat::append_record(writer, record, thread, d_clock);

    std::size_t size = writer.size();
    if (writer.is_truncated()) {
        free[size++] = '~';
        free[size++] = '\n';
    }

    d_batch_size += size;
}

inline void Logger::write_loss(const std::string_view thread,
                               const std::uint64_t    count) noexcept
{
    if (d_batch.size() - d_batch_size < k_FLUSH_THRESHOLD) {
        flush();
    }

    const std::span<char> free = std::span(d_batch).subspan(d_batch_size);
    format::LineWriter    writer(free);
    LogFormat::append_prefix(
        writer, tic::Clock::now(), Severity::e_WARN, "logger", d_clock);

    writer.append_char(' ');
    writer.append(thread);
    writer.append(" lost ");
    writer.append_decimal(count);
    writer.append(" records\n");
    d_batch_size += writer.size();
}

inline void Logger::flush() noexcept
{
    std::size_t written = 0;
    while (written < d_batch_size) {
        const ssize_t result = ::write(d_fd,
                                       d_batch.data() + written,
                                       d_batch_size - written);


        if (result > 0) {
            written += static_cast<std::size_t>(result);
        } else if (result < 0 && errno == EINTR) {
            continue;
        } else {
            // Nothing is left to report a failing log to: the batch is
            // lost.
            break;
        }
    }

    d_batch_size = 0;
}

// CREATORS

inline Logger::Logger(const std::int32_t fd) noexcept
: d_shared_p(std::make_unique<Producer>(k_SHARED_NAME))
, d_clock(tic::TickConversion::start_now())
, d_fd(fd)
, d_consumer([this](const std::stop_token& stop) { consume(stop); })
{
}

inline Logger::~Logger()
{
    d_consumer.request_stop();
    if (d_consumer.joinable()) {
        d_consumer.join();
    }

    drain();
    flush();

    assert(std::ranges::all_of(d_slots, [](const auto& slot) {
        return slot.load(std::memory_order_relaxed) == nullptr;
    }));
}

// MANIPULATORS

inline Result<Logger::Handle>
Logger::register_current_thread(const std::string_view name) noexcept
{
    auto fresh = std::make_unique<Producer>(name);

    for (std::atomic<Producer*>& slot : d_slots) {
        Producer* expected = nullptr;
        if (slot.compare_exchange_strong(expected,
                                         fresh.get(),
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {
            return Handle(*fresh.release());
        }
    }

    return error::make_unexpected(
        error::Error::make(error::Error::Kind::e_EXHAUSTED,
                           error::Error::CoreReason::e_LOG_TOO_MANY_THREADS,
                           static_cast<std::uint32_t>(k_MAX_THREADS)));
}

inline void Logger::log_error_locked(const Severity     severity,
                                     const error::Error error) noexcept
{
    push_locked(LogRecord::make(severity, tic::Clock::now(), error));
}

inline void Logger::log_message_locked(const Severity   severity,
                                       const LogMessage message) noexcept
{
    push_locked(LogRecord::make(severity, tic::Clock::now(), message));
}

}  // close namespace log
}  // close namespace engine::core

#endif
