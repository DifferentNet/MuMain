#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Inventory/JewelBankWindow.h"
#include "UI/Inventory/JewelBankNames.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/JewelBank.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Inventory/MyInventory.h"
#include "UI/Mobile/InterfaceMode.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/Text/CUIRenderText.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <string>

using namespace SEASON3B;
using namespace mu::ui::window;
using GameLogic::JewelBank::Bank;

namespace
{
// Without an answer this long after opening, the server doesn't have the jewel bank.
constexpr auto AnswerTimeout = std::chrono::seconds(5);
// The jewel icon's box in a row (reference px; jewel_bank.rcss places it).
constexpr float IconWidth = 24.f;
constexpr float IconHeight = 27.f;
// The most a withdrawal can ask for: the bank keeps at most a million of a kind.
constexpr int MaximumWithdrawDigits = 7;
// The jewels in a packed jewel of level 0, as the server's /jbwdp gives them.
constexpr int PackSize = 10;

std::wstring ItemName(int itemType)
{
    wchar_t name[256] = {};
    GetItemName(itemType, 0, name);
    return name;
}

std::wstring Amount(int count)
{
    wchar_t text[64] = {};
    ConvertGold(count, text);
    return text;
}

// Jewels of a kind in the inventory's bag (page 2 too, BagPages.h), packed ones counted as the jewels they hold
// (10 per level from +0); for a stacked kind (Rena) the level 0 items, each counted as its stack size.
int InventoryJewels(const GameLogic::JewelBank::Kind& kind)
{
    if (g_pMyInventory == nullptr)
        return 0;

    int count = 0;
    for (int g = 0; g < CMyInventory::OwnedBagGridCount; ++g)
    {
        CInventoryCtrl* bag = g_pMyInventory->OwnedBagGrid(g);
        if (bag == nullptr)
            continue;
        for (size_t i = 0; i < bag->GetNumberOfItems(); ++i)
        {
            const ITEM* item = bag->GetItem(static_cast<int>(i));
            if (item == nullptr)
                continue;
            if (kind.stacked)
            {
                if (item->Type == kind.itemType && item->Level == 0)
                    count += std::max(1, static_cast<int>(item->Durability));
            }
            else if (item->Type == kind.itemType)
                count += 1;
            else if (kind.packedType >= 0 && item->Type == kind.packedType)
                count += 10 * (item->Level + 1);
        }
    }
    return count;
}

// The space a row's name has under it: down to the count, or the whole row without one (reference px;
// jewel_bank.rcss: the name at 2, the count at 16, rows of 31).
constexpr float NameHeightAboveCount = 14.f;
constexpr float NameHeightAlone = 27.f;

// A text's size as the native renderer measures it.
SIZE MeasuredSize(const std::wstring& text)
{
    g_pRenderText->SetFont(g_hFont);
    return g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size()));
}

float MeasuredWidth(const std::wstring& text)
{
    return static_cast<float>(MeasuredSize(text).cx);
}

// The names as the rows show them: in the phone view, when one is too long for its column, without the words
// most of them start with ("Jewel of"), so they shrink less.
std::vector<std::wstring> ShownNames(std::vector<std::wstring> names, bool phoneView, int width)
{
    if (!phoneView)
        return names;
    const bool tooLong = std::any_of(names.begin(), names.end(), [width](const std::wstring& name)
                                     { return MeasuredWidth(name) > static_cast<float>(width); });
    if (!tooLong)
        return names;
    return UI::Inventory::JewelBankNames::WithoutSharedLeadingWords(names);
}
} // namespace

mu::ui::window::CJewelBankWindow::CJewelBankWindow() = default;

mu::ui::window::CJewelBankWindow::~CJewelBankWindow()
{
    Release();
}

bool mu::ui::window::CJewelBankWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_JEWEL_BANK, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CJewelBankWindow::Release()
{
    m_ItemTarget.Disable();
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CJewelBankWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CJewelBankWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CJewelBankWindow::OpenningProcess()
{
    m_openedAt = std::chrono::steady_clock::now();
    m_pendingExit = false;
    m_pendingDeposit = -1;
    m_pendingWithdraw = -1;
    Bank().Request();
}

void mu::ui::window::CJewelBankWindow::ClosingProcess()
{
}

bool mu::ui::window::CJewelBankWindow::UpdateMouseEvent()
{
    // The buttons and the corner close are RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CJewelBankWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_JEWEL_BANK))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_JEWEL_BANK);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    return true;
}

bool mu::ui::window::CJewelBankWindow::Update()
{
    const bool exit = m_pendingExit;
    const int deposit = m_pendingDeposit;
    const int withdraw = m_pendingWithdraw;
    m_pendingExit = false;
    m_pendingDeposit = -1;
    m_pendingWithdraw = -1;

    if (IsVisible())
    {
        // It belongs to the inventory: it closes with it.
        if (exit || !g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_INVENTORY))
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_JEWEL_BANK);
            if (exit)
                PlayBuffer(SOUND_CLICK01);
        }
        else if (deposit >= 0)
        {
            // The server answers with the new counts and a line in the chat.
            Bank().Deposit(deposit);
            PlayBuffer(SOUND_CLICK01);
        }
        else if (withdraw >= 0)
        {
            PlayBuffer(SOUND_CLICK01);
            AskWithdrawAmount(withdraw);
        }
        else if (!Bank().HasInfo() && !Bank().IsUnavailable()
                 && std::chrono::steady_clock::now() - m_openedAt < AnswerTimeout)
        {
            Bank().Request();
        }
    }

    SyncRmlModel();
    return true;
}

bool mu::ui::window::CJewelBankWindow::Render()
{
    // The jewels are drawn by m_ItemTarget into #jb_items; the rest is RmlUi. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CJewelBankWindow::AskWithdrawAmount(int kind)
{
    const auto& kinds = GameLogic::JewelBank::Kinds();
    if (kind < 0 || kind >= static_cast<int>(kinds.size()) || Bank().Count(kind) <= 0)
        return;

    wchar_t question[256] = {};
    mu_swprintf(question, I18N::Game::JewelBankHowMany, ItemName(kinds[kind].itemType).c_str(), Amount(Bank().Count(kind)).c_str());

    // Packs of 10 (ghinzuro, 2026-11-05): a second button for the kinds with a packed form, once the bank holds 10.
    const bool packs = GameLogic::JewelBank::Store::HasPacks(kind) && Bank().Count(kind) >= PackSize;

    GenericDialogConfig cfg;
    cfg.showCancel = true;
    cfg.primaryLabel = I18N::Game::JewelBankWithdraw;
    cfg.lines = {{question, false}};
    if (packs)
    {
        cfg.lines.push_back({I18N::Game::JewelBankPacksHint, false});
        cfg.secondaryLabel = std::wstring(I18N::Game::JewelBankInPacks);
    }
    cfg.input = GenericDialogConfig::InputField{};
    cfg.input->mode = GenericDialogConfig::InputField::Mode::Text;
    cfg.input->maxLength = MaximumWithdrawDigits;
    cfg.input->numericOnly = true;
    const auto take = [kind](bool inPacks)
    {
        const std::wstring text = g_pGenericConfirmDialog->GetInputText();
        const int amount = text.empty() ? 0 : _wtoi(text.c_str());
        if (amount <= 0)
        {
            g_pGenericConfirmDialog->KeepOpen();
            return;
        }

        Bank().Withdraw(kind, std::min(amount, Bank().Count(kind)), inPacks);
    };
    cfg.onPrimary = [take] { take(false); };
    if (packs)
        cfg.onSecondary = [take] { take(true); };
    g_pGenericConfirmDialog->Show(std::move(cfg));
}

void mu::ui::window::CJewelBankWindow::BindRmlModel(Rml::DataModelConstructor& c, JewelBankRmlModel& model)
{
    auto rowType = c.RegisterStruct<JewelBankRow>();
    rowType.RegisterMember("kind", &JewelBankRow::kind);
    rowType.RegisterMember("name", &JewelBankRow::name);
    rowType.RegisterMember("name_px", &JewelBankRow::namePx);
    rowType.RegisterMember("two_lines", &JewelBankRow::twoLines);
    rowType.RegisterMember("count", &JewelBankRow::count);
    rowType.RegisterMember("can_deposit", &JewelBankRow::canDeposit);
    rowType.RegisterMember("can_withdraw", &JewelBankRow::canWithdraw);
    c.RegisterArray<std::vector<JewelBankRow>>();

    c.Bind("text_px", &model.textPx);
    c.Bind("title", &model.title);
    c.Bind("rows", &model.rows);
    c.Bind("status", &model.status);
    c.Bind("status_px", &model.statusPx);
    c.Bind("deposit_tooltip", &model.depositTooltip);
    c.Bind("withdraw_tooltip", &model.withdrawTooltip);
    c.Bind("exit_tooltip", &model.exitTooltip);

    c.BindEventCallback("jewel_bank_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingExit = true; });
    c.BindEventCallback("jewel_bank_deposit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingDeposit = args[0].Get<int>(-1);
                        });
    c.BindEventCallback("jewel_bank_withdraw", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingWithdraw = args[0].Get<int>(-1);
                        });

    model.depositTooltip = StringUtils::WideToNarrow(I18N::Game::JewelBankDeposit);
    model.withdrawTooltip = StringUtils::WideToNarrow(I18N::Game::JewelBankWithdraw);
    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"Esc");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CJewelBankWindow::SyncRmlModel()
{
    m_RmlView.Ensure();
    m_ItemTarget.Sync(m_RmlView.Document(), "jb_items", IsVisible());
    if (!m_RmlView.Document())
        return;

    UI::RmlBridge::SyncDocumentVisibilityInFront(m_RmlView.Document(), IsVisible());
    if (!IsVisible())
        return;

    UI::RmlBridge::SyncNativeTextSize(m_RmlView.Binder());
    SyncContent();
}

const std::vector<mu::ui::window::CJewelBankWindow::NameLayout>&
mu::ui::window::CJewelBankWindow::NameLayouts(const std::vector<std::wstring>& names, bool phoneView, int width,
                                              bool withCounts)
{
    if (names == m_layoutNames && phoneView == m_layoutPhoneView && width == m_layoutWidth &&
        withCounts == m_layoutWithCounts && WindowWidth == m_layoutWindowWidth &&
        WindowHeight == m_layoutWindowHeight && m_nameLayouts.size() == names.size())
        return m_nameLayouts;

    m_layoutNames = names;
    m_layoutPhoneView = phoneView;
    m_layoutWidth = width;
    m_layoutWithCounts = withCounts;
    m_layoutWindowWidth = WindowWidth;
    m_layoutWindowHeight = WindowHeight;
    m_nameLayouts.clear();

    const float boxWidth = static_cast<float>(width);
    const float boxHeight = withCounts ? NameHeightAboveCount : NameHeightAlone;
    for (const std::wstring& name : ShownNames(names, phoneView, width))
    {
        NameLayout layout;
        layout.name = StringUtils::WideToNarrow(name.c_str());
        const float measured = MeasuredWidth(name);
        layout.px = UI::RmlBridge::NativeTextPxInBox(UI::Scaling::FontRole::Normal, measured, boxWidth);

        // In the phone view a name that is still cut at the size it shrank to (the smallest one) goes onto two
        // lines, broken at the space that makes the longer line shortest, sized so both lines fit the column and
        // the room under it.
        const float shrunkWidth = measured * layout.px / UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal);
        const UI::Inventory::JewelBankNames::TwoLines lines =
            phoneView && shrunkWidth > boxWidth + 0.5f ? UI::Inventory::JewelBankNames::SplitInTwo(name, MeasuredWidth)
                                                       : UI::Inventory::JewelBankNames::TwoLines{};
        if (!lines.second.empty())
        {
            const SIZE first = MeasuredSize(lines.first);
            const SIZE second = MeasuredSize(lines.second);
            const float wider = static_cast<float>(std::max(first.cx, second.cx));
            const float tall = static_cast<float>(first.cy + second.cy);
            layout.name = StringUtils::WideToNarrow((lines.first + L"\n" + lines.second).c_str());
            // Not held at the smallest size like a line that shrinks (that size is what cut the name): the lines
            // are made as small as the column and the row's room need, so the name shows whole.
            const float fit = std::min({1.f, boxWidth / std::max(wider, 1.f), boxHeight / std::max(tall, 1.f)});
            layout.px = UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal) * fit;
            layout.twoLines = true;
        }
        m_nameLayouts.push_back(std::move(layout));
    }
    return m_nameLayouts;
}

void mu::ui::window::CJewelBankWindow::SyncContent()
{
    // A text at the size the native renderer would have shrunk it to for its box.
    auto fitted = [](const std::wstring& text, int width) -> float
    {
        return UI::RmlBridge::NativeTextPxInBox(UI::Scaling::FontRole::Normal, MeasuredWidth(text),
                                                static_cast<float>(width));
    };

    auto& bank = Bank();
    // Read every frame the window shows, so a switch to or from the phone view fits the names anew (NameLayouts()
    // measures them only when it or the names change).
    const bool phoneView = UI::Mobile::IsPhoneView();
    const int nameWidth = phoneView ? PHONE_NAME_WIDTH : NAME_WIDTH;
    JewelBankRmlModel updated;
    updated.title = StringUtils::WideToNarrow(I18N::Game::JewelBankTitle);

    // A row per kind; a kind the server doesn't keep (-1: an optional one it lacks, or Harmony, Gemstone and the
    // refine stones since the server dropped the Jewel of Harmony, 2026-11-06) is left out once it has answered.
    const auto& kinds = GameLogic::JewelBank::Kinds();
    std::vector<int> shown;
    std::vector<std::wstring> names;
    for (int i = 0; i < static_cast<int>(kinds.size()); ++i)
    {
        if ((i >= GameLogic::JewelBank::BaseKindCount || bank.HasInfo()) && !bank.IsAvailable(i))
            continue;
        shown.push_back(i);
        names.push_back(ItemName(kinds[i].itemType));
    }
    const auto& layouts = NameLayouts(names, phoneView, nameWidth, bank.HasInfo());

    for (std::size_t r = 0; r < shown.size(); ++r)
    {
        const int i = shown[r];
        const auto& kind = kinds[i];
        const NameLayout& name = layouts[r];
        const int inBag = InventoryJewels(kind);

        JewelBankRow row;
        row.kind = i;
        row.name = name.name;
        row.namePx = name.px;
        row.twoLines = name.twoLines;
        if (bank.HasInfo())
        {
            std::wstring count = Amount(bank.Count(i));
            if (inBag > 0)
                count += L"  (+" + Amount(inBag) + L")";
            row.count = StringUtils::WideToNarrow(count.c_str());
        }
        row.canDeposit = bank.HasInfo() && inBag > 0;
        row.canWithdraw = bank.HasInfo() && bank.Count(i) > 0;
        updated.rows.push_back(std::move(row));
    }

    std::wstring status;
    if (bank.IsUnavailable())
        status = I18N::Game::JewelBankUnavailable;
    else if (!bank.HasInfo())
        status = std::chrono::steady_clock::now() - m_openedAt < AnswerTimeout ? I18N::Game::JewelBankAsking
                                                                              : I18N::Game::JewelBankNoAnswer;
    else
        status = I18N::Game::JewelBankHint;
    updated.status = StringUtils::WideToNarrow(status.c_str());
    updated.statusPx = fitted(status, STATUS_WIDTH);

    m_shownKinds = std::move(shown);

    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &JewelBankRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &JewelBankRmlModel::rows, "rows", updated);
    SyncFieldFrom(binder, &JewelBankRmlModel::status, "status", updated);
    SyncFieldFrom(binder, &JewelBankRmlModel::statusPx, "status_px", updated);
}

void mu::ui::window::CJewelBankWindow::RenderItems()
{
    Rml::ElementDocument* document = m_RmlView.Document();
    Rml::Element* panel = document != nullptr ? document->GetElementById("panel") : nullptr;
    if (panel == nullptr)
        return;

    const float scale = UI::RmlBridge::DrawnScale(*panel);
    Rml::ElementList icons;
    document->QuerySelectorAll(icons, ".jb-icon");
    // The icons are the shown rows' (SyncContent), in their order.
    const auto& kinds = GameLogic::JewelBank::Kinds();
    for (size_t i = 0; i < icons.size() && i < m_shownKinds.size(); ++i)
    {
        Rml::Vector2f origin;
        if (!UI::RmlBridge::DrawnTopLeft(*icons[i], origin))
            continue;
        ::RenderItem3D(origin.x, origin.y, IconWidth * scale, IconHeight * scale, kinds[static_cast<size_t>(m_shownKinds[i])].itemType,
                       0, 0, 0, false);
    }
}
