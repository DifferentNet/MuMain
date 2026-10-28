#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Inventory/ItemCameraTarget.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/Types.h>

#include <chrono>
#include <vector>

namespace Rml
{
class ElementDocument;
} // namespace Rml

namespace mu::ui::window
{
// A row of the jewel bank window: a kind of jewel, how many the bank holds, and whether its buttons work.
struct JewelBankRow
{
    // Its kind, an index into GameLogic::JewelBank::Kinds() (the buttons send it).
    int kind = 0;
    Rml::String name;
    // The name's size, shrunk to fit its line (the native renderer's rule).
    float namePx = 0.f;
    Rml::String count;
    // Jewels of the kind in the inventory to deposit, and in the bank to take.
    bool canDeposit = false;
    bool canWithdraw = false;

    bool operator==(const JewelBankRow&) const = default;
};

struct JewelBankRmlModel
{
    float textPx = 0.f; // native text size in physical px (RmlNativeTextSize.h)
    Rml::String title;
    std::vector<JewelBankRow> rows;
    // Under the rows: a hint, or why there are no counts.
    Rml::String status;
    float statusPx = 0.f;

    Rml::String depositTooltip;
    Rml::String withdrawTooltip;
    Rml::String exitTooltip;
};

// The jewel bank window (GameLogic/Items/JewelBank.h), opened by its button in the inventory: a row per kind of
// jewel with its icon, the count the account's bank holds, a button that deposits every jewel of the kind from the
// inventory and one that asks how many to take out. It asks the server when it opens and after each deposit or
// withdrawal; the answers stay out of the chat. Docked beside the inventory and closed with it; jewel_bank.rml
// draws it, C++ draws the jewels into its #jb_items.
class CJewelBankWindow : public CObject
{
    enum eLAYOUT
    {
        // The width a jewel's name is fitted to (reference px; jewel_bank.rcss places it).
        NAME_WIDTH = 132,
        STATUS_WIDTH = 120,
    };

public:
    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CJewelBankWindow();
    ~CJewelBankWindow() override;

    bool Create(CManager* pNewUIMng);
    void Release();

    bool UpdateMouseEvent() override;
    Rml::ElementDocument* GetPlacedDocument() const override { return m_RmlView.Document(); }
    bool UpdateKeyEvent() override;
    bool Update() override;
    bool Render() override;

    float GetLayerDepth() override;
    float GetKeyEventOrder() override;

    void OpenningProcess();
    void ClosingProcess();

private:
    void BindRmlModel(Rml::DataModelConstructor& c, JewelBankRmlModel& model);
    void SyncRmlModel();
    void SyncContent();
    void AskWithdrawAmount(int kind);
    // The jewels, into the document's #jb_items, each over its row's icon box.
    void RenderItems();

    CManager* m_pNewUIMng = nullptr;
    // The kinds of the rows shown, in their order: an optional kind the server doesn't keep has no row.
    std::vector<int> m_shownKinds;
    std::chrono::steady_clock::time_point m_openedAt{};
    // After a deposit or a withdrawal the counts are asked for again at this time; none while it is the epoch.
    std::chrono::steady_clock::time_point m_refreshDue{};
    // Clicks, handled in Update() rather than inside RmlUi's event dispatch.
    bool m_pendingExit = false;
    int m_pendingDeposit = -1;
    int m_pendingWithdraw = -1;

    UI::RmlBridge::ThemedView<JewelBankRmlModel> m_RmlView{"jewel_bank",
        [this](Rml::DataModelConstructor& c, JewelBankRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/jewel_bank.rml"}}};

    UI::Items::ItemCameraTarget m_ItemTarget{[this](const Rml::Vector2f&, const Rml::Vector2f&) { RenderItems(); }};
};
} // namespace mu::ui::window
