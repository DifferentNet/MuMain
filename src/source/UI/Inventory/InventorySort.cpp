#include "stdafx.h"
#include "UI/Inventory/InventorySort.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInventory.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/InventoryCtrl.h"
#include "UI/Inventory/MyShopInventory.h"

#include <string_view>

namespace UI::Inventory
{
namespace
{
constexpr std::wstring_view SortAnswer = L"#SRT|";
constexpr const wchar_t* BagCommand = L"/sort bag";
constexpr const wchar_t* VaultCommand = L"/sort vault";
// A sort without an answer this long is given up, so the buttons work again.
constexpr auto AnswerTimeout = std::chrono::seconds(4);

using mu::ui::window::INTERFACE_LIST;

// Windows that move items while open: no sort then, whichever grid it is.
constexpr INTERFACE_LIST ItemMovingWindows[] = {
    mu::ui::window::INTERFACE_TRADE,
    mu::ui::window::INTERFACE_MYSHOP_INVENTORY,
    mu::ui::window::INTERFACE_PURCHASESHOP_INVENTORY,
    mu::ui::window::INTERFACE_MIXINVENTORY,
    mu::ui::window::INTERFACE_LUCKYITEMWND,
};

// Windows an NPC opens (the vault aside, which sorts its own way) and the jewel bank, which takes jewels from the
// bag: no bag sort while one is open.
constexpr INTERFACE_LIST BagBusyWindows[] = {
    mu::ui::window::INTERFACE_JEWEL_BANK,
    mu::ui::window::INTERFACE_NPCSHOP,
    mu::ui::window::INTERFACE_NPCQUEST,
    mu::ui::window::INTERFACE_NPCBREEDER,
    mu::ui::window::INTERFACE_DEVILSQUARE,
    mu::ui::window::INTERFACE_BLOODCASTLE,
    mu::ui::window::INTERFACE_NPCGUILDMASTER,
    mu::ui::window::INTERFACE_GUARDSMAN,
    mu::ui::window::INTERFACE_SENATUS,
    mu::ui::window::INTERFACE_GATEKEEPER,
    mu::ui::window::INTERFACE_CATAPULT,
    mu::ui::window::INTERFACE_REFINERY,
    mu::ui::window::INTERFACE_KANTURU2ND_ENTERNPC,
    mu::ui::window::INTERFACE_CURSEDTEMPLE_NPC,
    mu::ui::window::INTERFACE_GOLD_BOWMAN,
    mu::ui::window::INTERFACE_GOLD_BOWMAN_LENA,
    mu::ui::window::INTERFACE_LUCKYCOIN_REGISTRATION,
    mu::ui::window::INTERFACE_EXCHANGE_LUCKYCOIN,
    mu::ui::window::INTERFACE_DOPPELGANGER_NPC,
    mu::ui::window::INTERFACE_EMPIREGUARDIAN_NPC,
    mu::ui::window::INTERFACE_NPC_DIALOGUE,
    mu::ui::window::INTERFACE_UNITEDMARKETPLACE_NPC_JULIA,
    mu::ui::window::INTERFACE_TIER_MAPS,
    mu::ui::window::INTERFACE_AUCTION,
    mu::ui::window::INTERFACE_ENDLESS_TOWER,
    mu::ui::window::INTERFACE_RENA_SHOP,
    mu::ui::window::INTERFACE_GRAND_SHOP,
};

template <size_t N>
bool AnyVisible(const INTERFACE_LIST (&windows)[N])
{
    for (const INTERFACE_LIST window : windows)
    {
        if (g_pNewUISystem->IsVisible(window))
            return true;
    }
    return false;
}

void SendCommand(const wchar_t* command)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(command));
}
} // namespace

InventorySort& InventorySort::Instance()
{
    static InventorySort sort;
    return sort;
}

bool InventorySort::TakeMessage(const std::wstring& text)
{
    if (!std::wstring_view(text).starts_with(SortAnswer))
        return false;

    // "ok|bag", "no|vault": either way the sort is over. The items come in their own packets, and a refusal's
    // reason came as the chat line before.
    m_askedAt = {};
    return true;
}

void InventorySort::Request(Target target)
{
    if (IsBlocked(target))
        return;

    m_askedAt = std::chrono::steady_clock::now();
    PlayBuffer(SOUND_CLICK01);
    SendCommand(target == Target::Bag ? BagCommand : VaultCommand);
}

bool InventorySort::IsWaiting() const
{
    return m_askedAt != std::chrono::steady_clock::time_point{}
        && std::chrono::steady_clock::now() - m_askedAt < AnswerTimeout;
}

bool InventorySort::IsBlocked(Target target) const
{
    if (IsWaiting() || mu::ui::window::CInventoryCtrl::GetPickedItem() != nullptr || IsItemMovePending()
        || g_pNewUISystem == nullptr)
        return true;

    const bool shopSelling = g_pMyShopInventory != nullptr && g_pMyShopInventory->IsEnablePersonalShop();
    if (shopSelling || AnyVisible(ItemMovingWindows))
        return true;

    return target == Target::Bag && AnyVisible(BagBusyWindows);
}

void InventorySort::Reset()
{
    m_askedAt = {};
}

namespace
{
// The sort answers stay out of the chat (Network/Server/HiddenChatLines.h); a new world entry forgets a waiting one.
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"InventorySort",
    .prefixes = {SortAnswer},
    .take = [](const std::wstring& text) { return Sort().TakeMessage(text); },
    .enteredWorld = [] { Sort().Reset(); },
});
} // namespace
} // namespace UI::Inventory
