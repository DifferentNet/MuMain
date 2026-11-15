#pragma once

#include "GameLogic/Events/Titles.h"
#include "Render/Renderer/Overlay2DRecorder.h"
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
class Element;
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
    Rml::ElementDocument* GetPlacedDocument() const override
    {
        return m_RmlView.Document();
    }
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
    void SyncPreview();
    void SyncRowShine();
    // The rows' light elements (one per row, titles.rml), found again only when the list's rows change.
    const std::vector<Rml::Element*>& RowShines();
    void ForgetRowShines();

    CManager* m_pNewUIMng = nullptr;
    // The list the rows' light elements were found in and its child count then: rows are only added or removed at
    // its end (data-for), so the same count means the same elements. Forgotten when the document is built again.
    Rml::Element* m_shineList = nullptr;
    int m_shineListChildren = -1;
    std::vector<Rml::Element*> m_rowShines;
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
    // The preview over the list (ghinzuro, 2026-11-03): the plate of the row under the mouse, else of the worn title,
    // drawn as over the heads with its effects. Each row's plate ("No title"'s without an id), the worn row (-1:
    // none) and the row under the mouse (-1: none; m_pendingHover -2: no change).
    std::vector<GameLogic::Titles::Worn> m_rowPlates;
    int m_wornRow = -1;
    int m_hoverRow = -1;
    int m_pendingHover = -2;
    // What the preview shows now (rarity -1: nothing; -2: build it again), and on which plate element (a theme
    // switch makes new ones), so its elements change only when it does.
    std::wstring m_previewId;
    std::wstring m_previewText;
    const Rml::Element* m_previewPlate = nullptr;
    int m_previewRarity = -2;
    float m_previewWidth = -1.f;
    float m_previewHeight = -1.f;
    float m_previewScale = -1.f;
    std::vector<Render::Renderer::RecordedVertex> m_previewVertices;

    UI::RmlBridge::ThemedView<TitlesRmlModel> m_RmlView{"titles",
                                                        [this](Rml::DataModelConstructor& c, TitlesRmlModel& model)
                                                        { BindRmlModel(c, model); },
                                                        {{"Data/Interface/RmlUi/titles.rml"}},
                                                        {.afterBuild = [this] { ForgetRowShines(); },
                                                         .beforeUnload = [this] { ForgetRowShines(); }}};
};
} // namespace mu::ui::window
