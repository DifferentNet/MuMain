#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Events/EndlessTowerRmlModel.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <chrono>
#include <string>

namespace Rml
{
class ElementDocument;
} // namespace Rml

namespace GameLogic::EndlessTower
{
struct ShopItem;
} // namespace GameLogic::EndlessTower

namespace mu::ui::window
{
// The Endless Tower window, opened by Lugard in Lorencia and Devias (the server sends "#ETO|"): the entry price and
// runs a day, the party's size against the 3 to 5 the tower takes, when the runs come back and the best climbs; then
// five tabs: the party's members (who can go in, and why not), the bosses' items, the rewards by the wave reached, the
// top climbs, and Lugard's shop for tower points; and the button that takes the party in (on the shop, the buttons
// that buy the picked item). It asks the server again every few seconds while it is open; the answers stay out of the
// chat (GameLogic/Events/EndlessTower.h, EndlessTowerShop.h). Docked right like the other NPC windows;
// endless_tower.rml draws it.
class CEndlessTowerWindow : public CObject
{
public:
    enum eLAYOUT
    {
        // The widths the texts are fitted to (reference px; the theme places them), as endless_tower.rcss lays them out.
        WINDOW_WIDTH = 190,
        CONTENT_WIDTH = 166,
        ROW_WIDTH = 162,
        TAB_WIDTH = 29,
        BUY_WIDTH = 72,
    };

    enum eTAB
    {
        TAB_PARTY = 0,
        TAB_BOSSES,
        TAB_REWARDS,
        TAB_TOP,
        TAB_SHOP,
        TAB_COUNT,
    };

    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CEndlessTowerWindow();
    ~CEndlessTowerWindow() override;

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
    void BindRmlModel(Rml::DataModelConstructor& c, EndlessTowerRmlModel& model);
    void SyncRmlModel();
    void SyncContent();
    void SyncHeader(EndlessTowerRmlModel& updated, std::chrono::steady_clock::time_point now) const;
    void AddTowerRows(EndlessTowerRmlModel& updated) const;
    void SyncEnterFoot(EndlessTowerRmlModel& updated) const;
    void AddShopRows(EndlessTowerRmlModel& updated, std::chrono::steady_clock::time_point now) const;
    void SyncShopFoot(EndlessTowerRmlModel& updated) const;
    void PublishModel(EndlessTowerRmlModel& updated);

    // The clicks RmlUi left for this frame. TakeTowerActions is true when the party went in and the window closed.
    void SelectTab(int tab);
    bool TakeTowerActions(bool enter);
    void TakeShopActions(int pick, int buy);
    void RefreshFromServer();
    void AskShop();
    const GameLogic::EndlessTower::ShopItem* PickedItem() const;

    CManager* m_pNewUIMng = nullptr;
    std::chrono::steady_clock::time_point m_openedAt{};
    std::chrono::steady_clock::time_point m_lastRefresh{};
    int m_tab = TAB_PARTY;
    int m_pendingTab = -1;
    int m_seenEnterAnswers = 0;
    bool m_pendingExit = false;
    bool m_pendingEnter = false;
    // The shop's picked item (by key, as the list comes again after each purchase), the row and the count clicked
    // this frame, when the shop was asked for, and whether the last purchase's answer is still the one to show.
    std::wstring m_shopPick;
    int m_pendingPick = -1;
    int m_pendingBuy = 0;
    std::chrono::steady_clock::time_point m_shopAskedAt{};
    int m_seenBuyAnswers = 0;
    bool m_showBuyAnswer = false;

    UI::RmlBridge::ThemedView<EndlessTowerRmlModel> m_RmlView{"endless_tower",
        [this](Rml::DataModelConstructor& c, EndlessTowerRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/endless_tower.rml"}}};
};
} // namespace mu::ui::window
