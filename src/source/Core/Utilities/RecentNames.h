#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Core::Utilities
{
// A list of distinct names, the most recent last, holding at most `capacity` of them: a newer name pushes
// the oldest out. Kept as text, one name a line (Join, Load). The browser client's learned texture warm
// list (Render/Sprites/TextureWarmUp) keeps the deferred textures a player needed this way.
class RecentNames
{
public:
    explicit RecentNames(std::size_t capacity);

    // Puts `name` last, dropping the oldest names beyond the capacity. False for an empty name and for a
    // name that is last already (nothing changed).
    bool Note(std::string_view name);

    // Oldest first.
    [[nodiscard]] const std::vector<std::string>& Names() const { return m_names; }
    [[nodiscard]] std::size_t Capacity() const { return m_capacity; }

    // One name a line, oldest first, each line ending in '\n'; empty without names.
    [[nodiscard]] std::string Join() const;
    // Replaces the names with the lines of `text` (Join's form; "\r\n" too), noted in order, so an empty
    // line is skipped, a repeated one counts where it comes last, and only the last `capacity` stay.
    void Load(std::string_view text);

private:
    std::size_t m_capacity;
    std::vector<std::string> m_names;
};
} // namespace Core::Utilities
