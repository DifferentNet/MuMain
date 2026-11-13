#pragma once

#include <chrono>
#include <string>

// Sorting the bag and the vault, from our server's add-on (MU Moldova, ghinzuro 2026-11-13): the sort button on
// the bag window (beside its page tabs) and on the vault window (beside its page arrows) asks the server, which
// sorts and sends the items back.
//
// A button sends "/sort bag" or "/sort vault". The server answers "#SRT|ok|bag" (or vault), or, when it refuses,
// a chat line with the reason and then "#SRT|no|bag". The items come by themselves: the bag as the whole
// inventory list (as on entering the world), the vault as "#VLS|page|pages" and the page's items
// (UI/Inventory/VaultPages.h). A server without the add-on never answers, and the button works again after a few
// seconds.
namespace UI::Inventory
{
class InventorySort
{
public:
    enum class Target
    {
        Bag,
        Vault,
    };

    static InventorySort& Instance();

    // Takes a server message. True when it was a sort line, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server to sort `target`. Nothing happens while IsBlocked(target).
    void Request(Target target);

    // A sort was asked for and its answer hasn't come yet (for a few seconds at most). Meanwhile no item of the bag
    // or the vault is lifted or auto-moved: the server is moving them.
    bool IsWaiting() const;

    // The button of `target` is dimmed: a sort waits for its answer, an item is on the cursor or its move waits for
    // the server, or a window that moves items is open (a trade, the personal shop, the chaos machine or another
    // mix); for the bag also any NPC's window other than the vault, and the jewel bank.
    bool IsBlocked(Target target) const;

    // Forgets a waiting sort, on entering the world.
    void Reset();

private:
    std::chrono::steady_clock::time_point m_askedAt{};
};

inline InventorySort& Sort()
{
    return InventorySort::Instance();
}
} // namespace UI::Inventory
