#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Events/SoloKalimaRmlModel.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <chrono>
#include <string>
#include <vector>

namespace Rml
{
class ElementDocument;
} // namespace Rml

namespace mu::ui::window
{
// Kundun's Lair (solo Kalima), opened by the lair's NPC in Lorencia and Devias (the server sends "#SKO|"): the
// character's counted Combat Power and HP, the Mirrors of Dimensions, the runs left today, the tiers (ten since 2026-11-27) with what
// they need and the best times, the curses to pick, and the button that takes the character in. It asks the server
// again every few seconds while it is open; the answers stay out of the chat (GameLogic/Events/SoloKalima.h). Docked
// right like the other NPC windows, in the Tier Maps window's look; solo_kalima.rml draws it.
class CSoloKalimaWindow : public CObject
{
    enum eLAYOUT
    {
        // The widths the texts are fitted to (reference px; the theme places them), as solo_kalima.rcss lays them out.
        WINDOW_WIDTH = 190,
        CONTENT_WIDTH = 166,
        ROW_WIDTH = 162,
        CP_WIDTH = 34,
        CP_GAP = 12, // the row is 158 px inside its padding (162 - 4): keep a gap between the name and the CP
        CURSE_WIDTH = 148,
        SCROLLBAR_ROOM = 6, // the list's 4 px scroll bar takes this off the full-width lines when it shows
        ENTER_WIDTH = 120,
    };

public:
    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CSoloKalimaWindow();
    ~CSoloKalimaWindow() override;

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
    void BindRmlModel(Rml::DataModelConstructor& c, SoloKalimaRmlModel& model);
    void HandleClicks();
    void ToggleCurse(int index);
    // Drops picked curses the server no longer offers, and those over its limit.
    void KeepPickedCursesValid();
    bool IsCursePicked(const std::wstring& key) const;
    void SyncRmlModel();
    void SyncContent();
    void SyncTiers(SoloKalimaRmlModel& updated, int selected) const;
    void SyncCurses(SoloKalimaRmlModel& updated) const;
    // Why the character can't go into the picked tier now (empty when it can).
    std::wstring EnterBlocker(int selected) const;
    // The picked tier, or the highest open one while none is picked.
    int SelectedTier() const;

    CManager* m_pNewUIMng = nullptr;
    std::chrono::steady_clock::time_point m_openedAt{};
    int m_selectedTier = 0;
    std::vector<std::wstring> m_pickedCurses;
    int m_pendingSelect = 0;
    int m_pendingCurse = -1;
    int m_seenEnterAnswers = 0;
    bool m_pendingExit = false;
    bool m_pendingEnter = false;

    UI::RmlBridge::ThemedView<SoloKalimaRmlModel> m_RmlView{"solo_kalima",
        [this](Rml::DataModelConstructor& c, SoloKalimaRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/solo_kalima.rml"}}};
};
} // namespace mu::ui::window
