#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Events/BattlePassRmlModel.h"
#include "UI/Inventory/ItemCameraTarget.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/Types.h>

#include <chrono>
#include <optional>
#include <string>
#include <vector>


typedef struct tagITEM ITEM;

namespace Rml
{
class ElementDocument;
} // namespace Rml

namespace mu::ui::window
{
// The battle pass window (key X, and the bottom bar's button): the season, the level reached with the gauge of the
// next one, and two tabs. Rewards: the levels of the track with their items, live 3D with the game's item tooltips,
// in a pane the wheel scrolls, opened on the next level; a reached level is claimed with a click on it, all of them
// with the button at the bottom. Quests: today's daily quests (the day's bar with its chests, the week's days, a
// button that replaces a quest), the weekly ones by week, the season's, and the season's result. It asks the server
// when it opens, when the server says something changed, when a countdown runs out, and every half minute while it is
// open; the answer stays out of the chat (GameLogic/Events/BattlePass.h). The content is built again only when the
// answer, the tab or the text size changes; the countdowns tick apart, once a second.
// Docked right like the daily rewards (key N); battle_pass.rml draws it.
class CBattlePassWindow : public CObject
{
    enum eLAYOUT
    {
        // The widths the texts are fitted to (reference px; battle_pass.rcss places them): the window, the lines
        // under the title, the two halves of a line, the gauge, a tab and the button at the bottom.
        WINDOW_WIDTH = 190,
        CONTENT_WIDTH = 166,
        HALF_WIDTH = 82,
        GAUGE_WIDTH = 162,
        TAB_WIDTH = 80,
        BUTTON_WIDTH = 100,
    };

public:
    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CBattlePassWindow();
    ~CBattlePassWindow() override;

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
    // A click on the window, queued by RmlUi and acted on in Update().
    enum class Click
    {
        None,
        Exit,
        Claim,
        ClaimLevel,
        Take,
        Reroll,
        TabRewards,
        TabQuests,
    };

    // Asks again on the server's signal, soon after a button, and every half minute.
    void RefreshIfDue();
    void HandleClick(Click click, int value);

    void BindRmlModel(Rml::DataModelConstructor& c, BattlePassRmlModel& model);
    void SyncRmlModel();
    // Builds the content from the answer when what it is built from changed; true when it did.
    bool SyncContent();
    // The countdowns of the header and the quests' headings, from the server's seconds, once a second (or after a
    // build); one that runs out asks the server again.
    void SyncTimers(bool force);
    // Scrolls the track so the next level (or the first to claim) is in the middle, once it is laid out.
    void CenterTrack();

    // Draws the shown levels' items into #bp_items (m_ItemTarget).
    void RenderItems(const Rml::Vector2f& offset, const Rml::Vector2f& size);
    // The item under the pointer, for its tooltip: made from the level's reward when the pointer moves onto it.
    void SyncTooltipItem();
    void ReleaseTooltipItem();

    CManager* m_pNewUIMng = nullptr;
    std::chrono::steady_clock::time_point m_openedAt{};
    // A refresh asked for (after a button) that is due at this time; none while it is the epoch.
    std::chrono::steady_clock::time_point m_refreshDue{};
    Click m_pendingClick = Click::None;
    int m_pendingValue = 0;
    int m_tab = 0;
    // The track is scrolled to the next level when it shows first after opening, or after its tab is picked.
    bool m_centerPending = false;
    unsigned int m_shownRevision = 0;

    // What the shown content was built from (SyncContent()).
    struct BuildKey
    {
        unsigned int revision = 0;
        int tab = 0;
        // 0 the server's answer, 1 the pass is off, 2 asking, 3 no answer.
        int state = 0;
        float textPx = 0.f;
        float boldPx = 0.f;
        // A string of the current language, which a switch of the language replaces.
        const wchar_t* language = nullptr;

        bool operator==(const BuildKey&) const = default;
    };
    std::optional<BuildKey> m_builtFrom;
    // The answer's second the countdowns were set for (-1: none shown), and the answer a countdown that ran out asked
    // again after (once per answer).
    long long m_timersAt = -1;
    unsigned int m_ranOutAskedFor = 0;

    // The game items of the shown levels, by their icon's id, for drawing and tooltips.
    struct ShownItem
    {
        Rml::String id;
        int type = 0;
        int level = 0;
        int count = 1;
    };
    std::vector<ShownItem> m_shownItems;
    // The icon under the pointer (its index in m_shownItems, -1 for none) and the item made for its tooltip.
    int m_hoveredItem = -1;
    int m_tooltipFor = -1;
    ITEM* m_tooltipItem = nullptr;

    UI::Items::ItemCameraTarget m_ItemTarget{[this](const Rml::Vector2f& offset, const Rml::Vector2f& size)
                                             { RenderItems(offset, size); }};
    UI::RmlBridge::ThemedView<BattlePassRmlModel> m_RmlView{"battle_pass",
        [this](Rml::DataModelConstructor& c, BattlePassRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/battle_pass.rml"}}};
};
} // namespace mu::ui::window
