#include "stdafx.h"

#include "Engine/Monsters/MonsterPreloadQueue.h"

#include <algorithm>
#include <utility>

namespace Engine::Monsters
{
    MonsterPreloadQueue::MonsterPreloadQueue(WarmUpFunction warmUp, ClockFunction clock,
                                             TextureMemoryFunction textureMemory, MonsterPreloadLimits limits)
        : m_warmUp(std::move(warmUp)), m_clock(std::move(clock)), m_textureMemory(std::move(textureMemory)),
          m_limits(limits)
    {
    }

    void MonsterPreloadQueue::Begin(std::vector<int> types)
    {
        m_stats = {};
        const std::size_t maxTypes = static_cast<std::size_t>(std::max(m_limits.maxTypes, 0));
        if (types.size() > maxTypes)
        {
            m_stats.overLimit = static_cast<int>(types.size() - maxTypes);
            types.resize(maxTypes);
        }
        m_types = std::move(types);
        m_next = 0;
        m_textureMemoryAtBegin = m_textureMemory();
        UpdateLeft();
    }

    const MonsterPreloadStats& MonsterPreloadQueue::RunFor(double budgetMs)
    {
        const double start = m_clock();
        while (HasPending() && m_clock() - start < budgetMs && !TextureLimitReached())
        {
            WarmNext();
        }
        return m_stats;
    }

    const MonsterPreloadStats& MonsterPreloadQueue::RunOne()
    {
        if (HasPending() && !TextureLimitReached())
        {
            WarmNext();
        }
        return m_stats;
    }

    void MonsterPreloadQueue::Clear()
    {
        m_next = m_types.size();
        UpdateLeft();
    }

    bool MonsterPreloadQueue::TextureLimitReached()
    {
        if (m_stats.textureBytes < m_limits.maxTextureBytes)
        {
            return false;
        }
        // Once over the limit nothing more is warmed: the rest loads when it first shows.
        m_stats.textureLimitReached = true;
        Clear();
        return true;
    }

    void MonsterPreloadQueue::WarmNext()
    {
        const int type = m_types[m_next++];
        const double start = m_clock();
        const WarmUpResult result = m_warmUp(type);
        m_stats.elapsedMs += m_clock() - start;
        if (result == WarmUpResult::Warmed)
        {
            ++m_stats.warmed;
        }
        else
        {
            ++m_stats.skipped;
        }

        // Texture memory also falls (a texture released meanwhile); the preload never counts below nothing.
        const std::uint64_t now = m_textureMemory();
        m_stats.textureBytes = now > m_textureMemoryAtBegin ? now - m_textureMemoryAtBegin : 0;
        UpdateLeft();
    }

    void MonsterPreloadQueue::UpdateLeft()
    {
        m_stats.left = static_cast<int>(m_types.size() - std::min(m_next, m_types.size()));
    }
}
