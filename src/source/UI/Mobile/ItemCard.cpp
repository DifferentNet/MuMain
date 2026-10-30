#include "stdafx.h"
#include "UI/Mobile/ItemCard.h"

#include "Engine/Object/ZzzInventory.h"
#include "I18N/All.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Dialogs/GenericMenuDialog.h"

#include <utility>

namespace UI::Mobile::ItemCard
{
namespace
{
using mu::ui::window::GenericDialogConfig;
using mu::ui::window::GenericMenuConfig;

std::wstring ItemName(const ITEM& item)
{
    // Longest name the item tables hold, plus room for the terminator.
    wchar_t name[128] = {};
    GetItemName(item.Type, item.Level, name);
    return name;
}

void AskToDrop(const ITEM& item, std::function<void()> drop)
{
    GenericDialogConfig cfg;
    cfg.primaryLabel = I18N::Game::TouchItemDrop;
    cfg.showCancel = true;
    cfg.cancelLabel = I18N::Game::Cancel;
    cfg.item3D = item;
    cfg.lines = {
        {ItemName(item), true},
        {I18N::Game::TouchItemDropAsk, false},
    };
    cfg.onPrimary = std::move(drop);
    mu::ui::window::g_pGenericConfirmDialog->Show(std::move(cfg));
}

void AddButton(GenericMenuConfig& cfg, std::wstring label, std::function<void()> onClick)
{
    if (!onClick)
        return;
    GenericMenuConfig::MenuButton button;
    button.label = std::move(label);
    button.onClick = std::move(onClick);
    cfg.buttons.push_back(std::move(button));
}
} // namespace

void Show(Content content)
{
    GenericMenuConfig cfg;
    cfg.title = ItemName(content.item);

    AddButton(cfg, std::move(content.primaryLabel), std::move(content.primary));
    AddButton(cfg, I18N::Game::TouchItemHotkeyQ, std::move(content.putOnQ));
    AddButton(cfg, I18N::Game::TouchItemHotkeyW, std::move(content.putOnW));
    if (content.drop)
    {
        AddButton(cfg, I18N::Game::TouchItemDrop,
                  [item = content.item, drop = std::move(content.drop)] { AskToDrop(item, drop); });
    }

    GenericMenuConfig::MenuButton close;
    close.label = I18N::Game::Close;
    close.compact = true;
    close.dismiss = true;
    cfg.buttons.push_back(std::move(close));

    mu::ui::window::g_pGenericMenuDialog->Show(std::move(cfg));
}
} // namespace UI::Mobile::ItemCard
