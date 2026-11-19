#pragma once

#include "UI/Events/EventEntryRmlModel.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <string>
#include <vector>

namespace Rml
{
class ElementDocument;
}

namespace mu::ui::window
{
// The RmlUi side of the right-docked event entry windows that share the original's layout
// (CEnterBloodCastle, CEnterDevilSquare): the docked frame, a bold title, description lines, a
// column of level buttons of which one is enabled, the exit button. Each window owns one,
// with its own document and data model; the window keeps its data, its hit tests and requests.
class EventEntryView
{
public:
    struct Button
    {
        std::wstring label;
        bool enabled = false;
    };

    struct Line
    {
        std::wstring text;
        bool missing = false; // red
    };

    // The ticket for a band: its name and level in white when one is in the bag (any page), else a red
    // "not in the bag" line.
    static Line TicketLine(short itemType, int ticketLevel);

    EventEntryView(const char* modelName, const char* documentPath);

    void Build();
    // Unloads the document; from the window's own Release().
    void Release() { m_View.Release(); }

    // Complete translated content, set when the window opens. The theme owns its layout.
    void SetContent(const wchar_t* title, const std::vector<Line>& lines, const std::vector<Button>& buttons);

    // Per frame, inside the window's CManager transform scope.
    void Sync(bool visible);

    // The pointer is over what the document draws (UI::RmlBridge::IsPointerOver()).
    bool IsPointerOver() const;
    Rml::ElementDocument* Document() const { return m_View.Document(); }

    // A click RmlUi reported since the last call: an enabled level button's index (else -1), the
    // exit button.
    int TakePressedButton();
    bool TakeExitPressed();

private:
    void BindModel(Rml::DataModelConstructor& c, EventEntryRmlModel& model);
    void SyncTextSizes();
    void ScrollEnabledButtonIntoView();

    static constexpr int ScrollWaitFrames = 10;

    UI::RmlBridge::ThemedView<EventEntryRmlModel> m_View;
    int m_PressedButton = -1;
    bool m_ExitPressed = false;
    // Set by SetContent(): the phone view scrolls the level list to the hero's band once the new bands are built.
    bool m_ScrollToEnabledPending = false;
    // The new content's band count and its first enabled band (-1: none), to tell the new bands from the old.
    int m_ScrollButtonCount = 0;
    int m_ScrollTargetIndex = -1;
    // Frames the scroll still waits for the new bands; the first frame after SetContent() is always skipped.
    int m_ScrollFramesLeft = 0;
};
} // namespace mu::ui::window
