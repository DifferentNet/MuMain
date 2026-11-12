#pragma once

#include <chrono>
#include <cstdint>
#include <string>

// The bag's pages, from our server's add-on (MU Moldova, BagPages.cs, ghinzuro 2026-11-08): page 2 is OpenMU's
// inventory extensions 1+2 (slots 76-139, two 8x4 grids), bought once per character for 2,000,000,000 zen, taken
// from the bag first and the rest from the vault.
//
// The bag window asks "/bag" when it first opens after entering the world; the server answers
// "#BGP|pages|max|price". Buying sends "/bag buy"; the server answers with a chat line, then "#BGB|ok" or "#BGB|no",
// and on success a new "#BGP". A server without the add-on never answers, and the window then shows no page tabs.
namespace UI::Inventory
{
class BagPages
{
public:
    static BagPages& Instance();

    // Takes a server message. True when it was a bag page line, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server for the pages once per world entry.
    void RequestInfoOnce();

    // Sends "/bag buy". Nothing happens while an earlier purchase waits for its answer.
    void Buy();

    bool HasInfo() const { return m_maxPages > 0; }
    int MaxPages() const { return m_maxPages; }
    // The pages the character has, from its extensions (CharacterAttribute->InventoryExtensions).
    int OwnedPages() const;
    std::int64_t Price() const { return m_price; }

    // A purchase was asked for and its answer hasn't come yet (for a few seconds at most).
    bool IsBuying() const;

    // Forgets the info, on entering the world.
    void Reset();

private:
    int m_maxPages = 0;
    std::int64_t m_price = 0;
    bool m_asked = false;
    std::chrono::steady_clock::time_point m_buyAskedAt{};
};

inline BagPages& Bag()
{
    return BagPages::Instance();
}
} // namespace UI::Inventory
