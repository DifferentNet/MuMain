#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Events/TierMapsRmlModel.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <chrono>

namespace Rml
{
class ElementDocument;
} // namespace Rml

namespace mu::ui::window
{
// The Tier Maps window, opened by Jerint the Assistant in Devias (the server sends "#TMO|"): the six tiers of the
// endgame dungeons in the Imperial Guardian fortress with their Combat Power and today's boss, which ones the party
// may enter, the party's members with their Combat Power and runs left today, the leader's Gaion's Order, and the
// button that takes the party in. It asks the server again every few seconds while it is open; the answers stay out
// of the chat (GameLogic/Events/TierMaps.h). Docked right like the other NPC windows; tier_maps.rml draws it.
class CTierMapsWindow : public CObject
{
    enum eLAYOUT
    {
        // The widths the texts are fitted to (reference px; the theme places them), as tier_maps.rcss lays them out.
        WINDOW_WIDTH = 190,
        CONTENT_WIDTH = 166,
        ROW_WIDTH = 162,
    };

public:
    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CTierMapsWindow();
    ~CTierMapsWindow() override;

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
    void BindRmlModel(Rml::DataModelConstructor& c, TierMapsRmlModel& model);
    void SyncRmlModel();
    void SyncContent();
    // The picked tier, or the highest the party may enter while none is picked (or the picked one closed).
    int SelectedTier() const;

    CManager* m_pNewUIMng = nullptr;
    std::chrono::steady_clock::time_point m_openedAt{};
    int m_selectedTier = 0;
    int m_pendingSelect = 0;
    int m_seenEnterAnswers = 0;
    bool m_pendingExit = false;
    bool m_pendingEnter = false;

    UI::RmlBridge::ThemedView<TierMapsRmlModel> m_RmlView{"tier_maps",
        [this](Rml::DataModelConstructor& c, TierMapsRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/tier_maps.rml"}}};
};
} // namespace mu::ui::window
