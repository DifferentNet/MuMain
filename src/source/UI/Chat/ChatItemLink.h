#pragma once

#include <cstddef>
#include <memory>

typedef struct tagITEM ITEM;

// MU Moldova, items shown in the chat (UI/Chat/ItemLinks.h): an item's "[name]" in a chat log row.
namespace UI::Chat
{
// The name's colour, as the item tooltip colours names (ancient, socket, excellent, high level, the rest).
enum class LinkRarity
{
    Normal,
    High,
    Excellent,
    Socket,
    Ancient,
};

// The slug the chat log's RCSS builds .chat-link--<slug> from.
inline const char* LinkRaritySlug(LinkRarity rarity)
{
    switch (rarity)
    {
    case LinkRarity::High:
        return "high";
    case LinkRarity::Excellent:
        return "excellent";
    case LinkRarity::Socket:
        return "socket";
    case LinkRarity::Ancient:
        return "ancient";
    default:
        return "normal";
    }
}

struct ChatItemLink
{
    // The "[name]" in the row's text.
    std::size_t start = 0;
    std::size_t length = 0;
    // The item the tooltip shows; rows of one line share it.
    std::shared_ptr<ITEM> item;
    LinkRarity rarity = LinkRarity::Normal;
};
} // namespace UI::Chat
