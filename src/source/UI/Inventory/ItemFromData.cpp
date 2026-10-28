#include "stdafx.h"

#include "UI/Inventory/ItemFromData.h"

#include "Engine/Object/ZzzInfomation.h"
#include "Network/Server/SocketSystem.h"
#include "UI/Inventory/ItemMng.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace
{
// The shortest item is 5 bytes (group and number, level, durability, flags); the longest format takes 32. The
// optional bytes follow the format's flags, so a shorter item is padded with zeros.
constexpr size_t MinimumItemBytes = 5;
constexpr size_t MaximumItemBytes = 32;

void SetSockets(ITEM& item, const ItemCreationParams& p)
{
    item.SocketCount = p.SocketCount;
    item.SocketSeedSetOption = p.SocketBonusOption;
    for (int s = 0; s < MAX_SOCKETS; ++s)
    {
        item.bySocketOption[s] = p.SocketOptions[s];
        if (item.bySocketOption[s] == SOCKET_EMPTY)
        {
            item.SocketSeedID[s] = SOCKET_EMPTY;
            continue;
        }
        item.SocketSeedID[s] = item.bySocketOption[s] % SEASON4A::MAX_SOCKET_OPTION;
        item.SocketSphereLv[s] = item.bySocketOption[s] / SEASON4A::MAX_SOCKET_OPTION + 1;
    }
}
} // namespace

namespace UI::Items::FromData
{
std::unique_ptr<ITEM> Make(std::span<const std::uint8_t> data)
{
    if (data.size() < MinimumItemBytes)
        return nullptr;

    std::array<BYTE, MaximumItemBytes> padded{};
    std::copy_n(data.begin(), std::min(data.size(), padded.size()), padded.begin());
    const ItemCreationParams p = ParseItemData(std::span<const BYTE>(padded.data(), padded.size()));

    auto item = std::make_unique<ITEM>();
    std::memset(item.get(), 0, sizeof(ITEM));
    item->bPeriodItem = p.WithExpiration;
    item->bExpiredPeriod = p.IsExpired;
    item->Type = static_cast<short>(p.Group * MAX_ITEM_INDEX + p.Number);
    item->Level = p.Level;
    item->Durability = p.Durability;
    item->HasLuck = p.WithLuck;
    item->HasSkill = p.WithSkill;
    item->OptionType = p.OptionType;
    item->OptionLevel = p.OptionLevel;
    item->ExcellentFlags = p.ExcellentFlags;
    item->AncientDiscriminator = p.AncientDiscriminator;
    item->AncientBonusOption = p.AncientBonusOption;
    item->Jewel_Of_Harmony_Option = p.HarmonyOptionType;
    item->Jewel_Of_Harmony_OptionLevel = p.HarmonyOptionLevel;
    item->option_380 = p.HasGuardianOption;
    SetSockets(*item, p);
    item->byColorState = ITEM_COLOR_NORMAL;
    SetItemAttributes(item.get());
    return item;
}
} // namespace UI::Items::FromData
