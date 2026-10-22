#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Events/DailyRewardsRmlModel.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <chrono>

namespace Rml
{
class ElementDocument;
} // namespace Rml

namespace mu::ui::window
{
// The daily rewards window (key N): the days of the login reward cycle with their rewards, which ones counted,
// today's progress, and the rewards that wait for room in the inventory with a button that hands them out. It asks
// the server when it opens and every half minute while it is open; the answer stays out of the chat
// (GameLogic/Events/DailyRewards.h). Docked right like the event timer (key O); daily_rewards.rml draws it.
class CDailyRewardsWindow : public CObject
{
    enum eLAYOUT
    {
        // The widths the texts are fitted to (reference px; the theme places them): the window, the progress
        // lines, the days' pane with a day's label and its items, as daily_rewards.rcss lays them out.
        WINDOW_WIDTH = 190,
        CONTENT_WIDTH = 166,
        TABLE_WIDTH = 170,
        LABEL_WIDTH = 44,
        ITEMS_WIDTH = TABLE_WIDTH - 2 * 4 - LABEL_WIDTH,
    };

public:
    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CDailyRewardsWindow();
    ~CDailyRewardsWindow() override;

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
    // Asks again every half minute, soon after the take button, and when today should have counted or a new UTC
    // day began.
    void RefreshIfDue();

    void BindRmlModel(Rml::DataModelConstructor& c, DailyRewardsRmlModel& model);
    void SyncRmlModel();
    void SyncContent();

    CManager* m_pNewUIMng = nullptr;
    std::chrono::steady_clock::time_point m_openedAt{};
    // A refresh asked for (after the take button) that is due at this time; none while it is the epoch.
    std::chrono::steady_clock::time_point m_refreshDue{};
    bool m_pendingExit = false;
    bool m_pendingTake = false;

    UI::RmlBridge::ThemedView<DailyRewardsRmlModel> m_RmlView{"daily_rewards",
        [this](Rml::DataModelConstructor& c, DailyRewardsRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/daily_rewards.rml"}}};
};
} // namespace mu::ui::window
