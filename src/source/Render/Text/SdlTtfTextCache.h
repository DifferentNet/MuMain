#pragma once

#include <SDL3_ttf/SDL_ttf.h>

#include <cstddef>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Render::Text
{
class SdlTtfTextCache final
{
public:
    using TextOwner = std::unique_ptr<TTF_Text, decltype(&TTF_DestroyText)>;

    struct PreparedText
    {
        TTF_Text* text = nullptr;
        int width = 0;
        int height = 0;
        TextOwner transient{nullptr, TTF_DestroyText};
    };

    // ponytail: 256 retained labels; raise only if post-warmup layout profiling still shows churn.
    explicit SdlTtfTextCache(TTF_TextEngine* engine = nullptr, std::size_t capacity = 256);
    ~SdlTtfTextCache();

    PreparedText Prepare(TTF_Font* font, std::string_view utf8);
    void Reset(TTF_TextEngine* engine);
    void Clear();
    [[nodiscard]] std::size_t Size() const;

private:
    struct Entry
    {
        TTF_Font* font = nullptr;
        std::string utf8;
        TextOwner text{nullptr, TTF_DestroyText};
        int width = 0;
        int height = 0;
    };

    // The view points into the list entry's own string; list nodes never move, so the view stays valid.
    struct Key
    {
        TTF_Font* font = nullptr;
        std::string_view utf8;

        bool operator==(const Key& other) const noexcept { return font == other.font && utf8 == other.utf8; }
    };

    struct KeyHash
    {
        std::size_t operator()(const Key& key) const noexcept;
    };

    using EntryList = std::list<Entry>;

    PreparedText Store(TTF_Font* font, std::string_view utf8, TextOwner text, int width, int height);
    void EvictLeastRecentlyUsed();

    TTF_TextEngine* m_engine = nullptr;
    std::size_t m_capacity = 0;
    // Most recently used first; the back is evicted when the cache is full.
    EntryList m_entries;
    std::unordered_map<Key, EntryList::iterator, KeyHash> m_index;
};
}
