#include "doctest.h"

#include "Engine/Object/AnimationTaskPool.h"
#include "Engine/Object/AnimationThreadSettings.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace
{
struct CountingJobs
{
    std::vector<std::atomic<int>> runs;
    explicit CountingJobs(std::size_t count) : runs(count) {}
};

void CountRun(void* context, std::size_t index)
{
    static_cast<CountingJobs*>(context)->runs[index].fetch_add(1, std::memory_order_relaxed);
}

// Deterministic per-index work heavy enough that threads overlap, like one character's bones.
struct MatrixJobs
{
    std::vector<float> out;
};

float MatrixWork(std::size_t index)
{
    float value = static_cast<float>(index) * 0.37f;
    for (int step = 0; step < 200; ++step)
    {
        value = std::sin(value) * 1.7f + std::cos(value * 0.5f + static_cast<float>(step));
    }
    return value;
}

void RunMatrixJob(void* context, std::size_t index)
{
    static_cast<MatrixJobs*>(context)->out[index] = MatrixWork(index);
}

// Slow jobs that note which threads ran them.
struct ThreadJobs
{
    std::mutex mutex;
    std::set<std::thread::id> threads;
    std::atomic<int> runs{ 0 };
};

void RunSlowJob(void* context, std::size_t)
{
    auto* jobs = static_cast<ThreadJobs*>(context);
    {
        std::lock_guard<std::mutex> lock(jobs->mutex);
        jobs->threads.insert(std::this_thread::get_id());
    }
    jobs->runs.fetch_add(1, std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}
} // namespace

TEST_CASE("The animation pool runs every job exactly once, batch after batch [animation][pool]")
{
    for (std::size_t workers = 2; workers <= 8; workers += 3)
    {
        AnimationTaskPool pool;
        pool.Initialize(workers);
        REQUIRE(pool.GetWorkerCount() == workers);

        constexpr int Cycles = 10000;
        constexpr std::size_t MaxJobs = 97;
        CountingJobs jobs(MaxJobs);
        std::vector<int> expected(MaxJobs, 0);
        bool allDispatched = true;
        for (int cycle = 0; cycle < Cycles; ++cycle)
        {
            const std::size_t count = static_cast<std::size_t>(cycle) % MaxJobs + 1;
            allDispatched = pool.Dispatch(&CountRun, &jobs, count) && allDispatched;
            pool.Wait();
            CHECK_FALSE(pool.InFlight());
            for (std::size_t index = 0; index < count; ++index) ++expected[index];
        }
        CHECK(allDispatched);
        for (std::size_t index = 0; index < MaxJobs; ++index)
        {
            CHECK(jobs.runs[index].load() == expected[index]);
        }
        pool.Shutdown();
        CHECK_FALSE(pool.IsRunning());
    }
}

TEST_CASE("The pool's results equal the serial ones [animation][pool]")
{
    constexpr std::size_t Count = 64;
    MatrixJobs serial{ std::vector<float>(Count) };
    for (std::size_t index = 0; index < Count; ++index) RunMatrixJob(&serial, index);

    AnimationTaskPool pool;
    pool.Initialize(4);
    MatrixJobs pooled{ std::vector<float>(Count) };
    REQUIRE(pool.Dispatch(&RunMatrixJob, &pooled, Count));
    pool.Wait();
    CHECK(std::memcmp(serial.out.data(), pooled.out.data(), sizeof(float) * Count) == 0);
}

TEST_CASE("Workers started right before the first batch take part in it [animation][pool]")
{
    // The lazy start: Initialize, then Dispatch at once. Up to 160 jobs of 5 ms each (100 ms or more
    // for one thread alone) leave the workers plenty of time to wake; tried a few times so a busy
    // machine cannot fail it by chance.
    bool workerRan = false;
    for (int attempt = 0; attempt < 5 && !workerRan; ++attempt)
    {
        AnimationTaskPool pool;
        pool.Initialize(2);
        ThreadJobs jobs;
        constexpr std::size_t Count = 160;
        REQUIRE(pool.Dispatch(&RunSlowJob, &jobs, Count));
        pool.Wait();
        CHECK(jobs.runs.load() == static_cast<int>(Count));
        jobs.threads.erase(std::this_thread::get_id());
        workerRan = !jobs.threads.empty();
        pool.Shutdown();
    }
    CHECK(workerRan);
}

TEST_CASE("A pool that never started refuses the batch and runs nothing [animation][pool]")
{
    AnimationTaskPool pool;
    CountingJobs jobs(10);
    CHECK_FALSE(pool.Dispatch(&CountRun, &jobs, 10));
    CHECK_FALSE(pool.InFlight());
    pool.Wait();
    for (const auto& run : jobs.runs) CHECK(run.load() == 0);
}

TEST_CASE("The pool starts again after a shutdown, and an empty batch is nothing to wait for [animation][pool]")
{
    AnimationTaskPool pool;
    pool.Wait();
    pool.Initialize(2);
    pool.Initialize(5); // already running: stays at 2
    CHECK(pool.GetWorkerCount() == 2);
    CountingJobs jobs(30);
    CHECK(pool.Dispatch(&CountRun, &jobs, 0));
    CHECK_FALSE(pool.InFlight());
    pool.Shutdown();

    pool.Initialize(3);
    CHECK(pool.GetWorkerCount() == 3);
    REQUIRE(pool.Dispatch(&CountRun, &jobs, 30));
    pool.Wait();
    for (const auto& run : jobs.runs) CHECK(run.load() == 1);
    pool.Shutdown();
}

TEST_CASE("With one worker the waiting thread shares the batch and every job still runs once [animation][pool]")
{
    AnimationTaskPool pool;
    pool.Initialize(1);
    for (int cycle = 0; cycle < 1000; ++cycle)
    {
        CountingJobs jobs(5);
        REQUIRE(pool.Dispatch(&CountRun, &jobs, 5));
        pool.Wait();
        for (const auto& run : jobs.runs) CHECK(run.load() == 1);
    }
}

TEST_CASE("EnableAnimationTaskPool reads auto, off and on, the old 0 and 1 included [animation][pool]")
{
    using namespace Engine::Object::AnimationThreads;
    CHECK(ParseMode(L"") == Mode::Auto);
    CHECK(ParseMode(L"auto") == Mode::Auto);
    CHECK(ParseMode(L"Auto ") == Mode::Auto);
    CHECK(ParseMode(L"0") == Mode::Off);
    CHECK(ParseMode(L"off") == Mode::Off);
    CHECK(ParseMode(L"OFF") == Mode::Off);
    CHECK(ParseMode(L"1") == Mode::On);
    CHECK(ParseMode(L" on") == Mode::On);
    CHECK(ParseMode(L"true") == Mode::On);
    CHECK(ParseMode(L"False") == Mode::Off);
    CHECK(ParseMode(L"maybe") == Mode::Auto);
    // Read as a number before: the leading integer, non-zero is on.
    CHECK(ParseMode(L"2") == Mode::On);
    CHECK(ParseMode(L"01") == Mode::On);
    CHECK(ParseMode(L"00") == Mode::Off);
    CHECK(ParseMode(L"-1") == Mode::On);
    CHECK(ParseMode(L"+0") == Mode::Off);
    CHECK(ParseMode(L"0 ; x") == Mode::Off);
    CHECK(ParseMode(L"1 ; x") == Mode::On);
    CHECK(ParseMode(L"auto ; the default") == Mode::Auto);
    CHECK(ParseMode(L"off;x") == Mode::Off);

    CHECK(ApplyCommandLine(Mode::Auto, false, false) == Mode::Auto);
    CHECK(ApplyCommandLine(Mode::Off, true, false) == Mode::On);
    CHECK(ApplyCommandLine(Mode::On, false, true) == Mode::Off);
    CHECK(ApplyCommandLine(Mode::Auto, true, true) == Mode::Off);
}

TEST_CASE("Auto uses half the hardware threads less two, 1 to 4, and none below 4 threads [animation][pool]")
{
    using namespace Engine::Object::AnimationThreads;
    CHECK(WorkerCount(Mode::Off, 16) == 0);
    CHECK(WorkerCount(Mode::Auto, 0) == 0);
    CHECK(WorkerCount(Mode::Auto, 2) == 0);
    CHECK(WorkerCount(Mode::Auto, 3) == 0);
    CHECK(WorkerCount(Mode::Auto, 4) == 1);
    CHECK(WorkerCount(Mode::Auto, 8) == 2);
    CHECK(WorkerCount(Mode::Auto, 12) == 4);
    CHECK(WorkerCount(Mode::Auto, 32) == 4);
    CHECK(WorkerCount(Mode::On, 0) == 1);
    CHECK(WorkerCount(Mode::On, 2) == 1);
    CHECK(WorkerCount(Mode::On, 8) == 2);
}
