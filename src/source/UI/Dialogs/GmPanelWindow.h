#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Dialogs/GmPanelRmlModel.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <chrono>
#include <string>

namespace Rml
{
class Element;
class ElementDocument;
} // namespace Rml

namespace mu::ui::window
{
// The game master panel (key Y), for characters of game master accounts: what GMs did with chat commands, with
// clicks. Three tabs:
//   Players: who is in game (level, resets, where, Combat Power), a search by name over every character, online or
//     not, and the bans and mutes; for the picked one: go to, summon, kick, mute and unmute, ban (timed or forever,
//     with a reason) and unban.
//   World: the teleport places and a map with coordinates; the GM's invisibility and GM mode (MU logo, admins
//     only), fireworks, a golden notice to everyone.
//   Log: the last actions of every GM (who, when, what, on whom, why).
// The server's add-on (GmPanel) decides every request by the account (GameLogic/Commands/GmPanel.h); the window
// only shows what it may do. Docked right like the other panels; gm_panel.rml draws it.
class CGmPanelWindow : public CObject
{
    enum eLAYOUT
    {
        // The widths the texts are fitted to (reference px), as gm_panel.rcss lays them out.
        WINDOW_WIDTH = 190,
        CONTENT_WIDTH = 166,
        // A list row: the content less the scrollbar's lane and the row's padding.
        ROW_WIDTH = 140,
        // Three tabs or modes, four action buttons or two switches to a row.
        TAB_WIDTH = 54,
        ACTION_WIDTH = 40,
        SWITCH_WIDTH = 82,
    };

public:
    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CGmPanelWindow();
    ~CGmPanelWindow() override;

    bool Create(CManager* pNewUIMng);
    void Release();

    bool UpdateMouseEvent() override;
    Rml::ElementDocument* GetPlacedDocument() const override { return m_RmlView.Document(); }
    bool UpdateKeyEvent() override;
    // Escape and Enter while a field of the window is typed in.
    bool TakesTypingFrom(const Rml::ElementDocument* document) const override
    {
        return document != nullptr && document == m_RmlView.Document();
    }
    bool Update() override;
    bool Render() override;

    float GetLayerDepth() override;
    float GetKeyEventOrder() override;

    void OpenningProcess();
    void ClosingProcess();

private:
    enum Tab
    {
        TAB_PLAYERS,
        TAB_WORLD,
        TAB_LOG,
    };
    enum Mode
    {
        MODE_ONLINE,
        MODE_SEARCH,
        MODE_BANS,
    };
    enum Action
    {
        ACTION_GOTO = 1,
        ACTION_SUMMON,
        ACTION_KICK,
        ACTION_REFRESH,
        ACTION_MUTE,
        ACTION_UNMUTE,
        ACTION_BAN,
        ACTION_UNBAN,
    };
    enum Switch
    {
        SWITCH_HIDE = 1,
        SWITCH_GM_MODE,
        SWITCH_FIREWORKS,
        SWITCH_NOTICE,
    };

    // What the window knows about the picked character, from whichever list it was picked in.
    struct Target
    {
        std::wstring Name;
        bool Known = false;
        bool Online = false;
        bool Muted = false;
        bool Banned = false;
    };

    void BindRmlModel(Rml::DataModelConstructor& c, GmPanelRmlModel& model);
    void SyncRmlModel();
    void SyncContent();
    void SyncPlayers(GmPanelRmlModel& updated);
    void SyncWorld(GmPanelRmlModel& updated);
    void SyncLog(GmPanelRmlModel& updated);

    void HandleClicks();
    void SwitchTab(int tab);
    void SwitchMode(int mode);
    void RequestCurrent();
    void Search();
    void GoToCoordinates();
    void DoAction(int action);
    void DoSwitch(int id);
    void AskMute(const std::wstring& name);
    void AskBan(const std::wstring& name);
    void Send(const std::wstring& request);
    void TakeResult();
    Target GetTarget() const;
    Rml::Element* GetField(const char* id) const;

    CManager* m_pNewUIMng = nullptr;
    int m_tab = TAB_PLAYERS;
    int m_mode = MODE_ONLINE;
    std::wstring m_selected;
    std::wstring m_lastSearch;
    std::chrono::steady_clock::time_point m_requestedAt{};
    std::chrono::steady_clock::time_point m_playersAt{};
    int m_seenResult = 0;
    std::wstring m_status;
    bool m_statusBad = false;
    bool m_focusSearchPending = false;

    // Clicks RmlUi reported, handled in Update().
    int m_pendingTab = -1;
    int m_pendingMode = -1;
    int m_pendingAction = 0;
    int m_pendingSwitch = 0;
    int m_pendingRow = -1;
    int m_pendingWarp = -1;
    bool m_pendingFind = false;
    bool m_pendingGo = false;
    bool m_pendingLogRefresh = false;
    bool m_pendingExit = false;

    UI::RmlBridge::ThemedView<GmPanelRmlModel> m_RmlView{"gm_panel",
        [this](Rml::DataModelConstructor& c, GmPanelRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/gm_panel.rml"}}};
};
} // namespace mu::ui::window
