#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace Engine::Monsters
{
    // How much one map's preload may do.
    struct MonsterPreloadLimits
    {
        // Time of the run under the loading screen; what is left goes one type a frame.
        double syncBudgetMs = 0.0;
        // Types a map; the ones past it stay to load when they first show.
        int maxTypes = 0;
        // Texture memory the preload may add; after it the rest stays to load when it first shows.
        std::uint64_t maxTextureBytes = 0;
    };

    enum class WarmUpResult
    {
        Warmed,
        Skipped,
    };

    // What one map's preload has done so far.
    struct MonsterPreloadStats
    {
        int warmed = 0;
        int skipped = 0;
        double elapsedMs = 0.0;
        std::uint64_t textureBytes = 0;
        // Still queued for the frames that follow.
        int left = 0;
        // Types past maxTypes, never queued.
        int overLimit = 0;
        bool textureLimitReached = false;
    };

    // The queue of one map's monster types to warm up, with the time, type and texture limits. The warm-up,
    // the clock (milliseconds) and the texture memory reading (bytes) are handed in, so the rules can be
    // tested without the game.
    class MonsterPreloadQueue
    {
    public:
        using WarmUpFunction = std::function<WarmUpResult(int type)>;
        using ClockFunction = std::function<double()>;
        using TextureMemoryFunction = std::function<std::uint64_t()>;

        MonsterPreloadQueue(WarmUpFunction warmUp, ClockFunction clock, TextureMemoryFunction textureMemory,
                            MonsterPreloadLimits limits);

        // Starts over with these types; the first maxTypes of them are queued.
        void Begin(std::vector<int> types);
        // Warms types until the queue is empty, a limit is reached or budgetMs has passed (checked before
        // each type, so the last one may run past it).
        const MonsterPreloadStats& RunFor(double budgetMs);
        // Warms one type, if any is left and the texture limit allows.
        const MonsterPreloadStats& RunOne();
        // Forgets what is left.
        void Clear();

        bool HasPending() const { return m_next < m_types.size(); }
        const MonsterPreloadStats& Stats() const { return m_stats; }
        const MonsterPreloadLimits& Limits() const { return m_limits; }

    private:
        bool TextureLimitReached();
        void WarmNext();
        void UpdateLeft();

        WarmUpFunction m_warmUp;
        ClockFunction m_clock;
        TextureMemoryFunction m_textureMemory;
        MonsterPreloadLimits m_limits;

        std::vector<int> m_types;
        std::size_t m_next = 0;
        std::uint64_t m_textureMemoryAtBegin = 0;
        MonsterPreloadStats m_stats;
    };
}
