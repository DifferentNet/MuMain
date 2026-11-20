#include "stdafx.h"

#include "UI/Chat/ItemLinks.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/CSItemOption.h"
#include "I18N/All.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/SocketSystem.h"
#include "Network/Server/WSclient.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "UI/Chat/ItemLinkAttach.h"
#include "UI/Chat/ItemLinkDraft.h"
#include "UI/Chat/ItemLinkPending.h"
#include "UI/Chat/ItemLinkWire.h"
#include "UI/Core/WindowCommon.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/InventoryCtrl.h"
#include "UI/Inventory/ItemFromData.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace UI::Chat::ItemLinks
{
namespace
{
// The tooltip names gear of this level and above in gold.
constexpr int HighLevel = 7;
// Wings, capes and the other items of this group show their excellent options without the word.
constexpr int WingsGroup = 12;
// The groups up to this one are gear, which an ancient set's name names without the level.
constexpr int LastGearGroup = 12;
// The excellent option bits of ITEM::ExcellentFlags.
constexpr int ExcellentOptionBits = 63;
constexpr std::size_t NameBufferSize = 256;

struct State
{
    bool offered = false;
    int maxItems = 0;
    int cooldownSeconds = 0;
    // Kept over a new character, as the server's cooldown is the account's.
    ItemLinkWire::Cooldown cooldown;
    ItemLinkPending pending;
};

State& Links()
{
    static State state;
    return state;
}

bool IsExcellent(const ITEM& item)
{
    return item.Type / MAX_ITEM_INDEX != WingsGroup && (item.ExcellentFlags & ExcellentOptionBits) != 0;
}

bool IsAncient(const ITEM& item)
{
    return item.AncientDiscriminator != 0;
}

void SendCommand(std::wstring_view command)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    const std::wstring text(command);
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(text.c_str()));
}

void TakeHello(const std::wstring& text)
{
    const auto hello = ItemLinkWire::ParseHello(text);
    if (!hello)
        return;
    State& state = Links();
    state.maxItems = std::clamp(hello->maxItems, 0, ItemLinkWire::MaxLinks);
    state.cooldownSeconds = std::max(hello->cooldownSeconds, 0);
    state.offered = state.maxItems > 0;
}

// The item lines stay out of the chat (Network/Server/HiddenChatLines.h), a broken one too.
bool TakeMessage(const std::wstring& text)
{
    const std::wstring_view line(text);
    if (line.starts_with(ItemLinkWire::HelloTag))
    {
        TakeHello(text);
        return true;
    }
    if (!line.starts_with(ItemLinkWire::LinksTag))
        return false;
    if (auto links = ItemLinkWire::ParseLinks(line))
        Links().pending.Put(std::move(*links), GetTickCount64());
    return true;
}

// A new character: forget the last connection's answer and ask again.
void ForgetAndHello()
{
    State& state = Links();
    state.offered = false;
    state.maxItems = 0;
    state.cooldownSeconds = 0;
    state.pending.Clear();
    SendCommand(ItemLinkWire::HelloCommand);
}

[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"ItemLinks",
    .prefixes = {ItemLinkWire::HelloTag, ItemLinkWire::LinksTag},
    .take = TakeMessage,
    .enteredWorld = ForgetAndHello,
});

// The items of the line's links, by link; null where a link has no place in the text or its bytes are no item.
std::vector<std::shared_ptr<ITEM>> ItemsOf(const std::vector<ItemLinkWire::WireLink>& wire,
                                           std::vector<std::optional<ItemLinkAttach::Span>>& spans)
{
    std::vector<std::shared_ptr<ITEM>> items(wire.size());
    for (std::size_t i = 0; i < wire.size(); ++i)
    {
        if (!spans[i])
            continue;
        items[i] = UI::Items::FromData::Make(wire[i].item);
        if (!items[i])
            spans[i].reset();
    }
    return items;
}

std::optional<Linked> Build(std::wstring_view text, const std::vector<ItemLinkWire::WireLink>& wire)
{
    auto spans = ItemLinkAttach::Attach(text, wire);
    const auto items = ItemsOf(wire, spans);
    std::vector<std::wstring> names(wire.size());
    for (std::size_t i = 0; i < wire.size(); ++i)
    {
        if (items[i])
            names[i] = TokenOf(*items[i]);
    }

    ItemLinkAttach::Renamed renamed = ItemLinkAttach::Rename(text, spans, names);
    Linked linked{std::move(renamed.text), {}};
    for (std::size_t i = 0; i < wire.size(); ++i)
    {
        if (renamed.spans[i] && items[i])
            linked.links.push_back({renamed.spans[i]->start, renamed.spans[i]->length, items[i], RarityOf(*items[i])});
    }
    if (linked.links.empty())
        return std::nullopt;
    std::sort(linked.links.begin(), linked.links.end(),
              [](const ChatItemLink& a, const ChatItemLink& b) { return a.start < b.start; });
    return linked;
}

// As the bag takes a click it used (UI/Inventory/MyInventory.cpp ResetMouseLButton).
void TakeLinkClick()
{
    MouseLButton = false;
    MouseLButtonPop = false;
    MouseLButtonPush = false;
}
} // namespace

bool IsOffered()
{
    return Links().offered;
}

int MaxItems()
{
    return Links().maxItems;
}

int CooldownWait()
{
    const State& state = Links();
    return state.cooldown.Wait(GetTickCount64(), state.cooldownSeconds);
}

void NoteSent(wchar_t channel)
{
    Links().cooldown.Sent(GetTickCount64(), channel);
}

bool LastLineRefused()
{
    return Links().cooldown.Refused(GetTickCount64());
}

std::optional<Linked> TakeFor(std::wstring_view sender, std::wstring_view text, mu::ui::window::MESSAGE_TYPE kind)
{
    State& state = Links();
    const auto channel = ItemLinkWire::ChannelOfLine(sender, kind);
    if (!channel)
        return std::nullopt;
    // The own line with an item's name answers for the message with items, also when it comes without them; a plain
    // own line sent meanwhile does not.
    if (Hero != nullptr && ItemLinkWire::IsOwnLine(sender, text, *channel, Hero->ID)
        && ItemLinkWire::HoldsItemName(text))
        state.cooldown.OwnLineBack(GetTickCount64(), *channel);
    if (state.pending.Waiting() == 0)
        return std::nullopt;
    const auto line = state.pending.Take(sender, *channel, GetTickCount64());
    if (!line)
        return std::nullopt;
    return Build(text, line->links);
}

// As the item tooltip and the auction name it (UI/Inventory/AuctionWindow.cpp ItemTitle): GetItemName already
// names the level; an ancient set's name comes without it.
std::wstring NameOf(const ITEM& item)
{
    const int level = static_cast<int>(item.Level);
    wchar_t name[NameBufferSize]{};
    if (IsAncient(item)
        && g_csItemOption.GetAncientItemName(name, std::size(name), item.Type, item.AncientDiscriminator))
    {
        const bool gear = item.Type / MAX_ITEM_INDEX <= LastGearGroup;
        return gear && level > 0 ? std::wstring(name) + L" +" + std::to_wstring(level) : std::wstring(name);
    }
    GetItemName(item.Type, level, name);
    if (IsExcellent(item))
        return std::wstring(I18N::Game::Excellent) + L" " + name;
    return std::wstring(name);
}

std::wstring TokenOf(const ITEM& item)
{
    return L"[" + ItemLinkWire::ClipName(NameOf(item)) + L"]";
}

// In the order the tooltip picks the name's colour.
LinkRarity RarityOf(const ITEM& item)
{
    if (IsAncient(item))
        return LinkRarity::Ancient;
    if (g_SocketItemMgr.IsSocketItem(&item))
        return LinkRarity::Socket;
    if (IsExcellent(item))
        return LinkRarity::Excellent;
    if (static_cast<int>(item.Level) >= HighLevel)
        return LinkRarity::High;
    return LinkRarity::Normal;
}

bool ToChat(wchar_t storage, int slot, const ITEM& item)
{
    if (!IsOffered() || g_pChatInputBox == nullptr)
        return false;

    ItemLinkDraft::Item link{
        TokenOf(item),
        ItemLinkWire::ItemRef(storage, slot, item.Type, static_cast<int>(item.Level)),
        std::make_shared<ITEM>(item),
        RarityOf(item),
    };
    return g_pChatInputBox->InsertItemLink(std::move(link));
}

bool IsLinkClick()
{
    return IsOffered() && mu::ui::window::IsRepeat(VK_SHIFT) && mu::ui::window::IsRelease(VK_LBUTTON)
        && mu::ui::window::CInventoryCtrl::GetPickedItem() == nullptr;
}

bool GridItemToChat(wchar_t storage, mu::ui::window::CInventoryCtrl& grid)
{
    if (grid.IsLocked())
        return false;
    ITEM* item = grid.FindItemAtPointer();
    if (item == nullptr || !ToChat(storage, grid.GetIndexByItem(item), *item))
        return false;
    TakeLinkClick();
    return true;
}

bool GridItemToChatByKey(wchar_t storage, mu::ui::window::CInventoryCtrl& grid, DWORD key)
{
    ITEM* item = grid.FindItemByKey(key);
    return item != nullptr && ToChat(storage, grid.GetIndexByItem(item), *item);
}
} // namespace UI::Chat::ItemLinks
