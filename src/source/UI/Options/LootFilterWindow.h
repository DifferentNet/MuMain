#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Options/LootFilterRmlModel.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <string>
#include <vector>

namespace Rml
{
class ElementDocument;
} // namespace Rml

namespace mu::ui::window
{
// The loot highlight filter's settings (GameLogic/Items/LootFilter.h), opened from Options > Gameplay or the touch
// menu: the on switch and the presets on top; one row per item type with Hide / Normal / Highlight (Hide is off for
// jewels and quest items); the options that always highlight (excellent, ancient, socket, luck, skill, option, item
// level from +N) with the level and zen thresholds; names to highlight (a field, Add and removable chips); and the
// beam and drop sound switches. Every change applies at once: the items on the ground are classified again and
// config.ini is saved. Centred and draggable; loot_filter.rml draws it.
class CLootFilterWindow : public CObject
{
public:
    static constexpr float LayerDepth = 10.68f;

    CLootFilterWindow();
    ~CLootFilterWindow() override;

    bool Create(CManager* pNewUIMng);
    void Release();

    bool UpdateMouseEvent() override;
    bool UpdateKeyEvent() override;
    bool Update() override;
    bool Render() override;
    // Escape and Enter while the name field is typed in.
    bool TakesTypingFrom(const Rml::ElementDocument* document) const override
    {
        return document != nullptr && document == m_RmlView.Document();
    }

    float GetLayerDepth() override;
    float GetKeyEventOrder() override;

    void OpenningProcess();
    void ClosingProcess();

    // A click RmlUi reported, applied in Update(). Public for the .cpp's helpers only: this header stays clear of
    // GameLogic::Items, whose name would hide the global Items[] in every file that includes WindowSystem.h.
    enum class Action
    {
        Close,
        Enabled,
        Marker,
        Sound,
        Preset,
        Category,
        Rarity,
        Level,
        Zen,
        AddName,
        RemoveName,
    };

    struct PendingAction
    {
        Action action;
        int first = 0;
        int second = 0;
    };

private:
    void BindRmlModel(Rml::DataModelConstructor& c, LootFilterRmlModel& model);
    void OnRmlBuilt();
    void Queue(Action action, int first = 0, int second = 0);
    // The name typed in the field, normalised; the field empties.
    std::wstring TakeTypedName();
    void Close();

    void SyncRmlModel();
    void SyncContent();

    CManager* m_pNewUIMng = nullptr;
    std::vector<PendingAction> m_pending;

    UI::RmlBridge::ThemedView<LootFilterRmlModel> m_RmlView{"loot_filter",
        [this](Rml::DataModelConstructor& c, LootFilterRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/loot_filter.rml"}}, {.afterBuild = [this] { OnRmlBuilt(); }}};
};
} // namespace mu::ui::window
