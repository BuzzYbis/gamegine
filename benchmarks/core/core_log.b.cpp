// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log.b.cpp                                                     -*-C++-*-

//@PURPOSE: Measure what the whole 'Logger' writes per second, in lines and in
// bytes, and what a log call costs the thread that makes it.
//
//@DESCRIPTION: The logger is a cheap half on the thread that logs, and an
// expensive half on the logger thread, which pops the rings, formats each
// record and writes the batch. Three things are measured, on the real logger
// writing to a real file:
//
//: o 'LogCall': the cost of one call to 'Handle::log_error' or
//:   'Handle::log_message' on the logging thread, while the logger thread
//:   empties the ring behind it. A call stamps the clock, makes a 16-byte
//:   record and pushes it; it never formats, locks or enters the kernel.
//:   'ns_per_call' is the figure. The logger writes to '/dev/null'.
//:
//: o 'Sustained': 1 or 3 threads log as fast as they can for a quarter of a
//:   second, three errors to one message, so that the ring is always full and
//:   the logger thread is the slow side. What it writes in that time is the
//:   most the logger sustains: 'items_per_second' is the lines written, and
//:   'bytes_per_second' the bytes of them. The threads push far faster than
//:   that, so most of what they log is dropped, and 'dropped_%' says how
//:   much. A dropped record is counted and reported by the logger, never lost
//:   silently, and the benchmark checks it: the lines in the file plus the
//:   lines it reports lost must be exactly the lines logged, or the run is
//:   skipped with an error.
//:
//: o 'Offered': one thread logs at a chosen rate, in bursts of 64, for a
//:   quarter of a second. Below what the logger sustains nothing is dropped
//:   and the lines written are the lines offered; past it, 'dropped_%' shows
//:   where it starts. A ring is full when the logger thread has not come back
//:   for as long as the ring takes to fill, so what is found is a rate that
//:   depends on the ring's capacity and on how long the logger thread rests
//:   between passes, as much as on how fast it formats. 'logger_cpu_%' is the
//:   share of a core the logger thread used at that rate.
//
// Each run of 'Sustained' and 'Offered' creates a 'Logger' on a file of its
// own, and reads the file back after the logger is destroyed, outside the
// time measured: that is when everything the logger owes has been written.
// The time measured is from the first log call to the end of the destructor,
// the last pass included. Build with the release flags ('-O3 -DNDEBUG') and
// run with repetitions, for a median:
// ```
//  xmake f -m release && xmake build benchmarks
//  xmake run benchmarks --benchmark_filter=Log --benchmark_repetitions=5
// ```
// ('--benchmark_report_aggregates_only=true' keeps just the median and the
// mean.) The rates are of the machine they ran on, to a file in its page
// cache; a terminal or a pipe nobody reads is slower, and the logging threads
// do not wait for it. The 'Sustained' threads spin, and the logger thread and
// the file system need cores of their own: the figures for 3 threads on a
// machine with 4 fast cores are what is left to the logger thread, not what
// it could do alone. On macOS every thread asks for a fast core ('asked P'),
// which only biases the choice, and the OS does not say where a thread ran.
// TODO(Profile L): run on the Linux machine, with the threads pinned.

#include <engine/core/core_log.h>

#include <engine/core/core_error.h>
#include <engine/core/core_log_message.h>
#include <engine/core/core_log_record.h>

#include <benchmark/benchmark.h>

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef __APPLE__
#include <pthread.h>
#include <pthread/qos.h>
#endif

namespace {

using engine::core::Result;
using engine::core::error::Error;
using engine::core::log::Logger;
using engine::core::log::LogMessage;
using engine::core::log::Severity;

using Clock = std::chrono::steady_clock;

/// How long 'Sustained' and 'Offered' log for.
constexpr std::chrono::milliseconds k_RUN{250};

/// Calls of one timed batch of 'LogCall'.
constexpr std::uint32_t k_BATCH = 256;

/// Timed batches between two rests of 'LogCall': fewer calls than a ring
/// holds, so that the ring never fills while one is timed.
constexpr std::uint32_t k_BATCHES = 8;

/// Lines of one burst of 'Offered'.
constexpr std::uint32_t k_BURST = 64;

/// The line of the loss report names this thread, in its column.
constexpr std::string_view k_LOGGER_THREAD = "logger";

/// Ask the OS to run the calling thread on a fast core, as far as the platform
/// lets a thread say so: a quality of service on macOS, nothing elsewhere. A
/// thread that creates another gives it the same, so the logger thread runs on
/// a fast core if the thread that makes the 'Logger' does. Without it, the
/// same code runs 2 to 3 times slower when it lands on an efficiency core,
/// and the figures do not repeat.
void prefer_fast_cores()
{
#ifdef __APPLE__
    (void)pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

// ------------------------------------------------------------- the file -----

/// A file the logger writes to, appended to, read back and emptied between
/// runs, and removed with this object.
class SinkFile {
  private:
    std::string  d_path;
    std::int32_t d_fd = -1;

  public:
    SinkFile()
    {
        std::string       pattern = (std::filesystem::temp_directory_path() /
                                     "gamegine_core_log_bench_XXXXXX")
                                        .string();
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        d_fd   = ::mkstemp(buffer.data());
        d_path = buffer.data();
        if (d_fd >= 0) {
            (void)::fcntl(d_fd, F_SETFL, O_APPEND);
        }
    }

    ~SinkFile()
    {
        if (d_fd >= 0) {
            ::close(d_fd);
        }
        ::unlink(d_path.c_str());
    }

    SinkFile(const SinkFile&)            = delete;
    SinkFile& operator=(const SinkFile&) = delete;

    /// The descriptor to give a 'Logger'.
    [[nodiscard]] std::int32_t fd() const
    {
        return d_fd;
    }

    /// The size of the file, in bytes.
    [[nodiscard]] std::uint64_t size() const
    {
        struct stat info{};
        return ::fstat(d_fd, &info) == 0
                   ? static_cast<std::uint64_t>(info.st_size)
                   : 0U;
    }

    /// Empty the file, so that runs do not pile up on the disk, and say
    /// whether it worked.
    [[nodiscard]] bool empty() const
    {
        return ::ftruncate(d_fd, 0) == 0;
    }

    [[nodiscard]] const std::string& path() const
    {
        return d_path;
    }
};

/// What a log file holds.
struct Tally {
    std::uint64_t d_lines     = 0;  // record lines
    std::uint64_t d_lost      = 0;  // records the logger reports lost
    std::uint64_t d_malformed = 0;  // lines that are not whole
    std::uint64_t d_bytes     = 0;  // size of the file
};

/// The prefix is fixed-width while the seconds are below 10000: 28 characters,
/// a space, and the message. The thread is the eight characters from 20.
constexpr std::size_t k_THREAD_AT   = 20;
constexpr std::size_t k_THREAD_SIZE = 8;
constexpr std::size_t k_MESSAGE_AT  = 29;

/// Read 'file' and count its lines. A line is a record, or a report that the
/// logger lost some; for a report, '<name> lost <count> records', the count
/// is added to 'lost'.
Tally read_back(const SinkFile& file)
{
    Tally tally;
    tally.d_bytes = file.size();

    std::ifstream in(file.path());
    for (std::string line; std::getline(in, line);) {
        if (line.size() < k_MESSAGE_AT || line[0] != '[' || line[28] != ' ') {
            ++tally.d_malformed;
            continue;
        }
        std::string thread = line.substr(k_THREAD_AT, k_THREAD_SIZE);
        while (!thread.empty() && thread.back() == ' ') {
            thread.pop_back();
        }
        if (thread == k_LOGGER_THREAD) {
            std::istringstream report(line.substr(k_MESSAGE_AT));
            std::string        name;
            std::string        word;
            std::uint64_t      count = 0;
            report >> name >> word >> count;
            tally.d_lost += count;
        } else {
            ++tally.d_lines;
        }
    }
    return tally;
}

// ---------------------------------------------------------- the logging -----

/// Log record number 'i' through 'handle': three errors to one message.
inline void log_one(Logger::Handle& handle, const std::uint32_t i)
{
    if ((i & 3U) == 3U) {
        handle.log_message(
            Severity::e_INFO,
            LogMessage::make(LogMessage::CoreMessage::e_UNKNOWN));
    } else {
        handle.log_error(Severity::e_WARN,
                         Error::make(Error::Kind::e_UNKNOWN,
                                     Error::CoreReason::e_UNKNOWN,
                                     i));
    }
}

/// A gate that holds threads until every one is ready, then lets them go
/// together.
class Gate {
  private:
    std::atomic<std::uint32_t> d_ready{0};
    std::atomic<bool>          d_go{false};

  public:
    /// Say that the calling thread is ready, and wait for the go.
    void arrive_and_wait()
    {
        d_ready.fetch_add(1);
        while (!d_go.load(std::memory_order_acquire)) {
        }
    }

    /// Wait until 'threads' threads have arrived.
    void wait_for(const std::uint32_t threads) const
    {
        while (d_ready.load() != threads) {
            std::this_thread::yield();
        }
    }

    void go()
    {
        d_go.store(true, std::memory_order_release);
    }
};

/// The CPU time the calling thread has used, in seconds.
double thread_cpu_seconds()
{
    timespec now{};
    (void)::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    return static_cast<double>(now.tv_sec) +
           (static_cast<double>(now.tv_nsec) * 1e-9);
}

/// The CPU time the whole process has used, in seconds, user and system.
double process_cpu_seconds()
{
    rusage usage{};
    (void)::getrusage(RUSAGE_SELF, &usage);
    const auto seconds = [](const timeval& t) {
        return static_cast<double>(t.tv_sec) +
               (static_cast<double>(t.tv_usec) * 1e-6);
    };
    return seconds(usage.ru_utime) + seconds(usage.ru_stime);
}

/// Seconds between two instants.
double seconds_between(const Clock::time_point from,
                       const Clock::time_point to)
{
    return std::chrono::duration<double>(to - from).count();
}

/// What the runs of one benchmark added up to.
struct Totals {
    std::uint64_t d_pushed     = 0;    // records logged
    std::uint64_t d_lines      = 0;    // record lines in the file
    std::uint64_t d_bytes      = 0;    // bytes of the file
    std::uint64_t d_lost       = 0;    // records the logger reports lost
    double        d_seconds    = 0.0;  // time measured
    double        d_logger_cpu = 0.0;  // CPU the logger thread used
};

/// Add a finished run to 'totals': the 'pushed' records the threads logged
/// in 'elapsed' seconds, with 'logger_cpu' seconds of CPU used by the logger
/// thread. Return false, with 'state' skipped, if the file does not hold
/// exactly what was logged: the lines in it plus the lines the logger reports
/// lost must be the lines 'pushed', and every line must be whole.
bool account(benchmark::State&   state,
             const SinkFile&     file,
             const std::uint64_t pushed,
             const double        elapsed,
             const double        logger_cpu,
             Totals&             totals)
{
    const Tally tally = read_back(file);
    (void)file.empty();  // only the disk use depends on it
    if (tally.d_malformed != 0) {
        state.SkipWithError("the file holds a line that is not whole");
        return false;
    }
    if (tally.d_lines + tally.d_lost != pushed) {
        state.SkipWithError("lines written plus lines reported lost is not "
                            "the lines logged");
        return false;
    }
    totals.d_pushed += pushed;
    totals.d_lines += tally.d_lines;
    totals.d_bytes += tally.d_bytes;
    totals.d_lost += tally.d_lost;
    totals.d_seconds += elapsed;
    totals.d_logger_cpu += logger_cpu;
    state.SetIterationTime(elapsed);
    return true;
}

/// Set the counters of 'state' from 'totals'.
void report(benchmark::State& state, const Totals& totals)
{
    const auto share = [](const double part, const double whole) {
        return whole > 0 ? part / whole : 0.0;
    };
    state.SetItemsProcessed(static_cast<std::int64_t>(totals.d_lines));
    state.SetBytesProcessed(static_cast<std::int64_t>(totals.d_bytes));
    state.counters["logged_per_s"] = benchmark::Counter(
        share(static_cast<double>(totals.d_pushed), totals.d_seconds));
    state.counters["dropped_%"] = benchmark::Counter(
        100.0 * share(static_cast<double>(totals.d_lost),
                      static_cast<double>(totals.d_pushed)));
    state.counters["bytes_per_line"] = benchmark::Counter(
        share(static_cast<double>(totals.d_bytes),
              static_cast<double>(totals.d_lines)));
    state.counters["logger_cpu_%"] = benchmark::Counter(
        100.0 * share(totals.d_logger_cpu, totals.d_seconds));
}

// ------------------------------------------------------------ LogCall -----

template <bool t_ERROR> void log_call(benchmark::State& state)
{
    prefer_fast_cores();
    const std::int32_t null_fd = ::open("/dev/null", O_WRONLY);
    double             seconds = 0.0;
    std::uint32_t      next    = 0;
    {
        Logger                 logger(null_fd);
        Result<Logger::Handle> handle = logger.register_current_thread(
            "bench");
        if (!handle) {
            state.SkipWithError("the thread could not register");
            ::close(null_fd);
            return;
        }

        for ([[maybe_unused]] const auto _ : state) {
            double elapsed = 0.0;
            for (std::uint32_t batch = 0; batch < k_BATCHES; ++batch) {
                const auto start = Clock::now();
                for (std::uint32_t i = 0; i < k_BATCH; ++i, ++next) {
                    if constexpr (t_ERROR) {
                        handle->log_error(
                            Severity::e_WARN,
                            Error::make(Error::Kind::e_UNKNOWN,
                                        Error::CoreReason::e_UNKNOWN,
                                        next));
                    } else {
                        handle->log_message(
                            Severity::e_INFO,
                            LogMessage::make(
                                LogMessage::CoreMessage::e_UNKNOWN));
                    }
                }
                elapsed += seconds_between(start, Clock::now());
            }
            seconds += elapsed;
            state.SetIterationTime(elapsed);
            // Let the logger thread empty the ring before the next calls.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }  // the handle, then the logger, which writes what is left

    ::close(null_fd);
    const std::uint64_t calls = static_cast<std::uint64_t>(
                                    state.iterations()) *
                                std::uint64_t{k_BATCH} * k_BATCHES;
    state.SetItemsProcessed(static_cast<std::int64_t>(calls));
    state.counters["ns_per_call"] = benchmark::Counter(
        calls != 0 ? seconds * 1e9 / static_cast<double>(calls) : 0.0);
}

// ---------------------------------------------------------- Sustained -----

void sustained(benchmark::State& state)
{
    prefer_fast_cores();
    const auto     threads = static_cast<std::uint32_t>(state.range(0));
    const SinkFile file;
    if (file.fd() < 0) {
        state.SkipWithError("could not make the output file");
        return;
    }

    Totals totals;
    for ([[maybe_unused]] const auto _ : state) {
        std::vector<std::uint64_t> counts(threads, 0);
        std::vector<double>        cpu(threads, 0.0);
        Clock::time_point          start;
        double                     process_before = 0.0;
        {
            Logger                   logger(file.fd());
            Gate                     gate;
            std::atomic<bool>        stop{false};
            std::vector<std::thread> workers;
            workers.reserve(threads);
            for (std::uint32_t t = 0; t < threads; ++t) {
                workers.emplace_back([&, t] {
                    prefer_fast_cores();
                    Result<Logger::Handle> handle =
                        logger.register_current_thread("worker");
                    gate.arrive_and_wait();
                    std::uint32_t i = 0;
                    while (!stop.load(std::memory_order_relaxed)) {
                        for (std::uint32_t n = 0; n < 256; ++n, ++i) {
                            log_one(*handle, i);
                        }
                    }
                    counts[t] = i;
                    cpu[t]    = thread_cpu_seconds();
                });
            }
            gate.wait_for(threads);
            process_before = process_cpu_seconds();
            start          = Clock::now();
            gate.go();
            std::this_thread::sleep_for(k_RUN);
            stop.store(true);
            for (std::thread& worker : workers) {
                worker.join();
            }
        }  // the destructor writes what is left in the rings
        const double  elapsed = seconds_between(start, Clock::now());
        double        process = process_cpu_seconds() - process_before;
        std::uint64_t total   = 0;
        for (std::uint32_t t = 0; t < threads; ++t) {
            total += counts[t];
            process -= cpu[t];
        }
        // What the workers did not use is the logger thread's, and the idle
        // benchmark thread's, which is nothing next to it.
        if (!account(state, file, total, elapsed, process, totals)) {
            return;
        }
    }
    report(state, totals);
}

// ------------------------------------------------------------ Offered -----

void offered(benchmark::State& state)
{
    prefer_fast_cores();
    const double   rate = static_cast<double>(state.range(0)) * 1'000.0;
    const SinkFile file;
    if (file.fd() < 0) {
        state.SkipWithError("could not make the output file");
        return;
    }

    Totals totals;
    for ([[maybe_unused]] const auto _ : state) {
        std::uint64_t     total          = 0;
        double            worker         = 0.0;
        double            process_before = 0.0;
        Clock::time_point start;
        {
            Logger            logger(file.fd());
            Gate              gate;
            std::atomic<bool> stop{false};
            std::thread       thread([&] {
                prefer_fast_cores();
                Result<Logger::Handle> handle = logger.register_current_thread(
                    "worker");
                gate.arrive_and_wait();
                const auto    begin = Clock::now();
                std::uint32_t i     = 0;
                while (!stop.load(std::memory_order_relaxed)) {
                    for (std::uint32_t n = 0; n < k_BURST; ++n, ++i) {
                        log_one(*handle, i);
                    }
                    // Not before this burst is due: the rate, not the speed.
                    const auto due =
                        begin +
                        std::chrono::nanoseconds(static_cast<std::int64_t>(
                            static_cast<double>(i) / rate * 1e9));
                    while (Clock::now() < due &&
                           !stop.load(std::memory_order_relaxed)) {
                    }
                }
                total  = i;
                worker = thread_cpu_seconds();
            });
            gate.wait_for(1);
            process_before = process_cpu_seconds();
            start          = Clock::now();
            gate.go();
            std::this_thread::sleep_for(k_RUN);
            stop.store(true);
            thread.join();
        }  // the destructor writes what is left in the ring
        const double elapsed    = seconds_between(start, Clock::now());
        const double logger_cpu = process_cpu_seconds() - process_before -
                                  worker;
        if (!account(state, file, total, elapsed, logger_cpu, totals)) {
            return;
        }
    }
    report(state, totals);
}

BENCHMARK(log_call<true>)
    ->Name("LogCall/error")
    ->UseManualTime()
    ->Iterations(2'000);
BENCHMARK(log_call<false>)
    ->Name("LogCall/message")
    ->UseManualTime()
    ->Iterations(2'000);

BENCHMARK(sustained)
    ->Name("Sustained")
    ->ArgName("threads")
    ->Arg(1)
    ->Arg(3)
    ->UseManualTime()
    ->Iterations(3);

BENCHMARK(offered)
    ->Name("Offered")
    ->ArgName("klines_per_s")
    ->Arg(100)
    ->Arg(1'000)
    ->Arg(2'000)
    ->Arg(3'000)
    ->Arg(4'000)
    ->Arg(6'000)
    ->Arg(8'000)
    ->UseManualTime()
    ->Iterations(3);

}  // close unnamed namespace
