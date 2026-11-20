#pragma once

#include "UI/Chat/ChatItemLink.h"
#include "UI/Chat/MessageType.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

typedef struct tagITEM ITEM;

namespace mu::ui::window
{
class CInventoryCtrl;
}

// MU Moldova, items shown in the chat (ghinzuro 2026-11-20, «Вариант 1», contract in the side-session note
// item-link-spec.md). On entering the world the client says "/ilinkhello"; a server with the feature answers
// "#ILH|…", and from then on Shift+click on an own item (the "To chat" button on a phone) puts its "[name]" into the
// chat line (UI/HUD/ChatInputBox), which then goes as "/il …". The server writes the line and sends each reader a
// hidden "#ILK|…" right before it; the chat log then shows those names as links with the item's tooltip
// (UI/HUD/ChatLogWindow). The wire format lives in UI/Chat/ItemLinkWire.h, the pure steps next to it.
namespace UI::Chat::ItemLinks
{
// The server answered "/ilinkhello" in this connection: items may go into the chat line.
bool IsOffered();
// How many items one message may show (the server's number, at most ItemLinkWire::MaxLinks).
int MaxItems();
// The whole seconds before the next message with items may go: the server takes one every cooldownSeconds of
// "#ILH" per account and refuses one sooner (ItemLinkWire::Cooldown). 0 when it may go now.
int CooldownWait();
// A message with items went to the server; `comesBack`: its line comes back to the sender (all but a whisper),
// which tells the server took it (TakeFor()).
void NoteSent(bool comesBack);
// The last message with items did not come back: the server refused it (ItemLinkWire::Cooldown::Refused()), so the
// chat bar may offer it again.
bool LastLineRefused();

// A chat line with its items: the text the log shows (each name in the reader's language) and the links in it.
struct Linked
{
    std::wstring text;
    std::vector<ChatItemLink> links;
};

// The chat line of this sender and type with the items the server announced for it; empty when none came for it
// (the line is then shown as it is).
std::optional<Linked> TakeFor(std::wstring_view sender, std::wstring_view text, mu::ui::window::MESSAGE_TYPE kind);

// The item's name as its tooltip names it ("Excellent Dragon Armor +13", an ancient one by its set).
std::wstring NameOf(const ITEM& item);
// "[name]", cut as the server cuts its own.
std::wstring TokenOf(const ITEM& item);
LinkRarity RarityOf(const ITEM& item);

// Puts an own item into the chat line: storage ItemLinkWire::InventoryStorage or VaultStorage, `slot` the server's
// slot of it. False when items cannot go into the chat now (the server never offered it).
bool ToChat(wchar_t storage, int slot, const ITEM& item);

// The click that puts an own item into the chat line: the left button comes up with Shift held and nothing on the
// pointer. The bag, the worn items and the vault take it before their grid picks the item up. Always false while
// items cannot go into the chat.
bool IsLinkClick();
// Puts the item of `grid` under the pointer into the chat line and takes the click; false when no item is there.
bool GridItemToChat(wchar_t storage, mu::ui::window::CInventoryCtrl& grid);
// The item card's "To chat" (UI/Mobile/ItemCard.h): the item of `grid` with this key, found again by it, as the card
// stays open while the grid may change. False when it is gone.
bool GridItemToChatByKey(wchar_t storage, mu::ui::window::CInventoryCtrl& grid, DWORD key);
} // namespace UI::Chat::ItemLinks
