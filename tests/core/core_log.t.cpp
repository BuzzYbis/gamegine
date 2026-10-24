// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_log.t.cpp                                                     -*-C++-*-

//@PURPOSE: Verify that 'Logger' writes what its threads log, whole and in
// order, and that nothing it does makes a logging thread wait.
//
//@DESCRIPTION: A 'Logger' is threads and a file, so every test runs real
// threads against a real file, destroys the logger, and then reads the file:
// that is when everything the logger owes has been written. Six properties
// have to hold:
//
//: o Nothing is lost that has room. What a thread logs comes out as lines,
//:   each whole, in the order the thread logged them, with the name of the
//:   thread and the severity it was given, for errors and for messages, and
//:   after a flush of more than one batch. A record is checked by the number
//:   it carries, so a lost, repeated or reordered one is seen as such.
//:
//: o Threads are independent. Two threads, thirty-two, and threads that do
//:   not register at all (the locked calls) each get exactly their records
//:   out, and two loggers at once do not hear each other.
//:
//: o A thread's place is given back. Thousands of threads that come and go
//:   share the thirty-two slots, and a thread that has finished still has
//:   every record written, because the logger empties its ring before it
//:   frees it. A thirty-third at once is refused with its own error, and a
//:   slot comes back when a handle is destroyed.
//:
//: o A flood loses records, never the log. A thread that logs faster than
//:   the logger writes drops what does not fit, and the loss is reported:
//:   what was written plus what was reported lost is exactly what was logged.
//:
//: o A file that cannot be written does not stop the program: the log is
//:   lost, and the logger is still destroyed.
//:
//: o The numbers the code relies on hold: a ring is a power of two, a
//:   record always fits where the batch makes room, the shared name fits its
//:   column.
//
// The two things a handle must not do -- be used by a thread other than the
// one that registered, and outlive the logger -- are asserted in a debug
// build; the death tests are skipped, and reported as skipped, where 'assert'
// is compiled out. That a handle cannot be copied, made by hand or called on
// the class is a compile-time rule, checked by the snippets in
// 'ci/test/compile_fail/'.

#include <engine/core/core_log.h>

#include <engine/core/core_error.h>
#include <engine/core/core_error_format.h>
#include <engine/core/core_format.h>
#include <engine/core/core_log_format.h>
#include <engine/core/core_log_message.h>
#include <engine/core/core_log_record.h>

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifdef __APPLE__
#include <pthread.h>
#include <pthread/qos.h>
#endif

namespace {

using engine::core::Result;
using engine::core::error::Error;
using engine::core::format::LineWriter;
using engine::core::log::LogFormat;
using engine::core::log::Logger;
using engine::core::log::LogMessage;
using engine::core::log::LogRecord;
using engine::core::log::Severity;
using engine::core::tic::TickConversion;

using namespace std::chrono_literals;

// ------------------------------------------------------------- helpers -----

/// A file the logger writes to, removed when this object goes.
class LogFile {
  private:
    std::string  d_path;
    std::int32_t d_fd = -1;

  public:
    LogFile()
    {
        std::string       pattern = (std::filesystem::temp_directory_path() /
                                     "gamegine_core_log_XXXXXX")
                                        .string();
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        d_fd   = ::mkstemp(buffer.data());
        d_path = buffer.data();
    }

    ~LogFile()
    {
        if (d_fd >= 0) {
            ::close(d_fd);
        }
        ::unlink(d_path.c_str());
    }

    LogFile(const LogFile&)            = delete;
    LogFile& operator=(const LogFile&) = delete;

    /// The descriptor to give a 'Logger'.
    std::int32_t fd() const
    {
        return d_fd;
    }

    /// Where the file is.
    const std::string& path() const
    {
        return d_path;
    }

    /// The lines of the file, in order. A last line with no newline is kept,
    /// so that a record cut short is seen as one.
    std::vector<std::string> lines() const
    {
        std::ifstream            in(d_path);
        std::vector<std::string> result;
        for (std::string line; std::getline(in, line);) {
            result.push_back(line);
        }
        return result;
    }
};

/// The record lines of the file at 'path': every line but the logger's own
/// reports of what it lost, which name the thread 'logger' in its column. Read
/// in chunks, so that a file of tens of megabytes is not held.
std::uint64_t count_record_lines(const std::string& path)
{
    constexpr std::size_t k_THREAD_AT   = 20;
    constexpr std::size_t k_THREAD_SIZE = 8;
    constexpr std::size_t k_LINE_MIN    = 28;

    std::ifstream     in(path, std::ios::binary);
    std::vector<char> chunk(std::size_t{1} << 20U);
    std::string       carry;
    std::uint64_t     lines = 0;
    while (in) {
        in.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        carry.append(chunk.data(), static_cast<std::size_t>(in.gcount()));
        std::size_t start = 0;
        for (std::size_t end = carry.find('\n', start);
             end != std::string::npos;
             start = end + 1, end = carry.find('\n', start)) {
            const bool report = end - start >= k_LINE_MIN &&
                                std::string_view(carry).substr(
                                    start + k_THREAD_AT, k_THREAD_SIZE) ==
                                    "logger  ";
            lines += report ? 0U : 1U;
        }
        carry.erase(0, start);
    }
    return lines;
}

/// Ask the OS to run the calling thread on a fast core, as far as the
/// platform lets a thread say so: a quality of service on macOS, nothing
/// elsewhere. A thread that creates another gives it the same, so the logger
/// thread follows. A test that compares a rate with what the machine can do
/// needs it: on an efficiency core the same code runs two to three times
/// slower, and the comparison means nothing.
void prefer_fast_cores()
{
#ifdef __APPLE__
    (void)pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

/// The CPU time this process has used, in seconds, user and system.
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

/// How many record lines a second this machine formats, in memory, one at a
/// time into a buffer: what the logger thread can do at best, with no file.
/// The best of several short runs, after a warm-up: the first milliseconds on
/// a thread may be on a slow core, and one run would read that.
double lines_formatted_per_second()
{
    constexpr std::uint32_t k_LINES = 100'000;
    constexpr std::uint32_t k_RUNS  = 8;

    const TickConversion clock(0, 1, 1);
    const LogRecord      record = LogRecord::make(
        Severity::e_WARN,
        1'000,
        Error::make(Error::Kind::e_UNKNOWN, Error::CoreReason::e_UNKNOWN, 1));
    std::array<char, 1024> buffer{};
    volatile std::uint8_t  sink = 0;

    double best = 0.0;
    for (std::uint32_t run = 0; run < k_RUNS; ++run) {
        const auto start = std::chrono::steady_clock::now();
        for (std::uint32_t i = 0; i < k_LINES; ++i) {
            LineWriter writer(buffer);
            LogFormat::append_record(writer, record, "calibrate", clock);
            // The compiler cannot know which byte, so it must write them all.
            sink = static_cast<std::uint8_t>(
                sink + static_cast<std::uint8_t>(buffer[writer.size() / 2]));
        }
        const double seconds = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        best = std::max(best, static_cast<double>(k_LINES) / seconds);
    }
    return best;
}

/// 'text' without the padding on its right.
std::string trimmed(std::string text)
{
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    return text;
}

/// A pipe the logger writes to, with a thread reading it as it goes: it counts
/// the lines written by one thread and discards every other, so that a flood
/// does not fill a disk and what arrives can be watched while it is running.
class PipeCounter {
  private:
    std::int32_t               d_fds[2] = {-1, -1};
    std::string                d_thread;  // the thread whose lines are counted
    std::chrono::milliseconds  d_delay;   // how long it rests after each read
    std::atomic<std::uint64_t> d_counted{0};
    std::thread                d_reader;

    void read_all()
    {
        std::string pending;
        char        chunk[65'536];
        for (;;) {
            const ssize_t got = ::read(d_fds[0], chunk, sizeof chunk);
            if (got <= 0) {
                return;
            }
            pending.append(chunk, static_cast<std::size_t>(got));
            std::size_t start = 0;
            for (std::size_t end;
                 (end = pending.find('\n', start)) != std::string::npos;
                 start = end + 1) {
                // The thread column: bytes 20 to 27 of a line.
                if (end - start >= 28 &&
                    trimmed(pending.substr(start + 20, 8)) == d_thread) {
                    d_counted.fetch_add(1, std::memory_order_relaxed);
                }
            }
            pending.erase(0, start);
            if (d_delay.count() != 0) {
                std::this_thread::sleep_for(d_delay);
            }
        }
    }

  public:
    /// Count the lines of 'thread', resting 'delay' after each read: a slow
    /// reader makes the logger the slower side, whatever the machine.
    explicit PipeCounter(std::string                     thread,
                         const std::chrono::milliseconds delay = 0ms)
    : d_thread(std::move(thread))
    , d_delay(delay)
    {
        if (::pipe(d_fds) == 0) {
            d_reader = std::thread([this] { read_all(); });
        }
    }

    ~PipeCounter()
    {
        close_write();
        if (d_reader.joinable()) {
            d_reader.join();
        }
        if (d_fds[0] >= 0) {
            ::close(d_fds[0]);
        }
    }

    PipeCounter(const PipeCounter&)            = delete;
    PipeCounter& operator=(const PipeCounter&) = delete;

    /// The descriptor to give a 'Logger'.
    std::int32_t fd() const
    {
        return d_fds[1];
    }

    /// Say that nothing more will be written, so the reader ends.
    void close_write()
    {
        if (d_fds[1] >= 0) {
            ::close(d_fds[1]);
            d_fds[1] = -1;
        }
    }

    std::uint64_t counted() const
    {
        return d_counted.load();
    }
};

/// A line of the log taken apart. The prefix is fixed-width while the
/// seconds are below 10000: '[', 4 for the seconds, '.', 6 for the
/// microseconds, ']', a space, 5 for the severity, a space, 8 for the thread,
/// a space, and then what the record says.
struct Line {
    bool          well_formed  = false;
    std::uint64_t microseconds = 0;
    std::string   severity;  // without its padding
    std::string   thread;    // without its padding
    std::string   payload;   // what follows the prefix
    bool          has_context = false;
    std::uint32_t context     = 0;
};

constexpr std::size_t k_PREFIX_SIZE = 29;

Line parse(const std::string& text)
{
    Line line;
    if (text.size() < k_PREFIX_SIZE || text[0] != '[' || text[5] != '.' ||
        text[12] != ']' || text[13] != ' ' || text[19] != ' ' ||
        text[28] != ' ') {
        return line;
    }
    const std::string seconds = text.substr(1, 4);
    const std::string micros  = text.substr(6, 6);
    if (seconds.find_first_not_of(" 0123456789") != std::string::npos ||
        micros.find_first_not_of("0123456789") != std::string::npos) {
        return line;
    }
    line.well_formed  = true;
    line.microseconds = (std::stoull(seconds) * 1'000'000U) +
                        std::stoull(micros);
    line.severity     = trimmed(text.substr(14, 5));
    line.thread       = trimmed(text.substr(20, 8));
    line.payload      = text.substr(k_PREFIX_SIZE);

    const auto at = line.payload.find("ctx=0x");
    if (at != std::string::npos && line.payload.size() >= at + 6 + 8) {
        line.has_context = true;
        line.context     = static_cast<std::uint32_t>(
            std::stoul(line.payload.substr(at + 6, 8), nullptr, 16));
    }
    return line;
}

std::vector<Line> parsed(const LogFile& file)
{
    std::vector<Line> result;
    for (const std::string& text : file.lines()) {
        result.push_back(parse(text));
    }
    return result;
}

/// The number of lines of 'lines' that are not whole.
std::size_t malformed(const std::vector<Line>& lines)
{
    return static_cast<std::size_t>(std::count_if(lines.begin(),
                                                  lines.end(),
                                                  [](const Line& line) {
                                                      return !line.well_formed;
                                                  }));
}

Error error_with(const std::uint32_t context)
{
    return Error::make(Error::Kind::e_UNKNOWN,
                       Error::CoreReason::e_UNKNOWN,
                       context);
}

LogMessage core_message()
{
    return LogMessage::make(LogMessage::CoreMessage::e_UNKNOWN);
}

/// Log 'count' errors through 'handle', whose contexts are 'first', 'first' +
/// 1, and so on.
void log_errors(Logger::Handle&     handle,
                const std::uint32_t first,
                const std::uint32_t count,
                const Severity      severity = Severity::e_INFO)
{
    for (std::uint32_t i = 0; i < count; ++i) {
        handle.log_error(severity, error_with(first + i));
    }
}

/// The contexts of the lines of 'lines' written by 'thread', in order.
std::vector<std::uint32_t> contexts_of(const std::vector<Line>& lines,
                                       const std::string_view   thread)
{
    std::vector<std::uint32_t> result;
    for (const Line& line : lines) {
        if (line.well_formed && line.has_context && line.thread == thread) {
            result.push_back(line.context);
        }
    }
    return result;
}

/// The values first, first + 1, ..., first + count - 1.
std::vector<std::uint32_t> sequence(const std::uint32_t first,
                                    const std::uint32_t count)
{
    std::vector<std::uint32_t> result(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        result[i] = first + i;
    }
    return result;
}

// ----------------------------------------------------------- constants -----

TEST(CoreLog, TheNumbersTheCodeReliesOnHold)
{
    // The ring masks, so its capacity is a power of two.
    static_assert((Logger::k_RING_CAPACITY & (Logger::k_RING_CAPACITY - 1)) ==
                  0);
    // A record is cut only if it is larger than the room a flush leaves, so
    // the room must exceed the longest line, with the two bytes of the cut
    // mark.
    static_assert(Logger::k_FLUSH_THRESHOLD > LogFormat::k_MAX_LINE_SIZE + 2);
    static_assert(Logger::k_BATCH_SIZE >= 2 * Logger::k_FLUSH_THRESHOLD);
    // The name the locked calls carry is kept whole and shown whole.
    static_assert(Logger::k_SHARED_NAME.size() <= Logger::k_MAX_NAME_SIZE);
    static_assert(Logger::k_SHARED_NAME.size() <= LogFormat::k_THREAD_WIDTH);
    static_assert(Logger::k_MAX_THREADS > 0);
    SUCCEED();
}

// ---------------------------------------------------------- one thread -----

TEST(CoreLog, ALoggerWithNothingLoggedWritesNothing)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger logger(file.fd());
    }
    EXPECT_TRUE(file.lines().empty());
}

TEST(CoreLog, OneThreadsRecordsAllComeOutWholeAndInOrder)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    constexpr std::uint32_t k_COUNT = 1'000;  // fewer than the ring holds
    {
        Logger                 logger(file.fd());
        Result<Logger::Handle> handle = logger.register_current_thread(
            "render");
        ASSERT_TRUE(handle.has_value());
        log_errors(*handle, 0, k_COUNT, Severity::e_WARN);
    }

    const std::vector<Line> lines = parsed(file);
    ASSERT_EQ(lines.size(), k_COUNT);
    EXPECT_EQ(malformed(lines), 0U);
    for (const Line& line : lines) {
        EXPECT_EQ(line.thread, "render");
        EXPECT_EQ(line.severity, "WARN");
    }
    EXPECT_EQ(contexts_of(lines, "render"), sequence(0, k_COUNT));
}

TEST(CoreLog, ALineIsTheErrorTheThreadLogged)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger                 logger(file.fd());
        Result<Logger::Handle> handle = logger.register_current_thread(
            "render");
        ASSERT_TRUE(handle.has_value());
        handle->log_error(Severity::e_FATAL, error_with(0xCD));
    }

    const std::vector<std::string> lines = file.lines();
    ASSERT_EQ(lines.size(), 1U);
    // The timestamp is whatever the clock said; what follows it is fixed.
    EXPECT_EQ(lines[0].substr(13),
              " FATAL render   core | unknown | unknown | ctx=0x000000cd");
}

TEST(CoreLog, ErrorsAndMessagesShareOneStreamInOrder)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger                 logger(file.fd());
        Result<Logger::Handle> handle = logger.register_current_thread(
            "render");
        ASSERT_TRUE(handle.has_value());
        for (std::uint32_t i = 0; i < 100; ++i) {
            handle->log_error(Severity::e_INFO, error_with(i));
            handle->log_message(Severity::e_DEBUG, core_message());
        }
    }

    const std::vector<Line> lines = parsed(file);
    ASSERT_EQ(lines.size(), 200U);
    EXPECT_EQ(malformed(lines), 0U);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i % 2 == 0) {
            EXPECT_EQ(lines[i].severity, "INFO") << i;
            EXPECT_TRUE(lines[i].has_context) << i;
            EXPECT_EQ(lines[i].context, i / 2) << i;
        } else {
            EXPECT_EQ(lines[i].severity, "DEBUG") << i;
            EXPECT_EQ(lines[i].payload, "core | unknown") << i;
        }
    }
}

TEST(CoreLog, EverySeverityComesOutAsItsName)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger                 logger(file.fd());
        Result<Logger::Handle> handle = logger.register_current_thread(
            "render");
        ASSERT_TRUE(handle.has_value());
        for (std::uint32_t s = 0;
             s < static_cast<std::uint32_t>(Severity::e_COUNT);
             ++s) {
            handle->log_error(static_cast<Severity>(s), error_with(s));
        }
    }

    const std::vector<Line> lines = parsed(file);
    ASSERT_EQ(lines.size(), 6U);
    const char* const names[] =
        {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    for (std::size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(lines[i].severity, names[i]);
    }
}

TEST(CoreLog, TimestampsNeverGoBackwardsOnOneThreadAndStartNearZero)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger                 logger(file.fd());
        Result<Logger::Handle> handle = logger.register_current_thread(
            "render");
        ASSERT_TRUE(handle.has_value());
        for (std::uint32_t i = 0; i < 50; ++i) {
            handle->log_error(Severity::e_INFO, error_with(i));
            std::this_thread::sleep_for(200us);
        }
    }

    const std::vector<Line> lines = parsed(file);
    ASSERT_EQ(lines.size(), 50U);
    for (std::size_t i = 1; i < lines.size(); ++i) {
        EXPECT_GE(lines[i].microseconds, lines[i - 1].microseconds) << i;
    }
    // Seconds since the logger started: well under a minute for this test.
    EXPECT_LT(lines.back().microseconds, 60U * 1'000'000U);
}

TEST(CoreLog, ThreadNamesAreCutToTheirColumnAndNeverRunIntoTheMessage)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    const std::vector<std::string> names = {
        "",
        "a",
        "render",
        "12345678",                                   // exactly the column
        "streaming-worker-with-a-std::int64_t-name",  // longer than the kept
                                                      // 16
    };
    {
        Logger                              logger(file.fd());
        std::vector<Result<Logger::Handle>> handles;
        for (const std::string& name : names) {
            handles.push_back(logger.register_current_thread(name));
            ASSERT_TRUE(handles.back().has_value()) << name;
        }
        for (std::size_t i = 0; i < handles.size(); ++i) {
            handles[i]->log_message(Severity::e_INFO, core_message());
        }
    }

    // One line per name, in slot order, so in the order registered.
    const std::vector<Line> lines = parsed(file);
    ASSERT_EQ(lines.size(), names.size());
    EXPECT_EQ(malformed(lines), 0U);
    const std::vector<std::string> shown = {
        "",
        "a",
        "render",
        "12345678",
        "streamin",
    };
    for (std::size_t i = 0; i < lines.size(); ++i) {
        EXPECT_EQ(lines[i].thread, shown[i]) << "name '" << names[i] << "'";
        // The message starts in its own column, a space after the thread's.
        EXPECT_EQ(lines[i].payload, "core | unknown")
            << "name '" << names[i] << "'";
    }
}

TEST(CoreLog, ABatchOfManyThousandRecordsIsWrittenWholeWithNoLineSplit)
{
    // Far more than one 64 KiB batch, so the flush that makes room happens
    // many times. Paced in bursts the ring holds, so that a loaded machine
    // loses at most a few; what is checked is that no line is cut by a flush
    // and that everything is accounted for.
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    constexpr std::uint32_t k_BURST  = 2'000;
    constexpr std::uint32_t k_BURSTS = 60;
    {
        Logger                 logger(file.fd());
        Result<Logger::Handle> handle = logger.register_current_thread(
            "render");
        ASSERT_TRUE(handle.has_value());
        for (std::uint32_t burst = 0; burst < k_BURSTS; ++burst) {
            log_errors(*handle, burst * k_BURST, k_BURST);
            std::this_thread::sleep_for(3ms);
        }
    }

    const std::vector<Line> lines = parsed(file);
    EXPECT_EQ(malformed(lines), 0U);

    std::uint64_t lost     = 0;
    std::uint64_t seen     = 0;
    std::uint32_t previous = 0;
    bool          first    = true;
    for (const Line& line : lines) {
        if (line.thread == "logger") {
            std::istringstream in(line.payload);
            std::string        who;
            std::string        word;
            std::uint64_t      count = 0;
            in >> who >> word >> count;
            lost += count;
        } else if (line.has_context) {
            ++seen;
            EXPECT_TRUE(first || line.context > previous);
            previous = line.context;
            first    = false;
        }
    }
    EXPECT_EQ(seen + lost, std::uint64_t{k_BURST} * k_BURSTS);
    EXPECT_GT(seen, std::uint64_t{k_BURST} * k_BURSTS / 2);
}

// --------------------------------------------------------- many threads -----

TEST(CoreLog, TwoThreadsOneFileEachInItsOwnOrder)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    constexpr std::uint32_t k_COUNT = 2'000;
    {
        Logger     logger(file.fd());
        const auto work = [&logger](const char*         name,
                                    const std::uint32_t first) {
            Result<Logger::Handle> handle = logger.register_current_thread(
                name);
            ASSERT_TRUE(handle.has_value());
            log_errors(*handle, first, k_COUNT);
        };
        std::thread render(work, "render", 0U);
        std::thread stream(work, "stream", 100'000U);
        render.join();
        stream.join();
    }

    const std::vector<Line> lines = parsed(file);
    EXPECT_EQ(lines.size(), 2U * k_COUNT);
    EXPECT_EQ(malformed(lines), 0U);
    EXPECT_EQ(contexts_of(lines, "render"), sequence(0, k_COUNT));
    EXPECT_EQ(contexts_of(lines, "stream"), sequence(100'000, k_COUNT));
}

TEST(CoreLog, EverySlotCanBeUsedAtOnce)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    constexpr std::uint32_t k_EACH = 50;
    {
        Logger                              logger(file.fd());
        std::vector<Result<Logger::Handle>> handles;
        for (std::size_t i = 0; i < Logger::k_MAX_THREADS; ++i) {
            handles.push_back(
                logger.register_current_thread("t" + std::to_string(i)));
            ASSERT_TRUE(handles.back().has_value()) << i;
        }
        for (std::size_t i = 0; i < handles.size(); ++i) {
            log_errors(*handles[i],
                       static_cast<std::uint32_t>(i) * 1'000,
                       k_EACH);
        }
    }

    const std::vector<Line> lines = parsed(file);
    EXPECT_EQ(lines.size(), Logger::k_MAX_THREADS * k_EACH);
    for (std::size_t i = 0; i < Logger::k_MAX_THREADS; ++i) {
        EXPECT_EQ(contexts_of(lines, "t" + std::to_string(i)),
                  sequence(static_cast<std::uint32_t>(i) * 1'000, k_EACH))
            << i;
    }
}

TEST(CoreLog, AThirtyThirdThreadIsRefusedWithItsOwnError)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    Logger                              logger(file.fd());
    std::vector<Result<Logger::Handle>> handles;
    for (std::size_t i = 0; i < Logger::k_MAX_THREADS; ++i) {
        handles.push_back(logger.register_current_thread("t"));
        ASSERT_TRUE(handles.back().has_value()) << i;
    }

    const Result<Logger::Handle> refused = logger.register_current_thread(
        "extra");
    ASSERT_FALSE(refused.has_value());
    // The context is the limit that was reached.
    EXPECT_EQ(refused.error(),
              Error::make(Error::Kind::e_EXHAUSTED,
                          Error::CoreReason::e_LOG_TOO_MANY_THREADS,
                          static_cast<std::uint32_t>(Logger::k_MAX_THREADS)));
    EXPECT_EQ(refused.error().context(), 32U);

    handles.clear();  // every handle must be gone before the logger is
}

TEST(CoreLog, ARefusalLeavesTheLoggerWorking)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger                              logger(file.fd());
        std::vector<Result<Logger::Handle>> handles;
        for (std::size_t i = 0; i < Logger::k_MAX_THREADS; ++i) {
            handles.push_back(logger.register_current_thread("t"));
            ASSERT_TRUE(handles.back().has_value());
        }
        EXPECT_FALSE(logger.register_current_thread("extra").has_value());
        EXPECT_FALSE(logger.register_current_thread("again").has_value());

        log_errors(*handles.front(), 7, 3);
        handles.clear();
    }

    const std::vector<Line> lines = parsed(file);
    EXPECT_EQ(contexts_of(lines, "t"), sequence(7, 3));
}

TEST(CoreLog, ASlotComesBackWhenItsHandleIsDestroyed)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    Logger                              logger(file.fd());
    std::vector<Result<Logger::Handle>> handles;
    for (std::size_t i = 0; i < Logger::k_MAX_THREADS; ++i) {
        handles.push_back(logger.register_current_thread("t"));
        ASSERT_TRUE(handles.back().has_value());
    }
    ASSERT_FALSE(logger.register_current_thread("extra").has_value());

    handles.pop_back();  // closes one; the logger thread frees it on its next
                         // pass

    // The slot is free again once the logger thread has emptied and freed the
    // producer, a pass or two (a millisecond each) later; given a generous
    // time, for a loaded machine.
    std::optional<Result<Logger::Handle>> extra;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline) {
        extra.emplace(logger.register_current_thread("extra"));
        if (extra->has_value()) {
            break;
        }
        std::this_thread::sleep_for(100us);
    }
    ASSERT_TRUE(extra.has_value() && extra->has_value())
        << "no slot came back within 10 s";

    extra.reset();
    handles.clear();
}

TEST(CoreLog, RegisteringTwiceOnOneThreadGivesTwoWorkingHandles)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger                 logger(file.fd());
        Result<Logger::Handle> first = logger.register_current_thread("first");
        Result<Logger::Handle> second = logger.register_current_thread(
            "second");
        ASSERT_TRUE(first.has_value());
        ASSERT_TRUE(second.has_value());
        log_errors(*first, 0, 20);
        log_errors(*second, 100, 20);
    }

    const std::vector<Line> lines = parsed(file);
    EXPECT_EQ(contexts_of(lines, "first"), sequence(0, 20));
    EXPECT_EQ(contexts_of(lines, "second"), sequence(100, 20));
}

TEST(CoreLog, AMovedHandleLogsAndTheOriginalDoesNothing)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger                        logger(file.fd());
        std::optional<Logger::Handle> moved;
        {
            Result<Logger::Handle> original = logger.register_current_thread(
                "render");
            ASSERT_TRUE(original.has_value());
            log_errors(*original, 0, 5);
            moved.emplace(std::move(*original));
        }  // the moved-from handle goes here, and closes nothing
        log_errors(*moved, 5, 5);
        // 'moved' goes at the end of the scope, and closes the producer, once.
    }

    EXPECT_EQ(contexts_of(parsed(file), "render"), sequence(0, 10));
}

// -------------------------------------------------- threads come and go -----

TEST(CoreLog, ThreadsComeAndGoThroughFewerSlotsThanThreads)
{
    // Far more threads than slots, a few alive at once, each logging three
    // records and leaving at once; and one that stays registered for all of
    // it. Every record carries (thread << 8 | number), so each thread must
    // have exactly 0, 1, 2 in that order, whichever slot it had.
    constexpr std::uint32_t k_THREADS  = 2'000;
    constexpr std::uint32_t k_WIDE     = 8;
    constexpr std::uint32_t k_EACH     = 3;
    constexpr std::uint32_t k_RESIDENT = 150;

    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger            logger(file.fd());
        std::atomic<bool> resident_ready{false};

        std::thread resident([&] {
            Result<Logger::Handle> handle = logger.register_current_thread(
                "resident");
            ASSERT_TRUE(handle.has_value());
            resident_ready.store(true);
            for (std::uint32_t i = 0; i < k_RESIDENT; ++i) {
                handle->log_error(Severity::e_INFO,
                                  error_with((k_THREADS << 8U) | i));
                std::this_thread::sleep_for(300us);
            }
        });
        while (!resident_ready.load()) {
            std::this_thread::yield();
        }

        const auto lifetime = [&logger](const std::uint32_t id) {
            // Every slot may be taken by threads that finished and are not
            // freed yet: wait for one.
            std::optional<Result<Logger::Handle>> handle;
            for (;;) {
                handle.emplace(logger.register_current_thread("t"));
                if (handle->has_value()) {
                    break;
                }
                std::this_thread::sleep_for(200us);
            }
            // Registered with nothing in the ring, for a while, so that the
            // logger thread sees a producer that is live and empty.
            std::this_thread::sleep_for(std::chrono::microseconds((id * 37U) %
                                                                  300U));
            for (std::uint32_t i = 0; i < k_EACH; ++i) {
                (*handle)->log_error(Severity::e_INFO,
                                     error_with((id << 8U) | i));
            }
        };

        for (std::uint32_t first = 0; first < k_THREADS; first += k_WIDE) {
            std::vector<std::thread> wave;
            for (std::uint32_t id = first;
                 id < first + k_WIDE && id < k_THREADS;
                 ++id) {
                wave.emplace_back(lifetime, id);
            }
            for (std::thread& thread : wave) {
                thread.join();
            }
        }
        resident.join();
    }

    const std::vector<Line> lines = parsed(file);
    EXPECT_EQ(lines.size(), (std::size_t{k_THREADS} * k_EACH) + k_RESIDENT);
    EXPECT_EQ(malformed(lines), 0U);

    std::map<std::uint32_t, std::vector<std::uint32_t>> seen;
    for (const Line& line : lines) {
        if (line.has_context) {
            seen[line.context >> 8U].push_back(line.context & 0xFFU);
        }
    }
    std::uint32_t wrong = 0;
    for (std::uint32_t id = 0; id < k_THREADS; ++id) {
        if (seen[id] != std::vector<std::uint32_t>{0, 1, 2}) {
            ++wrong;
        }
    }
    EXPECT_EQ(wrong, 0U) << "threads whose records are not exactly 0, 1, 2";
    EXPECT_EQ(seen[k_THREADS].size(), k_RESIDENT);
}

TEST(CoreLog, ThreadsThatRegisterAtTheSameInstantEachGetTheirOwnSlot)
{
    // Registration is a compare-exchange on the first free slot, so that two
    // threads reaching the same one cannot both have it. Twenty-four threads
    // are held at a gate and let go together, so that they all find slot 0
    // free at once, and this is repeated on a fresh logger each time; a
    // registration that looked and then stored would give two of them the
    // same ring, and records would be lost or doubled.
    constexpr std::uint32_t k_THREADS = 24;
    constexpr std::uint32_t k_ROUNDS  = 60;
    constexpr std::uint32_t k_EACH    = 10;

    for (std::uint32_t round = 0; round < k_ROUNDS; ++round) {
        const LogFile file;
        ASSERT_GE(file.fd(), 0);
        {
            Logger                     logger(file.fd());
            std::atomic<std::uint32_t> ready{0};
            std::atomic<bool>          go{false};
            std::vector<std::thread>   threads;
            threads.reserve(k_THREADS);
            for (std::uint32_t id = 0; id < k_THREADS; ++id) {
                threads.emplace_back([&, id] {
                    ready.fetch_add(1);
                    while (!go.load(std::memory_order_acquire)) {
                    }
                    Result<Logger::Handle> handle =
                        logger.register_current_thread("t");
                    ASSERT_TRUE(handle.has_value());
                    for (std::uint32_t i = 0; i < k_EACH; ++i) {
                        handle->log_error(Severity::e_INFO,
                                          error_with((id << 8U) | i));
                    }
                });
            }
            while (ready.load() != k_THREADS) {
                std::this_thread::yield();
            }
            go.store(true, std::memory_order_release);
            for (std::thread& thread : threads) {
                thread.join();
            }
        }

        std::map<std::uint32_t, std::vector<std::uint32_t>> seen;
        for (const Line& line : parsed(file)) {
            if (line.has_context) {
                seen[line.context >> 8U].push_back(line.context & 0xFFU);
            }
        }
        std::uint32_t wrong = 0;
        for (std::uint32_t id = 0; id < k_THREADS; ++id) {
            std::vector<std::uint32_t> expected(k_EACH);
            for (std::uint32_t i = 0; i < k_EACH; ++i) {
                expected[i] = i;
            }
            wrong += (seen[id] == expected) ? 0U : 1U;
        }
        ASSERT_EQ(wrong, 0U) << "round " << round;
    }
}

TEST(CoreLog, ARecordIsWrittenEvenIfItsThreadAndTheLoggerEndAtOnce)
{
    // Logged, then the handle and the logger destroyed straight away: the
    // last pass is what writes them.
    for (std::int32_t round = 0; round < 20; ++round) {
        const LogFile file;
        ASSERT_GE(file.fd(), 0);
        {
            Logger logger(file.fd());
            std::thread([&logger] {
                Result<Logger::Handle> handle = logger.register_current_thread(
                    "short");
                ASSERT_TRUE(handle.has_value());
                log_errors(*handle, 0, 100);
            }).join();
        }
        EXPECT_EQ(contexts_of(parsed(file), "short"), sequence(0, 100))
            << round;
    }
}

// --------------------------------------------------------- locked calls -----

TEST(CoreLog, ThreadsWithNoHandleLogThroughTheLockedPath)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    constexpr std::uint32_t k_THREADS = 4;
    constexpr std::uint32_t k_EACH = 400;  // fewer than the ring holds, in all
    {
        Logger                   logger(file.fd());
        std::vector<std::thread> threads;
        threads.reserve(k_THREADS);
        for (std::uint32_t t = 0; t < k_THREADS; ++t) {
            threads.emplace_back([&logger, t] {
                for (std::uint32_t i = 0; i < k_EACH; ++i) {
                    logger.log_error_locked(Severity::e_ERROR,
                                            error_with((t << 16U) | i));
                }
            });
        }
        for (std::thread& thread : threads) {
            thread.join();
        }
    }

    const std::vector<Line> lines = parsed(file);
    EXPECT_EQ(lines.size(), std::size_t{k_THREADS} * k_EACH);
    EXPECT_EQ(malformed(lines), 0U);
    std::map<std::uint32_t, std::vector<std::uint32_t>> per_thread;
    for (const Line& line : lines) {
        EXPECT_EQ(line.thread, "shared");
        EXPECT_EQ(line.severity, "ERROR");
        per_thread[line.context >> 16U].push_back(line.context & 0xFFFFU);
    }
    // Each thread's own records are in the order it made them.
    for (std::uint32_t t = 0; t < k_THREADS; ++t) {
        std::vector<std::uint32_t> expected(k_EACH);
        for (std::uint32_t i = 0; i < k_EACH; ++i) {
            expected[i] = i;
        }
        EXPECT_EQ(per_thread[t], expected) << "thread " << t;
    }
}

TEST(CoreLog, LockedMessagesComeOutAsMessagesNamedShared)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger logger(file.fd());
        std::thread([&logger] {
            logger.log_message_locked(Severity::e_INFO, core_message());
        }).join();
    }

    const std::vector<Line> lines = parsed(file);
    ASSERT_EQ(lines.size(), 1U);
    EXPECT_EQ(lines[0].thread, "shared");
    EXPECT_EQ(lines[0].payload, "core | unknown");
}

TEST(CoreLog, HandlesAndLockedCallsShareOneFile)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger                 logger(file.fd());
        Result<Logger::Handle> handle = logger.register_current_thread(
            "render");
        ASSERT_TRUE(handle.has_value());
        std::thread driver([&logger] {
            for (std::uint32_t i = 0; i < 100; ++i) {
                logger.log_error_locked(Severity::e_WARN, error_with(i));
            }
        });
        log_errors(*handle, 0, 100);
        driver.join();
    }

    const std::vector<Line> lines = parsed(file);
    EXPECT_EQ(contexts_of(lines, "render"), sequence(0, 100));
    EXPECT_EQ(contexts_of(lines, "shared"), sequence(0, 100));
}

// ---------------------------------------------------------- independence
// -----

TEST(CoreLog, TwoLoggersAtOnceDoNotHearEachOther)
{
    const LogFile first_file;
    const LogFile second_file;
    ASSERT_GE(first_file.fd(), 0);
    ASSERT_GE(second_file.fd(), 0);
    {
        auto first               = std::make_unique<Logger>(first_file.fd());
        auto second              = std::make_unique<Logger>(second_file.fd());
        Result<Logger::Handle> a = first->register_current_thread("alpha");
        Result<Logger::Handle> b = second->register_current_thread("beta");
        ASSERT_TRUE(a.has_value());
        ASSERT_TRUE(b.has_value());
        log_errors(*a, 0, 30);
        log_errors(*b, 500, 30);
        first->log_error_locked(Severity::e_INFO, error_with(900));
    }

    const std::vector<Line> first  = parsed(first_file);
    const std::vector<Line> second = parsed(second_file);
    EXPECT_EQ(contexts_of(first, "alpha"), sequence(0, 30));
    EXPECT_EQ(contexts_of(first, "shared"), sequence(900, 1));
    EXPECT_TRUE(contexts_of(first, "beta").empty());
    EXPECT_EQ(contexts_of(second, "beta"), sequence(500, 30));
    EXPECT_TRUE(second.size() == 30U);
}

TEST(CoreLog, ManyLoggersOneAfterAnotherLeaveNothingBehind)
{
    // Each one starts a thread, gives out a producer and frees it; run under
    // a sanitizer this is where a leak or a stale pointer shows.
    for (std::int32_t i = 0; i < 40; ++i) {
        const LogFile file;
        ASSERT_GE(file.fd(), 0);
        {
            Logger                 logger(file.fd());
            Result<Logger::Handle> handle = logger.register_current_thread(
                "t");
            ASSERT_TRUE(handle.has_value());
            log_errors(*handle, 0, 10);
        }
        EXPECT_EQ(parsed(file).size(), 10U) << i;
    }
}

TEST(CoreLog, TheLoggerDoesNotCloseTheDescriptorItWasGiven)
{
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    {
        Logger logger(file.fd());
    }
    // Still open: the caller opened it and closes it, after the logger.
    EXPECT_EQ(::write(file.fd(), "x", 1), 1);
}

// --------------------------------------------------------------- floods -----

TEST(CoreLog, AnIdleLoggerUsesAlmostNoCpu)
{
    // The logger thread wakes every k_DRAIN_INTERVAL and finds nothing: a
    // pass over empty rings and a rest, about one percent of a core. One that
    // never rested would use all of one, for as long as the program runs.
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    const Logger logger(file.fd());
    std::this_thread::sleep_for(100ms);

    const double before = process_cpu_seconds();
    const auto   start  = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(500ms);
    const double used = process_cpu_seconds() - before;
    const double wall = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - start)
                            .count();

    EXPECT_LT(used / wall, 0.25)
        << "an idle logger used " << 100.0 * used / wall << " % of a core";
}

TEST(CoreLog, ALoggerWithALightLoadRestsBetweenPasses)
{
    // A thread logs about twenty lines a millisecond: far from a full ring,
    // so a pass is not behind and the logger thread rests after it. One that
    // rested only when it found nothing would not rest while a thread logs,
    // and would use all of a core.
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    Logger                 logger(file.fd());
    Result<Logger::Handle> handle = logger.register_current_thread("light");
    ASSERT_TRUE(handle.has_value());
    std::this_thread::sleep_for(100ms);

    const double  before = process_cpu_seconds();
    const auto    start  = std::chrono::steady_clock::now();
    std::uint32_t logged = 0;
    while (std::chrono::steady_clock::now() - start < 500ms) {
        for (std::uint32_t n = 0; n < 20; ++n, ++logged) {
            handle->log_error(Severity::e_TRACE, error_with(logged));
        }
        std::this_thread::sleep_for(1ms);
    }
    const double used = process_cpu_seconds() - before;
    const double wall = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - start)
                            .count();

    EXPECT_LT(used / wall, 0.25)
        << "a logger that is not behind used " << 100.0 * used / wall
        << " % of a core, for " << logged << " lines in " << wall << " s";
}

/// The most lines a second a logger can write if it rests k_DRAIN_INTERVAL
/// after taking one ring: a pass takes at most a ring's worth from a thread,
/// whatever its speed.
constexpr double k_RESTING_CAP =
    static_cast<double>(Logger::k_RING_CAPACITY) /
    std::chrono::duration<double>(Logger::k_DRAIN_INTERVAL).count();

/// Return why a test of the logger's speed cannot tell a logger that rests
/// from one that is slow on this machine, or an empty string if it can. The
/// difference only shows on a machine that formats well past the cap of one
/// that rests; a slower one, or a build under a sanitizer, would have the test
/// reported as skipped, not passed. 'formats' is set to what the machine
/// formats a second.
std::string reason_to_skip(double& formats)
{
    // The flooding thread, the logger thread and the rest of the machine.
    if (std::thread::hardware_concurrency() < 3) {
        return "fewer than 3 hardware threads: the two busy threads would "
               "share them with everything else";
    }

    prefer_fast_cores();
    formats = lines_formatted_per_second();
    if (formats >= 2.0 * k_RESTING_CAP) {
        return {};
    }
    std::ostringstream reason;
    reason << "this build formats " << formats / 1e6
           << " M lines a second, under twice the " << k_RESTING_CAP / 1e6
           << " M a second a logger that rests can write: it could not tell "
              "the two apart";
    return reason.str();
}

/// Run 'flood' on a thread of its own for 200 ms, with the logger and a flag
/// that says when to stop, and return the record lines the logger wrote a
/// second, from the start to the end of its destructor: the last pass counts.
template <class t_FLOOD> double lines_written_per_second(t_FLOOD flood)
{
    const LogFile file;
    if (file.fd() < 0) {
        return 0.0;
    }
    std::chrono::steady_clock::time_point start;
    {
        Logger            logger(file.fd());
        std::atomic<bool> stop{false};
        std::thread       thread([&] { flood(logger, stop); });
        start = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(200ms);
        stop.store(true);
        thread.join();
    }
    const double seconds = std::chrono::duration<double>(
                               std::chrono::steady_clock::now() - start)
                               .count();
    return static_cast<double>(count_record_lines(file.path())) / seconds;
}

/// The rate a flood must pass: more than a logger that rests can write.
constexpr double k_PASS_RATE = 1.25 * k_RESTING_CAP;

/// Run 'flood' as [lines_written_per_second] does, until one run passes or
/// five have failed, and return the best rate. A logger that rests can never
/// pass, whatever the machine does, so a second try cannot hide it; a busy
/// machine that takes the cores of the two busy threads for a while can fail
/// a run, and not often five.
template <class t_FLOOD> double best_rate_of(t_FLOOD flood)
{
    constexpr std::uint32_t k_ATTEMPTS = 5;

    double best = 0.0;
    for (std::uint32_t attempt = 0;
         attempt < k_ATTEMPTS && best <= k_PASS_RATE;
         ++attempt) {
        best = std::max(best, lines_written_per_second(flood));
    }
    return best;
}

TEST(CoreLog, ALoggerThatIsBehindDoesNotRestBetweenPasses)
{
    // A thread that floods keeps its ring full, so the logger is behind and
    // must go on at once. Were it to rest, it could write no more than one
    // ring a pass.
    double            formats = 0.0;
    const std::string skip    = reason_to_skip(formats);
    if (!skip.empty()) {
        GTEST_SKIP() << skip;
    }

    const double rate = best_rate_of([](Logger&                  logger,
                                        const std::atomic<bool>& stop) {
        Result<Logger::Handle> handle = logger.register_current_thread(
            "flood");
        if (!handle) {
            return;
        }
        for (std::uint32_t i = 0; !stop.load(std::memory_order_relaxed);) {
            for (std::uint32_t n = 0; n < 256; ++n, ++i) {
                handle->log_error(Severity::e_TRACE, error_with(i));
            }
        }
    });

    EXPECT_GT(rate, k_PASS_RATE)
        << "the logger wrote " << rate / 1e6 << " M lines a second from a "
        << "thread that floods it, and formats " << formats / 1e6 << " M: "
        << "it is resting between passes while a ring is full";
}

TEST(CoreLog, AFloodThroughTheLockedPathIsNotHeldToOneRingAPass)
{
    // The ring shared by the threads that have no handle is a ring like the
    // others: when it is full the logger is behind, and must not rest.
    double            formats = 0.0;
    const std::string skip    = reason_to_skip(formats);
    if (!skip.empty()) {
        GTEST_SKIP() << skip;
    }

    const double rate = best_rate_of([](Logger&                  logger,
                                        const std::atomic<bool>& stop) {
        for (std::uint32_t i = 0; !stop.load(std::memory_order_relaxed);) {
            for (std::uint32_t n = 0; n < 256; ++n, ++i) {
                logger.log_error_locked(Severity::e_TRACE, error_with(i));
            }
        }
    });

    EXPECT_GT(rate, k_PASS_RATE)
        << "the logger wrote " << rate / 1e6 << " M lines a second from a "
        << "thread that floods the locked path, and formats " << formats / 1e6
        << " M: it is resting between passes while the shared ring is full";
}

TEST(CoreLog, AFloodLosesRecordsButReportsExactlyHowManyAndKeepsTheRest)
{
    // The thread's name is longer than the sixteen characters kept, and than
    // the eight of its column: a record line shows eight of them, and the loss
    // line, which names the thread in full, shows the sixteen kept.
    const LogFile file;
    ASSERT_GE(file.fd(), 0);
    constexpr std::uint32_t k_FLOOD = 2'000'000;  // 500 rings' worth, at once
    {
        Logger                 logger(file.fd());
        Result<Logger::Handle> handle = logger.register_current_thread(
            "render-thread-flooded");
        ASSERT_TRUE(handle.has_value());
        log_errors(*handle, 0, k_FLOOD);
    }

    const std::vector<Line> lines = parsed(file);
    EXPECT_EQ(malformed(lines), 0U);

    std::uint64_t              lost = 0;
    std::vector<std::uint32_t> kept;
    std::size_t                loss_lines = 0;
    for (const Line& line : lines) {
        if (line.thread == "logger") {
            // '<name> lost <count> records', from the logger itself.
            ++loss_lines;
            EXPECT_EQ(line.severity, "WARN");
            std::istringstream in(line.payload);
            std::string        who;
            std::string        word;
            std::string        tail;
            std::uint64_t      count = 0;
            in >> who >> word >> count >> tail;
            EXPECT_EQ(who, "render-thread-fl");
            EXPECT_EQ(word, "lost");
            EXPECT_EQ(tail, "records");
            EXPECT_GT(count, 0U);
            lost += count;
        } else if (line.has_context) {
            EXPECT_EQ(line.thread, "render-t");
            kept.push_back(line.context);
        }
    }

    // What was written plus what was reported lost is what was logged.
    EXPECT_EQ(kept.size() + lost, std::uint64_t{k_FLOOD});
    EXPECT_GT(lost, 0U) << "two million records at once lost nothing?";
    EXPECT_GT(loss_lines, 0U);
    // What was kept is in order: a full ring refuses the newest, it never
    // reorders or discards the oldest.
    EXPECT_TRUE(std::is_sorted(kept.begin(), kept.end()));
    EXPECT_TRUE(std::adjacent_find(kept.begin(), kept.end()) == kept.end());
}

TEST(CoreLog, AFloodingThreadDoesNotKeepAQuietOneWaiting)
{
    // The logger takes at most a ring's worth from each thread each pass. A
    // thread that refills its ring as fast as it is emptied would otherwise
    // keep the logger in its ring for as long as it floods, and the quiet
    // thread's records would wait for it to stop. So they are watched arriving
    // WHILE the flood is still running, on a pipe, and the flood is stopped
    // only after they have; a logger that starves them never gets there, and
    // the wait ends after ten seconds with the test failed.
    // The reader rests a millisecond after each read, so the logger can write
    // about 64 KiB a millisecond: slower, by far, than a thread can log. The
    // flooding thread's ring is therefore never empty while it runs.
    PipeCounter output("quiet", 1ms);
    ASSERT_GE(output.fd(), 0);
    constexpr std::uint32_t k_QUIET = 100;
    bool                    arrived = false;
    {
        Logger            logger(output.fd());
        std::atomic<bool> stop{false};
        std::thread       flood([&] {
            Result<Logger::Handle> handle = logger.register_current_thread(
                "flood");
            ASSERT_TRUE(handle.has_value());
            for (std::uint32_t i = 0; !stop.load(std::memory_order_relaxed);
                 ++i) {
                handle->log_error(Severity::e_TRACE, error_with(i));
            }
        });
        std::thread       quiet([&] {
            Result<Logger::Handle> handle = logger.register_current_thread(
                "quiet");
            ASSERT_TRUE(handle.has_value());
            for (std::uint32_t i = 0; i < k_QUIET; ++i) {
                handle->log_error(Severity::e_INFO, error_with(i));
                std::this_thread::sleep_for(500us);
            }
        });
        quiet.join();

        const auto deadline = std::chrono::steady_clock::now() + 10s;
        while (std::chrono::steady_clock::now() < deadline) {
            if (output.counted() == k_QUIET) {
                arrived = true;
                break;
            }
            std::this_thread::sleep_for(1ms);
        }
        stop.store(true);
        flood.join();
    }
    output.close_write();

    EXPECT_TRUE(arrived) << "the quiet thread's records had not been written "
                            "after ten seconds of the other one's flood";
}

// ------------------------------------------------------ a broken output -----

TEST(CoreLog, AnUnwritableDescriptorLosesTheLogAndDoesNotHangOrCrash)
{
    // Nothing is left to report a failing log to, so the batches are lost;
    // what must not happen is a hang or a crash. A timer ends the process if
    // the destructor never returns, which a test cannot otherwise report.
    ::alarm(60);
    {
        Logger                 logger(-1);
        Result<Logger::Handle> handle = logger.register_current_thread(
            "render");
        ASSERT_TRUE(handle.has_value());
        for (std::int32_t burst = 0; burst < 20; ++burst) {
            log_errors(*handle, 0, 2'000);
            std::this_thread::sleep_for(2ms);
        }
    }
    ::alarm(0);
    SUCCEED();
}

// --------------------------------------------------------------- death -----

// GoogleTest runs suites named '*DeathTest' first, before any threads exist,
// which is what makes forking the process to watch it die safe.
TEST(CoreLogDeathTest,
     AHandleUsedByAnotherThreadThanTheRegisteredOneIsRejected)
{
    // The ring has one producer: a second thread pushing is a data race that
    // corrupts it. The assert in 'Handle::push' is the only thing that
    // notices. Release builds define NDEBUG, which compiles the assert out, so
    // there the test is reported as skipped, not as passed.
#ifdef NDEBUG
    GTEST_SKIP() << "assert() is compiled out of this build: this test checks "
                    "nothing here, and runs in a debug build";
#endif
    EXPECT_DEATH(
        {
            Logger                 logger(-1);
            Result<Logger::Handle> handle = logger.register_current_thread(
                "main");
            std::thread other([&handle] {
                handle->log_error(Severity::e_INFO, error_with(1));
            });
            other.join();
        },
        "");
}

TEST(CoreLogDeathTest, ALoggerDestroyedWithALiveHandleIsRejected)
{
#ifdef NDEBUG
    GTEST_SKIP() << "assert() is compiled out of this build: this test checks "
                    "nothing here, and runs in a debug build";
#endif
    EXPECT_DEATH(
        {
            auto                         logger = std::make_unique<Logger>(-1);
            const Result<Logger::Handle> handle =
                logger->register_current_thread("main");
            logger.reset();  // the handle is still alive
        },
        "");
}

}  // close unnamed namespace
