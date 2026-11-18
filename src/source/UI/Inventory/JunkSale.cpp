#include "stdafx.h"

#include "UI/Inventory/JunkSale.h"

#include "Core/Utilities/StringUtils.h"
#include "Data/GameConfig/GameConfig.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/InventoryUtils.h"
#include "GameLogic/Items/ItemCategories.h"
#include "GameLogic/Items/JunkItem.h"
#include "GameLogic/Items/JunkMarkSet.h"
#include "GameLogic/Items/SaleRefusal.h"
#include "GameLogic/Items/ShopRestrictions.h"
#include "I18N/All.h"
#include "Network/Server/SocketSystem.h"
#include "UI/Chat/CharacterChangeLines.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Inventory/InventoryContents.h"
#include "UI/Inventory/InventoryCtrl.h"
#include "UI/Inventory/InventorySort.h"
#include "UI/Inventory/MyInventory.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
namespace JunkMarks = GameLogic::Items::JunkMarks;
using mu::ui::window::CInventoryCtrl;
using mu::ui::window::CPickedItem;

// A sale's answer comes within this, or the sale stops (the item on its way comes back with the answer).
constexpr DWORD SaleAnswerTimeoutMs = 5000;
// How often marks whose item left the bag are looked for.
constexpr DWORD MarkCheckMs = 250;
constexpr std::size_t ItemNameSize = 128;
constexpr std::size_t LineSize = 256;
constexpr std::size_t GoldTextSize = 64;

JunkMarks::JunkMarkSet g_marks;
// The character the marks belong to; empty until its inventory came.
std::wstring g_owner;
// The marks as last written, so an unchanged set is not written again.
std::string g_savedMarks;
DWORD g_marksCheckedAt = 0;

struct Sale
{
    bool running = false;
    bool waiting = false;
    DWORD sentAt = 0;
    // The marks the player agreed to sell, in order, and the next one.
    std::vector<JunkMarks::Mark> items;
    std::size_t next = 0;
    int sold = 0;
    int soldSlot = -1;
    std::int64_t goldAtStart = 0;
};

Sale g_sale;

void SaySystem(const wchar_t* text)
{
    g_pSystemLogBox->AddText(text, mu::ui::window::TYPE_SYSTEM_MESSAGE);
}

void SayError(const wchar_t* text)
{
    if (!g_pSystemLogBox->CheckChatRedundancy(text))
        g_pSystemLogBox->AddText(text, mu::ui::window::TYPE_ERROR_MESSAGE);
}

bool IsOwnerInWorld()
{
    return Hero != nullptr && !g_owner.empty() && g_owner == Hero->ID;
}

void SaveMarks()
{
    if (g_owner.empty())
        return;
    std::string text = g_marks.Serialize();
    if (text == g_savedMarks)
        return;
    GameConfig::GetInstance().SetJunkMarks(g_owner, StringUtils::NarrowToWide(text));
    g_savedMarks = std::move(text);
}

void LoadMarks()
{
    g_owner = Hero != nullptr ? Hero->ID : L"";
    g_marks = JunkMarks::JunkMarkSet::Parse(
        StringUtils::WideToNarrow(GameConfig::GetInstance().GetJunkMarks(g_owner).c_str()));
    g_savedMarks = g_marks.Serialize();
}

// The grid of the bag page holding the item at slot.
CInventoryCtrl* GridHolding(int slot)
{
    for (int g = 0; g < mu::ui::window::CMyInventory::OwnedBagGridCount; ++g)
    {
        CInventoryCtrl* grid = g_pMyInventory->OwnedBagGrid(g);
        if (grid != nullptr && grid->FindItem(slot) != nullptr)
            return grid;
    }
    return nullptr;
}

// Every bag item with its slot, over the bag pages the character owns.
template <typename Visit> void ForEachBagItem(Visit&& visit)
{
    for (int g = 0; g < mu::ui::window::CMyInventory::OwnedBagGridCount; ++g)
    {
        CInventoryCtrl* grid = g_pMyInventory->OwnedBagGrid(g);
        if (grid == nullptr)
            continue;
        const int count = static_cast<int>(grid->GetNumberOfItems());
        for (int i = 0; i < count; ++i)
        {
            if (ITEM* item = grid->GetItem(i))
                visit(grid->GetIndexByItem(item), *item);
        }
    }
}

// The bag item a mark stands for, or nullptr when another item (or none) is in its cell.
ITEM* MarkedItem(const JunkMarks::Mark& mark)
{
    ITEM* item = UI::Inventory::FindPlayerItem(mark.slot);
    if (item == nullptr || JunkMarks::FingerprintOf(*item) != mark.print)
        return nullptr;
    // The item must start in that very cell, not only cover it.
    CInventoryCtrl* grid = GridHolding(mark.slot);
    if (grid == nullptr || grid->GetIndexByItem(item) != mark.slot)
        return nullptr;
    return item;
}

// Whether a marked item may be sold to the open shop right now: the junk rule and the shop's own refusals.
bool MaySell(ITEM& item)
{
    if (!JunkMarks::MayBeJunk(item) || GameLogic::Items::IsHighValueItem(&item) ||
        GameLogic::Items::IsSellingBan(&item))
        return false;
    return !GameLogic::Items::SaleRefusalText(&item, CharacterMachine->Gold, ItemValue(&item, 1));
}

// The bag settles: nothing on the cursor, no move or sort waiting for the server.
bool IsBagSettled()
{
    return !IsItemMovePending() && CInventoryCtrl::GetPickedItem() == nullptr &&
           !UI::Inventory::Sort().IsWaiting();
}

// The marks meet the bag as it stands: see JunkMarkSet::Settle.
void SettleMarks(bool followSort)
{
    if (g_marks.Count() == 0)
        return;
    std::vector<JunkMarks::BagItem> bag;
    ForEachBagItem(
        [&bag](int slot, const ITEM& item)
        {
            if (slot >= 0)
                bag.push_back({slot, JunkMarks::FingerprintOf(item)});
        });
    g_marks.Settle(bag, followSort);
    SaveMarks();
}

// The marks a sale would take now, in bag order, cut where the zen would pass the limit.
std::vector<JunkMarks::Mark> SellableMarks(bool& capped, std::int64_t& total)
{
    std::vector<JunkMarks::Mark> marks(g_marks.All());
    std::sort(marks.begin(), marks.end(),
              [](const JunkMarks::Mark& a, const JunkMarks::Mark& b) { return a.slot < b.slot; });
    std::vector<JunkMarks::Mark> sellable;
    capped = false;
    total = 0;
    for (const JunkMarks::Mark& mark : marks)
    {
        ITEM* item = MarkedItem(mark);
        if (item == nullptr || !MaySell(*item))
            continue;
        const std::int64_t price = ItemValue(item, 1);
        if (CharacterMachine->Gold + total + price > GameLogic::Items::kMaxCarriedZen)
        {
            capped = true;
            break;
        }
        total += price;
        sellable.push_back(mark);
    }
    return sellable;
}

std::wstring GoldText(std::int64_t gold)
{
    wchar_t text[GoldTextSize]{};
    ConvertGold(static_cast<double>(gold), text);
    return text;
}

std::vector<mu::ui::window::GenericDialogConfig::Line> ConfirmLines(const std::vector<JunkMarks::Mark>& marks,
                                                                    std::int64_t total, bool capped)
{
    std::vector<mu::ui::window::GenericDialogConfig::Line> lines;
    wchar_t line[LineSize]{};
    mu_swprintf(line, I18N::Game::ShopSellJunkCountD, static_cast<int>(marks.size()));
    lines.push_back({line, true});
    // Every item is named: the player sees all that goes before agreeing. A long list scrolls in the dialog.
    for (std::size_t i = 0; i < marks.size(); ++i)
    {
        const ITEM* item = UI::Inventory::FindPlayerItem(marks[i].slot);
        if (item == nullptr)
            continue;
        wchar_t name[ItemNameSize]{};
        GetItemName(item->Type, item->Level, name);
        lines.push_back({name, false});
    }
    mu_swprintf(line, I18N::Game::ShopSellJunkTotalS, GoldText(total).c_str());
    lines.push_back({line, true});
    if (capped)
    {
        mu_swprintf(line, I18N::Game::ShopSellJunkCapD, static_cast<int>(marks.size()));
        lines.push_back({line, false});
    }
    return lines;
}

bool CanSendSale()
{
    return IsBagSettled() && !g_pNPCShop->IsSellingItem();
}

// Lifts the item off its cell, as the right click's sale does, and asks the shop to buy it.
bool SendSale(int slot, ITEM& item)
{
    CInventoryCtrl* grid = GridHolding(slot);
    if (grid == nullptr || !CInventoryCtrl::CreatePickedItem(grid, &item))
        return false;
    CPickedItem* picked = CInventoryCtrl::GetPickedItem();
    if (picked == nullptr)
        return false;
    grid->RemoveItem(&item);
    picked->HidePickedItem();
    const int sourceIndex = picked->GetSourceLinealPos();
    if (sourceIndex != slot || !IsPlayerInventorySlot(sourceIndex))
    {
        CInventoryCtrl::BackupPickedItem();
        return false;
    }
    UI::Chat::ChangeLines::NoteSaleAsked();
    SocketClient->ToGameServer()->SendSellItemToNpcRequest(static_cast<BYTE>(sourceIndex));
    g_pNPCShop->SetSellingItem(true);
    return true;
}

void EndSale()
{
    if (!g_sale.running)
        return;
    const int planned = static_cast<int>(g_sale.items.size());
    wchar_t line[LineSize]{};
    if (g_sale.sold == planned)
    {
        const std::int64_t earned = std::max<std::int64_t>(CharacterMachine->Gold - g_sale.goldAtStart, 0);
        mu_swprintf(line, I18N::Game::ShopSellJunkDone, g_sale.sold, GoldText(earned).c_str());
    }
    else
    {
        mu_swprintf(line, I18N::Game::ShopSellJunkStopped, g_sale.sold, planned);
    }
    SaySystem(line);
    g_sale = {};
    // The marks the sale took off are written once, here.
    SaveMarks();
}

void StartSale(std::vector<JunkMarks::Mark> marks)
{
    if (g_sale.running || marks.empty() || !g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_NPCSHOP))
        return;
    g_sale = {};
    g_sale.running = true;
    g_sale.items = std::move(marks);
    g_sale.goldAtStart = CharacterMachine->Gold;
}

// The next marked item goes, checked again first; one that no longer passes is passed over and loses its mark.
void SellNext(DWORD now)
{
    while (g_sale.next < g_sale.items.size())
    {
        const JunkMarks::Mark mark = g_sale.items[g_sale.next];
        ITEM* item = MarkedItem(mark);
        // The zen would pass the limit: the sale ends here and the rest keep their marks.
        if (item != nullptr && CharacterMachine->Gold + ItemValue(item, 1) > GameLogic::Items::kMaxCarriedZen)
        {
            EndSale();
            return;
        }
        if (item == nullptr || !MaySell(*item))
        {
            g_sale.items.erase(g_sale.items.begin() + static_cast<std::ptrdiff_t>(g_sale.next));
            g_marks.Unmark(mark.slot);
            continue;
        }
        if (!SendSale(mark.slot, *item))
        {
            EndSale();
            return;
        }
        g_sale.waiting = true;
        g_sale.sentAt = now;
        g_sale.soldSlot = mark.slot;
        ++g_sale.next;
        return;
    }
    EndSale();
}

void TickSale(DWORD now)
{
    if (!g_sale.running)
        return;
    if (g_sale.waiting)
    {
        if (now - g_sale.sentAt >= SaleAnswerTimeoutMs)
            EndSale();
        return;
    }
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_NPCSHOP) || Hero == nullptr || Hero->Dead != 0)
    {
        EndSale();
        return;
    }
    if (CanSendSale())
        SellNext(now);
}
} // namespace

namespace UI::Inventory::JunkSale
{
bool IsMarked(int slot, const ITEM& item)
{
    if (g_marks.Count() == 0 || !IsOwnerInWorld())
        return false;
    return g_marks.IsMarked(slot, JunkMarks::FingerprintOf(item));
}

bool OffersMark(const ITEM& item)
{
    return JunkMarks::ReadTraits(item).kind != JunkMarks::Kind::Other;
}

void ToggleMark(int slot)
{
    const ITEM* item = FindPlayerItem(slot);
    if (item == nullptr || !IsOwnerInWorld() || g_sale.running)
        return;
    if (IsMarked(slot, *item))
    {
        g_marks.Unmark(slot);
        SaveMarks();
        return;
    }
    if (!JunkMarks::MayBeJunk(*item))
    {
        SayError(I18N::Game::JunkCannotMark);
        return;
    }
    g_marks.Mark(slot, JunkMarks::FingerprintOf(*item));
    SaveMarks();
}

void MarkPlainGear()
{
    if (!IsOwnerInWorld() || g_sale.running)
        return;
    int marked = 0;
    ForEachBagItem(
        [&marked](int slot, const ITEM& item)
        {
            if (slot < 0 || !JunkMarks::MayBeJunk(item))
                return;
            const JunkMarks::Fingerprint print = JunkMarks::FingerprintOf(item);
            if (g_marks.IsMarked(slot, print) || !g_marks.Mark(slot, print))
                return;
            ++marked;
        });
    if (marked > 0)
        SaveMarks();
    wchar_t line[LineSize]{};
    mu_swprintf(line, I18N::Game::JunkMarkedD, marked);
    SaySystem(line);
}

int SellableCount()
{
    if (g_marks.Count() == 0 || !IsOwnerInWorld())
        return 0;
    bool capped = false;
    std::int64_t total = 0;
    return static_cast<int>(SellableMarks(capped, total).size());
}

void AskToSell()
{
    if (g_sale.running || !IsOwnerInWorld() || !CanSendSale())
        return;
    bool capped = false;
    std::int64_t total = 0;
    std::vector<JunkMarks::Mark> marks = SellableMarks(capped, total);
    if (marks.empty())
        return;
    mu::ui::window::GenericDialogConfig cfg;
    cfg.title = I18N::Game::ShopSellJunkTitle;
    cfg.showCancel = true;
    cfg.lines = ConfirmLines(marks, total, capped);
    // Nothing is sold before the answer; the sale checks every item again.
    cfg.onPrimary = [marks = std::move(marks)]() mutable { StartSale(std::move(marks)); };
    mu::ui::window::g_pGenericConfirmDialog->Show(std::move(cfg));
}

bool IsSelling()
{
    return g_sale.running;
}

void StopSale()
{
    if (!g_sale.running)
        return;
    // The answer on its way still comes back to the shop's handler, which keeps or discards the item.
    EndSale();
}

void OnSaleAnswer(bool sold)
{
    if (!g_sale.running || !g_sale.waiting)
        return;
    g_sale.waiting = false;
    if (!sold)
    {
        EndSale();
        return;
    }
    ++g_sale.sold;
    g_marks.Unmark(g_sale.soldSlot);
}

void OnBagListed()
{
    if (Hero == nullptr)
        return;
    if (g_owner != Hero->ID)
        LoadMarks();
    SettleMarks(Sort().IsWaitingFor(InventorySort::Target::Bag));
    SaveMarks();
}

void OnItemMoved(int from, int to)
{
    if (g_marks.Count() == 0 || !IsPlayerInventorySlot(from))
        return;
    if (IsPlayerInventorySlot(to))
        g_marks.Move(from, to);
    else
        g_marks.Unmark(from);
    SaveMarks();
}

void Tick()
{
    const DWORD now = GetTickCount();
    TickSale(now);
    if (g_marks.Count() == 0 || !IsOwnerInWorld() || g_sale.running || now - g_marksCheckedAt < MarkCheckMs)
        return;
    g_marksCheckedAt = now;
    if (IsBagSettled())
        SettleMarks(false);
}
} // namespace UI::Inventory::JunkSale
