#include "UI/Chat/ItemLinkDraft.h"

#include <algorithm>

namespace UI::Chat
{
void ItemLinkDraft::Clear()
{
    m_tokens.clear();
    m_items.clear();
    m_text.clear();
}

ItemLinkDraft::Refusal ItemLinkDraft::CanInsert(std::wstring_view fieldText, const Item& item, int maxItems) const
{
    if (static_cast<int>(m_tokens.size()) >= maxItems)
        return Refusal::TooManyItems;
    const std::size_t shown = ItemLinkTokens::CharacterOfIndex(fieldText, fieldText.size());
    const std::size_t added = ItemLinkTokens::CharacterOfIndex(item.token, item.token.size());
    if (shown + added > m_maxChars)
        return Refusal::TooLong;
    return Refusal::None;
}

std::size_t ItemLinkDraft::Insert(std::wstring& text, std::size_t caret, Item item)
{
    if (m_tokens.empty())
        m_text = text;
    const std::size_t at = ItemLinkTokens::InsertPoint(m_tokens, std::min(caret, text.size()));
    const std::size_t length = item.token.size();
    text.insert(at, item.token);
    const int key = m_nextKey++;
    ItemLinkTokens::AddToken(m_tokens, at, length, key);
    m_items.emplace_back(key, std::move(item));
    m_text = text;
    return at + length;
}

std::optional<ItemLinkTokens::Fix> ItemLinkDraft::Follow(const std::wstring& text, std::size_t caret)
{
    if (text == m_text)
        return std::nullopt;
    auto fix = ItemLinkTokens::ApplyEdit(m_text, text, caret, m_tokens);
    m_text = fix ? fix->text : text;
    ForgetUnused();
    return fix;
}

std::vector<ItemLinkWire::PlacedRef> ItemLinkDraft::Placed() const
{
    std::vector<ItemLinkWire::PlacedRef> placed;
    placed.reserve(m_tokens.size());
    for (const ItemLinkTokens::Token& token : m_tokens)
    {
        if (const Item* item = ItemOf(token.key))
            placed.push_back({token.start, token.length, item->ref});
    }
    return placed;
}

std::vector<ChatItemLink> ItemLinkDraft::LinksIn(std::wstring_view shown) const
{
    std::vector<ChatItemLink> links;
    std::size_t from = 0;
    for (const ItemLinkTokens::Token& token : m_tokens)
    {
        const Item* item = ItemOf(token.key);
        if (item == nullptr || item->token.empty())
            continue;
        const std::size_t at = shown.find(item->token, from);
        if (at == std::wstring_view::npos)
            continue;
        links.push_back({at, item->token.size(), item->item, item->rarity});
        from = at + item->token.size();
    }
    return links;
}

const ItemLinkDraft::Item* ItemLinkDraft::ItemOf(int key) const
{
    const auto found = std::find_if(m_items.begin(), m_items.end(), [key](const auto& entry) { return entry.first == key; });
    return found != m_items.end() ? &found->second : nullptr;
}

void ItemLinkDraft::ForgetUnused()
{
    std::erase_if(m_items, [this](const auto& entry)
                  {
                      return std::none_of(m_tokens.begin(), m_tokens.end(),
                                          [&entry](const ItemLinkTokens::Token& token) { return token.key == entry.first; });
                  });
    if (m_tokens.empty())
        m_text.clear();
}
} // namespace UI::Chat
