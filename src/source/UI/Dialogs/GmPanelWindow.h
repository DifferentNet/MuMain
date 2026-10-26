#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Dialogs/GmPanelRmlModel.h"
#include "UI/Inventory/ItemCameraTarget.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

struct tagITEM;

namespace Rml
{
class Element;
class ElementDocument;
} // namespace Rml

namespace mu::ui::window
{
// The game master panel (key Y), for characters of game master accounts: what GMs did with chat commands, with
// clicks. Three tabs, six for admins:
//   Players: who is in game (level, resets, where, Combat Power), a search by name over every character, online or
//     not, and the bans and mutes; for the picked one: go to, summon, kick, mute and unmute, ban (timed or forever,
//     with a reason) and unban.
//   World: the teleport places and a map with coordinates; the GM's invisibility and GM mode (MU logo, admins
//     only), fireworks, a golden notice to everyone.
//   Log: the last actions of every GM (who, when, what, on whom, why).
//   Events (admins): the scheduled events (start now, skip a wait, end), monsters (spawn around the GM, clear
//     around the GM) and the Tier Map runs (end, skip a wait, give a character today's runs back).
//   Items (admins): the client's item list by group or by name; the picked item with what it can have (level, its
//     option, luck, skill, its excellent or other options, its ancient sets, durability, count): the icon's tooltip
//     shows the item as it will be made; Create puts it into the GM's bag, the picked player's or on the ground
//     (with a reason); items can be saved (in config.ini) and loaded again.
//   Character (admins): a character by its name, online or not. Editor: level, resets, free points, stats, zen, PK
//     status, class (offline only) and a move; Save shows what changes and asks for a reason. Items: its bag, worn
//     items, personal store and vault, each with its tooltip; delete one (vault items only offline) or copy it into
//     the GM's bag. The Log tab undoes an edit or a deletion.
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
        // Three tabs or modes, four action buttons (or tabs) or two switches to a row.
        TAB_WIDTH = 54,
        ACTION_WIDTH = 40,
        SWITCH_WIDTH = 82,
        // The Items tab: a step's label and value, an arrow between them; the item's lines by its icon; the
        // count's and durability's labels; Create and Save, which share their row with the durability.
        STEP_LABEL_WIDTH = 56,
        // The Character tab's labels before its typed values.
        PAIR_LABEL_WIDTH = 36,
        STEP_VALUE_WIDTH = 66,
        ITEM_LINES_WIDTH = 120,
        COUNT_LABEL_WIDTH = 30,
        DUR_LABEL_WIDTH = 34,
        ITEM_BUTTON_WIDTH = 48,
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
        // Admins only.
        TAB_EVENTS,
        TAB_ITEMS,
        TAB_CHARACTER,
    };
    enum Mode
    {
        MODE_ONLINE,
        MODE_SEARCH,
        MODE_BANS,
    };
    // The Events tab's own modes.
    enum EventsMode
    {
        EV_EVENTS,
        EV_MONSTERS,
        EV_TIERS,
    };
    // The Events tab's buttons.
    enum EventsAction
    {
        EV_START = 1,
        EV_SKIP,
        EV_END,
        EV_REFRESH,
        EV_SPAWN,
        EV_CLEAR,
        EV_TIER_END,
        EV_TIER_SKIP,
        EV_ENTRIES,
    };
    // The Items tab's own modes.
    enum ItemsMode
    {
        IT_CATALOG,
        IT_MAKE,
        IT_SAVED,
    };
    // The Items tab's buttons.
    enum ItemsAction
    {
        IT_GROUP_DOWN = 1,
        IT_GROUP_UP,
        IT_LEVEL_DOWN,
        IT_LEVEL_UP,
        IT_OPTION_KIND,
        IT_OPTION_DOWN,
        IT_OPTION_UP,
        IT_SET_DOWN,
        IT_SET_UP,
        IT_LUCK,
        IT_SKILL,
        IT_CREATE,
        IT_SAVE,
        IT_LOAD,
        IT_DELETE,
        IT_OPTION_KIND_DOWN,
    };
    // The Character tab's modes and buttons; the Log tab's.
    enum CharacterMode
    {
        CH_EDITOR,
        CH_ITEMS,
    };
    enum CharacterAction
    {
        CH_SAVE = 1,
        CH_REFRESH,
        CH_PK_DOWN,
        CH_PK_UP,
        CH_CLASS_DOWN,
        CH_CLASS_UP,
        CH_MOVE_DOWN,
        CH_MOVE_UP,
        CH_ITEM_DELETE,
        CH_ITEM_COPY,
    };
    enum LogAction
    {
        LOG_UNDO = 1,
    };
    // The Character tab's move step besides the warps (their index in the warp list).
    enum CharacterMove
    {
        MOVE_STAY = -1,
        MOVE_TO_ME = -2,
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

    // An item as the Items tab makes it: what "/gm mk" sends (the option by its number, the extras as a mask of
    // iteminfo's indexes, the set by its index from 1), and what a saved one keeps.
    struct ItemDraft
    {
        int Group = -1;
        int Number = -1;
        int Level = 0;
        // 0: none.
        int Option = 0;
        int OptionLevel = 0;
        bool Luck = false;
        bool Skill = false;
        int Extras = 0;
        // 0: none; Bonus 0 for a set without a bonus option.
        int Set = 0;
        int Bonus = 0;
        // 0: a whole item (one piece of a stack).
        int Durability = 0;
        int Count = 1;
        // The set's name as the server said it, for the saved list.
        std::wstring SetName;

        bool Picked() const { return Group >= 0 && Number >= 0; }
        bool operator==(const ItemDraft&) const = default;
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
    void SyncEvents(GmPanelRmlModel& updated);
    void SyncMonsters(GmPanelRmlModel& updated);
    void SyncTiers(GmPanelRmlModel& updated);
    void SyncItems(GmPanelRmlModel& updated);
    void SyncItemCatalog(GmPanelRmlModel& updated);
    void SyncItemMake(GmPanelRmlModel& updated);
    void SyncItemSaved(GmPanelRmlModel& updated);
    void SyncCharacter(GmPanelRmlModel& updated);
    void SyncCharEditor(GmPanelRmlModel& updated);
    void SyncCharItems(GmPanelRmlModel& updated);

    void HandleClicks();
    void PickRow(int row);
    void SwitchTab(int tab);
    void SwitchMode(int mode);
    void RequestCurrent();
    void Search();
    void FindMonsters();
    void GoToCoordinates();
    void DoAction(int action);
    void DoEventsAction(int action);
    void DoSwitch(int id);
    void DoItemsAction(int action);
    void DoCharacterAction(int action);
    void DoLogAction(int action);

    // The Character tab.
    // Opens the typed name: asks for its values or its items.
    void OpenCharacter();
    // A new answer about the opened character: the typed values and the steps are set from it.
    void TakeCharacter();
    // A new list of its items: the items are made again.
    void TakeInventory();
    // True while the editor shows the opened character's values.
    bool CharacterReady() const;
    // Shows what changes, asks for a reason, sends "/gm edit".
    void SaveCharacter();
    // Asks for a reason, sends "/gm itemdel" or "/gm itemcopy" for the picked item.
    void AskItemAction(bool remove);
    // The picked item of the list, or null.
    int PickedItemIndex() const;
    // The item the icon shows (the Items tab's preview, the Character tab's picked item) and the icon's element id.
    tagITEM* ShownItem() const;
    const char* IconId() const;

    // The Items tab.
    // The catalog's items (by the group, or by the searched name in every group), its rows built again.
    void BuildItemList();
    void SearchItems();
    // Opens the Make page for the item and asks the server what it can have.
    void PickItem(int group, int number);
    // The draft within what the server's answer allows (once it came).
    void FitDraft();
    // True when the server's answer is about the draft's item.
    bool ItemInfoReady() const;
    // The draft with the typed count and durability.
    ItemDraft CurrentDraft() const;
    void ToggleExtra(int index);
    // Asks where to, then for the reason, then sends "/gm mk".
    void CreateItem();
    void SaveDraft();
    void LoadSaved();
    void DeleteSaved();
    // The preview item: made again when the draft changed.
    void UpdatePreview();
    void RenderItemIcon();
    // The item camera's field of view that fits the preview into its icon box (the bigger the item, the wider).
    float ItemFieldOfView() const;
    // A saved item as config.ini keeps it ("group|number|level|option|option level|luck|skill|extras|set|bonus|
    // durability|count|set name"), and back.
    static std::wstring SavedText(const ItemDraft& draft);
    static bool ParseSaved(const std::wstring& text, ItemDraft& draft);
    // "opt 3, luck, skill, 2 exc, Hyon +10, x5": the level is left out (the saved list's title has it).
    static std::wstring DraftSummary(const ItemDraft& draft);
    // "/gm mk <to> ..." without the reason.
    static std::wstring MakeRequest(const ItemDraft& draft, const std::wstring& to);
    // Enter in one of the window's text fields (by its id).
    void TakeEnter(const Rml::String& fieldId);
    // The field that takes the keys when the page opens: the players' or the monsters' search.
    const char* FocusField() const;
    // The time left of an event or a run: what the server said, less the time since its list came.
    int SecondsLeft(int left, std::chrono::steady_clock::time_point receivedAt) const;
    void NoteLists();
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

    // The Events tab: its mode, what is picked in each list (an event's key, a monster's number, a run's leader),
    // the last monster search, and when its list was asked for (it asks again now and then while shown).
    int m_evMode = EV_EVENTS;
    std::wstring m_evSelected;
    int m_monsterSelected = -1;
    std::wstring m_tierSelected;
    std::wstring m_lastMonsterSearch;
    std::chrono::steady_clock::time_point m_evAskAt{};
    // When the events' and the runs' lists came (their seconds count from then).
    int m_eventsVersion = -1;
    int m_tiersVersion = -1;
    std::chrono::steady_clock::time_point m_eventsAt{};
    std::chrono::steady_clock::time_point m_tiersAt{};

    // The Items tab: its mode, the catalog's group and search, the listed item types and their rows (built only
    // when they change), the item being made, the server answer it was fitted to, the saved item picked, and the
    // preview the icon and its tooltip show (owned here, not by the item manager).
    int m_itMode = IT_CATALOG;
    int m_itGroup = 0;
    std::wstring m_itSearch;
    std::vector<int> m_itTypes;
    std::vector<GmPanelRow> m_itRows;
    bool m_itMore = false;
    bool m_itListDirty = true;
    ItemDraft m_draft;
    bool m_draftFresh = false;
    // Which of the item's normal options the option's arrows step (an item may have several kinds, wings).
    int m_optionKind = 0;
    int m_draftInfoVersion = -1;
    int m_savedSelected = -1;
    std::unique_ptr<tagITEM> m_preview;
    ItemDraft m_previewOf;
    int m_previewInfoVersion = -1;
    // The Character tab: its mode, the name its answers are about (what Open sent, then as the server writes it),
    // the editor's steps (the PK state, the class number, the move: MOVE_STAY, MOVE_TO_ME or a warp's index) and
    // the answer they were set from; the items: the picked one (its storage and slot), the answer they were made
    // from, the items (made here, owned here) and their rows.
    int m_chMode = CH_EDITOR;
    std::wstring m_charName;
    int m_chPk = 3;
    int m_chClass = 0;
    int m_chMove = MOVE_STAY;
    int m_chSeenVersion = -1;
    wchar_t m_invStorage = L'i';
    int m_invSlot = -1;
    int m_invSeenVersion = -1;
    std::vector<std::unique_ptr<tagITEM>> m_invItems;
    std::vector<GmPanelRow> m_invRows;
    // The Log tab's entry picked for an undo (its number).
    long long m_logSelected = 0;

    UI::Items::ItemCameraTarget m_itemTarget{[this](const Rml::Vector2f&, const Rml::Vector2f&) { RenderItemIcon(); }};

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
