#include "stdafx.h"
#include "UI/Inventory/SellConfirmLines.h"

#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInventory.h"
#include "I18N/All.h"

#include <string>

namespace
{
constexpr std::size_t kItemNameSize = 256;
constexpr std::size_t kGoldTextSize = 64;
constexpr std::size_t kPriceLineSize = 160;
} // namespace

namespace UI::Inventory
{
std::vector<mu::ui::window::GenericDialogConfig::Line> SellPriceLines(ITEM* item)
{
    if (item == nullptr)
    {
        return {};
    }

    wchar_t name[kItemNameSize]{};
    GetItemName(item->Type, item->Level, name);

    wchar_t gold[kGoldTextSize]{};
    ConvertGold(static_cast<double>(ItemValue(item, 1)), gold);
    wchar_t price[kPriceLineSize]{};
    mu_swprintf(price, I18N::Game::SellingPriceS, gold);

    return {
        { name, true },
        { std::wstring(price) + L" " + I18N::Game::Zen, true },
    };
}
} // namespace UI::Inventory
