#include "AnimationTaskPool.h"

#include <algorithm>
#include <cassert>

namespace
{
// Jobs a thread claims at once: few enough that the threads finish together, enough that the
// shared counter is not touched once per character.
constexpr std::size_t JobsPerClaim = 8;
constexpr int GenerationShift = 32;
constexpr std::uint64_t IndexMask = 0xFFFFFFFFull;

std::uint64_t ClaimStart(std::uint32_t generation)
{
    return static_cast<std::uint64_t>(generation) << GenerationShift;
}
} // namespace

AnimationTaskPool& AnimationTaskPool::Instance()
{
    static AnimationTaskPool instance;
    return instance;
}

void AnimationTaskPool::Initialize(std::size_t workerCount)
{
    if (m_running) return;

    m_running = true;
    // Read before any thread starts: a batch dispatched right after Initialize (the lazy start) is newer
    // than this, so the new workers take part in it instead of treating it as already seen.
    std::uint32_t startGeneration = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        startGeneration = m_batch.generation;
    }
    const std::size_t count = std::max<std::size_t>(1, workerCount);
    m_workers.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        m_workers.emplace_back(&AnimationTaskPool::WorkerLoop, this, startGeneration);
    }
}

void AnimationTaskPool::Shutdown()
{
    if (!m_running) return;

    Wait();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    m_workerCV.notify_all();

    for (std::thread& worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    m_workers.clear();
    m_stopping = false;
    m_running = false;
}

bool AnimationTaskPool::Dispatch(Job job, void* context, std::size_t count)
{
    // One batch at a time: the previous one must have been awaited.
    assert(!InFlight());
    if (!m_running) return false;
    if (count == 0) return true;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_batch.job = job;
        m_batch.context = context;
        m_batch.count = count;
        ++m_batch.generation;
        m_remaining.store(count, std::memory_order_release);
        m_claim.store(ClaimStart(m_batch.generation), std::memory_order_release);
    }
    m_workerCV.notify_all();
    return true;
}

void AnimationTaskPool::Wait()
{
    if (!InFlight()) return;

    Batch batch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        batch = m_batch;
    }
    RunClaimedJobs(batch);

    std::unique_lock<std::mutex> lock(m_mutex);
    m_waiterCV.wait(lock, [this]() { return m_remaining.load(std::memory_order_acquire) == 0; });
}

void AnimationTaskPool::WorkerLoop(std::uint32_t startGeneration)
{
    std::uint32_t seenGeneration = startGeneration;
    while (true)
    {
        Batch batch;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_workerCV.wait(lock, [&]() { return m_stopping || m_batch.generation != seenGeneration; });
            if (m_stopping) return;
            batch = m_batch;
            seenGeneration = batch.generation;
        }
        RunClaimedJobs(batch);
    }
}

void AnimationTaskPool::RunClaimedJobs(const Batch& batch)
{
    std::size_t first = 0;
    std::size_t last = 0;
    while (ClaimJobs(batch, first, last))
    {
        for (std::size_t index = first; index < last; ++index)
        {
            batch.job(batch.context, index);
        }
        FinishJobs(last - first);
    }
}

bool AnimationTaskPool::ClaimJobs(const Batch& batch, std::size_t& first, std::size_t& last)
{
    std::uint64_t claim = m_claim.load(std::memory_order_acquire);
    while (true)
    {
        const auto generation = static_cast<std::uint32_t>(claim >> GenerationShift);
        const auto index = static_cast<std::size_t>(claim & IndexMask);
        if (generation != batch.generation || index >= batch.count) return false;

        const std::size_t taken = std::min(JobsPerClaim, batch.count - index);
        if (m_claim.compare_exchange_weak(claim, claim + taken, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            first = index;
            last = index + taken;
            return true;
        }
    }
}

void AnimationTaskPool::FinishJobs(std::size_t finished)
{
    if (m_remaining.fetch_sub(finished, std::memory_order_acq_rel) != finished) return;

    // The last job of the batch: the waiter checks m_remaining under the mutex, so notify under it too.
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waiterCV.notify_all();
}
