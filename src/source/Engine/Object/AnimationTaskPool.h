#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

/**
 * @brief Worker threads that run one batch of independent jobs at a time (character bones).
 *
 * Contract: one thread (the game's main thread) calls Dispatch and then Wait before it dispatches
 * again or touches anything the jobs read or write. While a batch is in flight (InFlight()), the
 * main thread must not change an object's animation inputs (SetAction) or load a model (BMD::Open2).
 * Workers claim job indices from one shared counter, a few at a time; Wait lets the calling thread
 * claim the rest itself before it sleeps.
 */
class AnimationTaskPool
{
public:
    // Runs the job for one index of the batch; context is the caller's data. A job must not throw:
    // an exception would leave the batch unfinished (InFlight() for ever) and Wait() blocked.
    using Job = void (*)(void* context, std::size_t index);

    static AnimationTaskPool& Instance();

    // Starts workerCount worker threads (at least one). Does nothing while already running.
    void Initialize(std::size_t workerCount);

    // Stops and joins the workers. Call after the last Wait.
    void Shutdown();

    bool IsRunning() const { return m_running; }
    std::size_t GetWorkerCount() const { return m_workers.size(); }

    // Hands count jobs to the workers without waiting. Returns false (and runs nothing) when the
    // workers are not running: the caller then runs the jobs itself.
    bool Dispatch(Job job, void* context, std::size_t count);

    // Runs the batch's unclaimed jobs on the calling thread, then blocks until every job finished.
    void Wait();

    // True between Dispatch and the end of Wait.
    bool InFlight() const { return m_remaining.load(std::memory_order_acquire) > 0; }

    AnimationTaskPool() = default;
    ~AnimationTaskPool() { Shutdown(); }

    AnimationTaskPool(const AnimationTaskPool&) = delete;
    AnimationTaskPool& operator=(const AnimationTaskPool&) = delete;

private:
    struct Batch
    {
        Job job = nullptr;
        void* context = nullptr;
        std::size_t count = 0;
        std::uint32_t generation = 0;
    };

    // startGeneration: the batch generation when the thread was created; any newer batch is its work.
    void WorkerLoop(std::uint32_t startGeneration);
    // Claims and runs jobs of the batch until none are left or a newer batch replaced it.
    void RunClaimedJobs(const Batch& batch);
    bool ClaimJobs(const Batch& batch, std::size_t& first, std::size_t& last);
    void FinishJobs(std::size_t finished);

    std::vector<std::thread> m_workers;
    bool m_running = false;

    // Guards m_batch, m_stopping and the two condition variables' predicates.
    std::mutex m_mutex;
    std::condition_variable m_workerCV;
    std::condition_variable m_waiterCV;
    Batch m_batch;
    bool m_stopping = false;

    // Next unclaimed index in the low 32 bits, the batch generation in the high 32 bits, so a worker
    // still holding an older batch can never claim an index of a newer one.
    std::atomic<std::uint64_t> m_claim{ 0 };
    // Jobs of the current batch not finished yet.
    std::atomic<std::size_t> m_remaining{ 0 };
};
