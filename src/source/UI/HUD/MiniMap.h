#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/HUD/MainFrameWindow.h"
#include "UI/HUD/ChatLogWindow.h"
#include "UI/Inventory/MyInventory.h"
#include "UI/HUD/MiniMapLayout.h"
#include "UI/HUD/MiniMapRmlModel.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <string>

namespace Rml
{
class ElementDocument;
}

namespace mu::ui::window
{
// The full-screen mini map. mini_map.rml draws it; C++ keeps the world's marker data, the
// hero's position, the turned quads of the map and its markers (MiniMapLayout.h), the marker
// name hint, Tab/Escape and the close request.
class CMiniMap : public CObject
{
public:
    bool m_bSuccess;

    CMiniMap();
    virtual ~CMiniMap();

    bool Create(CManager* pNewUIMng);
    void Release();

    void SetBtnPos(int Num, float x, float y, float nx, float ny);

    bool UpdateMouseEvent();
    bool UpdateKeyEvent();
    bool Update();
    bool Render();

    float GetLayerDepth(); //. 8.1f

    void OpenningProcess();
    void ClosingProcess();
    void LoadImages(const wchar_t* Filename);
    void UnloadImages();


private:
    void BuildRmlUi();
    void SyncRmlModel();
    void SyncScreen();
    void SyncMap();
    void SyncHint();
    void SyncClips();
    // MU Moldova: the live event monsters (UI/HUD/MiniMapLive.h); their names join the boss labels.
    void SyncLive(const UI::MiniMap::Screen& screen, float heroX, float heroY, float length,
                  std::vector<MiniMapLabelEntry>& labels);

    CManager* m_pNewUIMng;
    MINI_MAP m_Mini_Map_Data[MAX_MINI_MAP_DATA];
    float m_Btn_Loc[MAX_MINI_MAP_DATA][4];

    // The world folder (e.g. L"World1") whose mini_map texture the document shows.
    std::wstring m_WorldName;

    void BindRmlModel(Rml::DataModelConstructor& c, MiniMapRmlModel& model);
    UI::RmlBridge::ThemedView<MiniMapRmlModel> m_RmlView{"mini_map",
        [this](Rml::DataModelConstructor& c, MiniMapRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/mini_map.rml"}}};
    bool m_PendingClose = false;
    // The map showed last frame: the live monster list is asked while it does and dropped when it closes.
    bool m_WasVisible = false;
    // The live markers' hint boxes (reference units, as m_Btn_Loc) and texts.
    struct LiveHint
    {
        float box[4];
        std::wstring text;
    };
    std::vector<LiveHint> m_LiveHints;
    // The screen the border tiles' matrices were built for (SyncScreen()).
    UI::MiniMap::Screen m_SideLinesScreen;
};
}
