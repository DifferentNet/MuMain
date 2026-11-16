#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Core/WindowManager.h"
#include "UI/Options/KeyBindingsRmlModel.h"
#include "UI/RmlBridge/RmlStackingOrder.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <chrono>

namespace Rml
{
class ElementDocument;
} // namespace Rml
#include <vector>

namespace mu::ui::window
{
// The player's key bindings (Core/Input/KeyBindings.h), opened from Options > Gameplay: one row per action in three
// groups (windows, potions, skill slots) with its key. A click on a row waits for a key; the next key pressed is
// the action's new key, and the action that had it takes the old one (its row flashes). Escape cancels the wait,
// keys that may not be bound are refused with a hint and the wait goes on. Reset gives every action its default
// key. Every change is saved to config.ini at once. Centred and draggable; key_bindings.rml draws it.
class CKeyBindingsWindow : public CObject
{
public:
    static constexpr float LayerDepth = 10.68f;
    // Above the options window and the chat (10 and 9), so a key pressed while waiting reaches no hot key; below
    // the dialogs (100).
    static constexpr float KeyEventOrder = 50.f;

    CKeyBindingsWindow();
    ~CKeyBindingsWindow() override;

    bool Create(CManager* pNewUIMng);
    void Release();

    bool UpdateMouseEvent() override;
    bool UpdateKeyEvent() override;
    bool Update() override;
    bool Render() override;

    float GetLayerDepth() override;
    float GetKeyEventOrder() override;
#ifdef __EMSCRIPTEN__
    // The warm-up (UI/Core/WindowWarmUp.h).
    Rml::ElementDocument* GetWarmUpDocument() const override { return m_RmlView.Document(); }
#endif

    void OpenningProcess();
    void ClosingProcess();

private:
    // A click RmlUi reported, applied in Update().
    enum class Command
    {
        Close,
        Capture,
        Reset,
    };

    struct PendingCommand
    {
        Command command;
        int action = 0;
    };

    using Clock = std::chrono::steady_clock;

    // What the model was last made from; it is made again only when one of these changed (a key, the waiting
    // row, the flashing row, the hint, the language through its title text, the document after a theme switch).
    struct ShownState
    {
        unsigned revision = 0;
        int capturing = -1;
        int flashing = -1;
        bool refused = false;
        const wchar_t* title = nullptr;
        const Rml::ElementDocument* document = nullptr;

        bool operator==(const ShownState&) const = default;
    };

    void BindRmlModel(Rml::DataModelConstructor& c, KeyBindingsRmlModel& model);
    void OnRmlBuilt();
    void Queue(Command command, int action = 0);
    void ApplyPending();
    void StartCapture(int action);
    void StopCapture();
    // The key pressed this frame while waiting: binds it or refuses it.
    void CaptureKey();
    // Scrolls the row of m_scrollTo into view, once.
    void ScrollToPendingRow();
    void Close();

    void SyncRmlModel();
    void SyncContent();

    CManager* m_pNewUIMng = nullptr;
    std::vector<PendingCommand> m_pending;
    // The action waiting for a key (Core::Input::KeyBindings::Action), or -1.
    int m_capturing = -1;
    // The last key pressed while waiting could not be bound.
    bool m_refused = false;
    // The action that last took another one's key in a swap, flashing until m_flashUntil; -1 for none.
    int m_flashing = -1;
    Clock::time_point m_flashUntil{};
    // The row to scroll into view on the next Update (the one that just lost its key), or -1.
    int m_scrollTo = -1;
    ShownState m_shown;

    UI::RmlBridge::ThemedView<KeyBindingsRmlModel> m_RmlView{"key_bindings",
        [this](Rml::DataModelConstructor& c, KeyBindingsRmlModel& model) { BindRmlModel(c, model); },
        {{"Data/Interface/RmlUi/key_bindings.rml"}}, {.afterBuild = [this] { OnRmlBuilt(); }}};
};
} // namespace mu::ui::window
