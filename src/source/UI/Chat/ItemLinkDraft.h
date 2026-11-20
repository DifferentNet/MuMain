#pragma once

#include "UI/Chat/ChatItemLink.h"
#include "UI/Chat/ItemLinkTokens.h"
#include "UI/Chat/ItemLinkWire.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace UI::Chat
{
// MU Moldova, items shown in the chat: the chat line's items while the player types (UI/HUD/ChatInputBox), each
// token's place in the field's text (UI/Chat/ItemLinkTokens.h), the item it shows and the ref "/il" names it by.
class ItemLinkDraft
{
public:
    struct Item
    {
        // "[Name +13]" as the field shows it.
        std::wstring token;
        // "i37:6153:13" (ItemLinkWire::ItemRef).
        std::wstring ref;
        std::shared_ptr<ITEM> item;
        LinkRarity rarity = LinkRarity::Normal;
    };

    enum class Refusal
    {
        None,
        TooManyItems,
        TooLong,
    };

    bool Empty() const { return m_tokens.empty(); }
    std::size_t Count() const { return m_tokens.size(); }
    void Clear();

    // Why `item` cannot go into a field holding `fieldText` with at most `maxItems` items and `maxChars` characters
    // (a token counted as long as it shows); None when it can.
    Refusal CanInsert(std::wstring_view fieldText, const Item& item, int maxItems, std::size_t maxChars) const;
    // Puts the item's token into `text` (the field's text) at the caret, never inside another token; returns the
    // caret after it.
    std::size_t Insert(std::wstring& text, std::size_t caret, Item item);
    // Follows the field after the player changed it (caret as an index into `text`). A fix is what the field must
    // show instead: a token went out whole.
    std::optional<ItemLinkTokens::Fix> Follow(const std::wstring& text, std::size_t caret);
    // The text the tokens stand in.
    const std::wstring& Text() const { return m_text; }

    // The tokens for "/il", in the order they stand.
    std::vector<ItemLinkWire::PlacedRef> Placed() const;
    // The items as links of `shown` (a chat log line holding the typed text), each token found after the one
    // before it; a token not found there gives no link.
    std::vector<ChatItemLink> LinksIn(std::wstring_view shown) const;

private:
    const Item* ItemOf(int key) const;
    void ForgetUnused();

    std::vector<ItemLinkTokens::Token> m_tokens;
    std::vector<std::pair<int, Item>> m_items;
    std::wstring m_text;
    int m_nextKey = 0;
};
} // namespace UI::Chat
