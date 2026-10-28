#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Inventory/JewelBankWindow.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/JewelBank.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Inventory/MyInventory.h"

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

// Jewels of a kind in the inventory's bag, packed ones counted as the jewels they hold (10 per level from +0).
int InventoryJewels(const GameLogic::JewelBank::Kind& kind)
{
    CInventoryCtrl* bag = g_pMyInventory != nullptr ? g_pMyInventory->GetInventoryCtrl() : nullptr;
    if (bag == nullptr)
        return 0;

    int count = 0;
    for (size_t i = 0; i < bag->GetNumberOfItems(); ++i)
    {
        const ITEM* item = bag->GetItem(static_cast<int>(i));
        if (item == nullptr)
            continue;
        if (item->Type == kind.itemType)
            count += 1;
        else if (kind.packedType >= 0 && item->Type == kind.packedType)
            count += 10 * (item->Level + 1);
    }
    return count;
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

    GenericDialogConfig cfg;
    cfg.showCancel = true;
    cfg.lines = {{question, false}};
    cfg.input = GenericDialogConfig::InputField{};
    cfg.input->mode = GenericDialogConfig::InputField::Mode::Text;
    cfg.input->maxLength = MaximumWithdrawDigits;
    cfg.input->numericOnly = true;
    cfg.onPrimary = [kind]
    {
        const std::wstring text = g_pGenericConfirmDialog->GetInputText();
        const int amount = text.empty() ? 0 : _wtoi(text.c_str());
        if (amount <= 0)
        {
            g_pGenericConfirmDialog->KeepOpen();
            return;
        }

        Bank().Withdraw(kind, std::min(amount, Bank().Count(kind)));
    };
    g_pGenericConfirmDialog->Show(std::move(cfg));
}

void mu::ui::window::CJewelBankWindow::BindRmlModel(Rml::DataModelConstructor& c, JewelBankRmlModel& model)
{
    auto rowType = c.RegisterStruct<JewelBankRow>();
    rowType.RegisterMember("kind", &JewelBankRow::kind);
    rowType.RegisterMember("name", &JewelBankRow::name);
    rowType.RegisterMember("name_px", &JewelBankRow::namePx);
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
    m_ItemTarget.Sync(m_RmlView.Document() ? m_RmlView.Document()->GetElementById("jb_items") : nullptr, IsVisible());
    if (!m_RmlView.Document())
        return;

    UI::RmlBridge::SyncDocumentVisibilityInFront(m_RmlView.Document(), IsVisible());
    if (!IsVisible())
        return;

    UI::RmlBridge::SyncNativeTextSize(m_RmlView.Binder());
    SyncContent();
}

void mu::ui::window::CJewelBankWindow::SyncContent()
{
    // A text at the size the native renderer would have shrunk it to for its box.
    auto fitted = [](const std::wstring& text, int width) -> float
    {
        g_pRenderText->SetFont(g_hFont);
        const float measured = static_cast<float>(g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx);
        return UI::RmlBridge::NativeTextPxInBox(UI::Scaling::FontRole::Normal, measured, static_cast<float>(width));
    };

    auto& bank = Bank();
    JewelBankRmlModel updated;
    updated.title = StringUtils::WideToNarrow(I18N::Game::JewelBankTitle);

    // A row per kind; an optional kind the server doesn't keep is left out once it has answered.
    const auto& kinds = GameLogic::JewelBank::Kinds();
    std::vector<int> shown;
    for (int i = 0; i < static_cast<int>(kinds.size()); ++i)
    {
        if (i >= GameLogic::JewelBank::BaseKindCount && !bank.IsAvailable(i))
            continue;
        const auto& kind = kinds[i];
        const std::wstring name = ItemName(kind.itemType);
        const int inBag = InventoryJewels(kind);
        shown.push_back(i);

        JewelBankRow row;
        row.kind = i;
        row.name = StringUtils::WideToNarrow(name.c_str());
        row.namePx = fitted(name, NAME_WIDTH);
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
