#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Events/GrandShopRmlModel.h"
#include "UI/Inventory/ItemCameraTarget.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <chrono>
#include <string>
#include <vector>

namespace Rml
{
class ElementDocument;
} // namespace Rml

namespace GameLogic::GrandShop
{
struct Line;
} // namespace GameLogic::GrandShop

namespace mu::ui::window
{
// The Grand Reset shop of ancient sets, opened by the chat command /grshop (the server answers "#GSO|"): the Grand
// Marks balance on top, the sets on the left (the high-tier ones marked, the Jewel of Ancient on a row of its own) and
// the shown set's pieces on the right, each with its live 3D icon, name, price and a Buy button that asks for a
// confirmation first; then the line with the last answer or why buying is off, and the note that bought pieces are bound
// to the account. The answers stay out of the chat (GameLogic/Events/GrandShop.h). Docked right like the other NPC
// windows, but wider; grand_shop.rml draws it.
class CGrandShopWindow : public CObject
{
public:
    enum eLAYOUT
    {
        // The widths the texts are fitted to (reference px; the theme places them), as grand_shop.rcss lays them out.
        WINDOW_WIDTH = 400,
        CONTENT_WIDTH = 376,
        SET_NAME_WIDTH = 92,
        TOP_WIDTH = 24,
        ITEM_NAME_WIDTH = 136,
        ITEM_PRICE_WIDTH = 136,
        BUY_WIDTH = 56,
    };

    static constexpr float LayerDepth = UI::RmlBridge::ForegroundPanelLayerDepth;

    CGrandShopWindow();
    ~CGrandShopWindow() override;

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
    // A set of the list: the key is its kind and name (the Jewel of Ancient has its own), the lines are its pieces.
    struct SetEntry
    {
        std::wstring key;
        std::wstring name;
        bool top = false;
        std::vector<const GameLogic::GrandShop::Line*> lines;
    };

    // A piece's box drawn this frame.
    struct ShownIcon
    {
        Rml::String id;
        int type = 0;
        int setType = 0;
    };

    void BindRmlModel(Rml::DataModelConstructor& c, GrandShopRmlModel& model);
    void SyncRmlModel();
    void SyncContent();
    void SyncSets(GrandShopRmlModel& updated, const std::vector<SetEntry>& sets);
    void SyncItems(GrandShopRmlModel& updated, const SetEntry* shown, std::vector<ShownIcon>& icons);
    void SyncFoot(GrandShopRmlModel& updated) const;
    void PublishModel(GrandShopRmlModel& updated);
    void RenderItems(const Rml::Vector2f& offset, const Rml::Vector2f& size);

    // The clicks RmlUi left for this frame.
    void TakeClicks(bool exit, int set, int buy);
    void AskToBuy(const GameLogic::GrandShop::Line& line);
    void TakeAnswers();
    const SetEntry* ShownSet(const std::vector<SetEntry>& sets);
    // The sets of the shop's lines in the order they come, the Jewel of Ancient last.
    static std::vector<SetEntry> BuildSets(const std::vector<GameLogic::GrandShop::Line>& lines);

    CManager* m_pNewUIMng = nullptr;
    std::chrono::steady_clock::time_point m_openedAt{};
    bool m_pendingExit = false;
    int m_pendingSet = -1;
    int m_pendingBuy = -1;
    // The picked set (by key, as the list comes again), the sets and the shown set's pieces as of the last frame (what a
    // click's index means), and whether the last answer is still the one to show.
    std::wstring m_pickedSet;
    std::vector<std::wstring> m_setKeys;
    std::vector<std::wstring> m_itemKeys;
    int m_seenAnswers = 0;
    bool m_showAnswer = false;
    std::vector<ShownIcon> m_shownIcons;

    UI::Items::ItemCameraTarget m_ItemTarget{[this](const Rml::Vector2f& offset, const Rml::Vector2f& size)
                                             { RenderItems(offset, size); }};
    UI::RmlBridge::ThemedView<GrandShopRmlModel> m_RmlView{"grand_shop",
        [this](Rml::DataModelConstructor& c, GrandShopRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/grand_shop.rml"}}};
};
} // namespace mu::ui::window
