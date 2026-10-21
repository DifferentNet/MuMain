#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Events/EventScheduleRmlModel.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <chrono>

namespace Rml
{
class ElementDocument;
} // namespace Rml

namespace mu::ui::window
{
// The event timer window (key O): which events run now and how long until the others start, counting down
// every second. It asks the server when it opens and every minute while it is open; the answer stays out of
// the chat (GameLogic/Events/EventTimes.h), so the times can be watched while levelling, when "/events" would
// scroll away under the experience lines. Docked right like the command list (key J); event_schedule.rml
// draws it.
class CEventScheduleWindow : public CObject
{
    enum eLAYOUT
    {
        WINDOW_WIDTH = 190,
        CONTENT_WIDTH = WINDOW_WIDTH - 2 * 22,
        // Rows that fit between the title and the exit button: a name line and a time line each.
        VISIBLE_ROWS = 11,
    };

public:
    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CEventScheduleWindow();
    ~CEventScheduleWindow() override;

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
    // Asks again every minute, and soon after an event started or ended so its next time shows.
    void RefreshIfDue();

    void BindRmlModel(Rml::DataModelConstructor& c, EventScheduleRmlModel& model);
    void SyncRmlModel();
    void SyncContent();

    CManager* m_pNewUIMng = nullptr;
    std::chrono::steady_clock::time_point m_openedAt{};
    bool m_pendingExit = false;

    UI::RmlBridge::ThemedView<EventScheduleRmlModel> m_RmlView{"event_schedule",
        [this](Rml::DataModelConstructor& c, EventScheduleRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/event_schedule.rml"}}};
};
} // namespace mu::ui::window
