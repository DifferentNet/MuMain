#pragma once

// Web build: the sounds the game asked MiniAudioBackend to load while their files were still on their way
// (WebFileSystem.h WaitsForDeferredPart: the startup pack's sounds come after the game started). Each
// waits here, first asked first loaded, until the backend loads it a few a frame. A slot holds the last
// sound asked for: loading it again replaces its waiting load, releasing it drops it.
// Header-only, so builds without the web's waits compile nothing of it.

#include <deque>
#include <optional>
#include <string>
#include <utility>

namespace mu
{

struct WaitingSoundLoad
{
    int slot = 0;
    std::wstring file; // as the game named it, for the log
    std::string path;  // as the backend opens it
    int channels = 0;
    bool enable3D = false;
};

class WaitingSoundLoads
{
public:
    // Waits to load `load`, in place of a load waiting for the same slot.
    void Add(WaitingSoundLoad load)
    {
        Remove(load.slot);
        m_loads.push_back(std::move(load));
    }

    // Drops the load waiting for `slot`, if any; true where one waited.
    bool Remove(int slot)
    {
        return std::erase_if(m_loads, [slot](const WaitingSoundLoad& load) { return load.slot == slot; }) > 0;
    }

    // The load asked for first, or null without any.
    [[nodiscard]] const WaitingSoundLoad* First() const
    {
        return m_loads.empty() ? nullptr : &m_loads.front();
    }

    // Takes the load asked for first; nothing without any.
    std::optional<WaitingSoundLoad> TakeFirst()
    {
        return TakeFirstReady([](const WaitingSoundLoad&) { return true; });
    }

    // Takes the load asked for first of those `ready` accepts, the others keep waiting in their order;
    // nothing where it accepts none.
    template <typename Ready>
    std::optional<WaitingSoundLoad> TakeFirstReady(Ready ready)
    {
        for (auto it = m_loads.begin(); it != m_loads.end(); ++it)
        {
            if (ready(*it))
            {
                WaitingSoundLoad load = std::move(*it);
                m_loads.erase(it);
                return load;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool Empty() const
    {
        return m_loads.empty();
    }

    [[nodiscard]] std::size_t Size() const
    {
        return m_loads.size();
    }

    void Clear()
    {
        m_loads.clear();
    }

private:
    std::deque<WaitingSoundLoad> m_loads;
};

} // namespace mu
