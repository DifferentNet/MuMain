#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Events/RenaShopRmlModel.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <chrono>
#include <string>

namespace Rml
{
class ElementDocument;
} // namespace Rml

namespace GameLogic::RenaShop
{
struct Line;
} // namespace GameLogic::RenaShop

namespace mu::ui::window
{
// The Golden Archer's Rena shop window, opened by the NPC (the server sends "#RNO|"): the Rena balance and the Rena in
// the bag with a button that hands them in, then three tabs: the shop (lines for Rena, the picked one bought one or
// five at a time), the Golden Arrow (the prizes with their chances, the spin price, one or ten spins and the last
// results) and the progress (Rena handed in, the title milestones, this week's hand-ins and the week's top 5); and the
// line with the last answer. It asks the server again every few seconds while it is open; the answers stay out of the
// chat (GameLogic/Events/RenaShop.h). Docked right like the other NPC windows; rena_shop.rml draws it.
class CRenaShopWindow : public CObject
{
public:
    enum eLAYOUT
    {
        // The widths the texts are fitted to (reference px; the theme places them), as rena_shop.rcss lays them out.
        WINDOW_WIDTH = 190,
        CONTENT_WIDTH = 166,
        ROW_WIDTH = 162,
        TAB_WIDTH = 46,
        BUY_WIDTH = 72,
    };

    enum eTAB
    {
        TAB_SHOP = 0,
        TAB_ARROW,
        TAB_PROGRESS,
        TAB_COUNT,
    };

    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CRenaShopWindow();
    ~CRenaShopWindow() override;

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
    void BindRmlModel(Rml::DataModelConstructor& c, RenaShopRmlModel& model);
    void SyncRmlModel();
    void SyncContent();
    void SyncHeader(RenaShopRmlModel& updated, std::chrono::steady_clock::time_point now) const;
    void AddRows(RenaShopRmlModel& updated) const;
    void SyncFoot(RenaShopRmlModel& updated) const;
    void SyncShopButtons(RenaShopRmlModel& updated) const;
    void SyncSpinButtons(RenaShopRmlModel& updated) const;
    void PublishModel(RenaShopRmlModel& updated);

    // The clicks RmlUi left for this frame.
    void SelectTab(int tab);
    void TakeActions(bool give, int pick, int act);
    void TakeAnswers();
    void RefreshFromServer();
    const GameLogic::RenaShop::Line* PickedLine() const;

    CManager* m_pNewUIMng = nullptr;
    std::chrono::steady_clock::time_point m_openedAt{};
    std::chrono::steady_clock::time_point m_lastRefresh{};
    int m_tab = TAB_SHOP;
    int m_pendingTab = -1;
    bool m_pendingExit = false;
    bool m_pendingGive = false;
    // The picked shop line (by key, as the list comes again after each action), the row and the count clicked this
    // frame, and whether the last answer is still the one to show.
    std::wstring m_pick;
    int m_pendingPick = -1;
    int m_pendingAct = 0;
    int m_seenAnswers = 0;
    bool m_showAnswer = false;

    UI::RmlBridge::ThemedView<RenaShopRmlModel> m_RmlView{"rena_shop",
        [this](Rml::DataModelConstructor& c, RenaShopRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/rena_shop.rml"}}};
};
} // namespace mu::ui::window
