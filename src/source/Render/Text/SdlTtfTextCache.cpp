#include "SdlTtfTextCache.h"

#include <functional>
#include <new>
#include <utility>

namespace Render::Text
{
std::size_t SdlTtfTextCache::KeyHash::operator()(const Key& key) const noexcept
{
    const std::size_t textHash = std::hash<std::string_view>{}(key.utf8);
    const std::size_t fontHash = std::hash<const void*>{}(key.font);
    return textHash ^ (fontHash + 0x9e3779b9u + (textHash << 6) + (textHash >> 2));
}

SdlTtfTextCache::SdlTtfTextCache(TTF_TextEngine* engine, std::size_t capacity)
    : m_engine(engine), m_capacity(capacity)
{
}

SdlTtfTextCache::~SdlTtfTextCache() = default;

SdlTtfTextCache::PreparedText SdlTtfTextCache::Prepare(TTF_Font* font, std::string_view utf8)
{
    if (m_engine == nullptr || font == nullptr)
        return {};

    if (const auto found = m_index.find(Key{font, utf8}); found != m_index.end())
    {
        m_entries.splice(m_entries.begin(), m_entries, found->second);
        const Entry& entry = *found->second;
        return {entry.text.get(), entry.width, entry.height};
    }

    TextOwner text(TTF_CreateText(m_engine, font, utf8.data(), utf8.size()), TTF_DestroyText);
    if (text == nullptr)
        return {};

    int width = 0;
    int height = 0;
    if (!TTF_GetTextSize(text.get(), &width, &height))
        return {};

    if (m_capacity == 0)
    {
        PreparedText prepared{text.get(), width, height};
        prepared.transient = std::move(text);
        return prepared;
    }

    return Store(font, utf8, std::move(text), width, height);
}

SdlTtfTextCache::PreparedText SdlTtfTextCache::Store(TTF_Font* font, std::string_view utf8, TextOwner text,
                                                     int width, int height)
{
    PreparedText prepared{text.get(), width, height};
    if (m_entries.size() >= m_capacity)
        EvictLeastRecentlyUsed();

    // Everything that can throw happens before `text` is moved, so a failure still hands the label back.
    try
    {
        if (m_index.bucket_count() < m_capacity)
            m_index.reserve(m_capacity);
        std::string key(utf8);
        m_entries.emplace_front();
        m_entries.front().utf8 = std::move(key);
    }
    catch (const std::bad_alloc&)
    {
        prepared.transient = std::move(text);
        return prepared;
    }

    Entry& entry = m_entries.front();
    entry.font = font;
    entry.text = std::move(text);
    entry.width = width;
    entry.height = height;
    try
    {
        m_index.emplace(Key{entry.font, entry.utf8}, m_entries.begin());
    }
    catch (const std::bad_alloc&)
    {
        prepared.transient = std::move(entry.text);
        m_entries.pop_front();
        return prepared;
    }

    return {entry.text.get(), entry.width, entry.height};
}

void SdlTtfTextCache::EvictLeastRecentlyUsed()
{
    if (m_entries.empty())
        return;

    const Entry& oldest = m_entries.back();
    m_index.erase(Key{oldest.font, oldest.utf8});
    m_entries.pop_back();
}

void SdlTtfTextCache::Reset(TTF_TextEngine* engine)
{
    Clear();
    m_engine = engine;
}

void SdlTtfTextCache::Clear()
{
    m_index.clear();
    m_entries.clear();
}

std::size_t SdlTtfTextCache::Size() const
{
    return m_entries.size();
}
}
