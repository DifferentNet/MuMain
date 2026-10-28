#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Events/TitlesRmlModel.h"
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
// The titles window (MU Moldova, ghinzuro 2026-10-27), opened by the titles button of the character window (C) or by
// typing /titles: the character's titles ("Mine") or the whole catalogue ("All") with how long each lasts, the ones
// not owned greyed with how to get them. A click on an owned title wears it, "No title" takes the plate off. It asks
// the server when it opens and every minute while it is open; the answers stay out of the chat
// (GameLogic/Events/Titles.h). Docked right beside the character window; titles.rml draws it.
class CTitlesWindow : public CObject
{
    enum eLAYOUT
    {
        // The widths the texts are fitted to (reference px; the theme places them), as titles.rcss lays them out.
        WINDOW_WIDTH = 190,
        CONTENT_WIDTH = 166,
        TAB_WIDTH = 76,
        ROW_WIDTH = 140,
        MARK_WIDTH = 40,
    };

public:
    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CTitlesWindow();
    ~CTitlesWindow() override;

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
    void BindRmlModel(Rml::DataModelConstructor& c, TitlesRmlModel& model);
    void SyncRmlModel();
    void SyncContent();

    CManager* m_pNewUIMng = nullptr;
    std::chrono::steady_clock::time_point m_openedAt{};
    std::chrono::steady_clock::time_point m_lastRequest{};
    // 0: the own titles, 1: the catalogue.
    int m_tab = 0;
    int m_pendingTab = -1;
    int m_pendingWear = -1;
    bool m_pendingExit = false;
    // The title of each row shown ("" for "No title"), for a click; and whether the row may be worn.
    std::vector<std::wstring> m_rowIds;
    std::vector<bool> m_rowOwned;

    UI::RmlBridge::ThemedView<TitlesRmlModel> m_RmlView{"titles",
        [this](Rml::DataModelConstructor& c, TitlesRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/titles.rml"}}};
};
} // namespace mu::ui::window
