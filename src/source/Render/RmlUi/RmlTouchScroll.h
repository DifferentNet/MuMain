#pragma once

#include <RmlUi/Core/ObserverPtr.h>
#include <RmlUi/Core/Types.h>

namespace Rml
{
class Context;
class Element;
} // namespace Rml

namespace Render::RmlUi
{
// Whether the element under the window point, or a box around it, scrolls up and down: has more to
// show than it fits, and lets it scroll (overflow auto or scroll).
bool ScrollsVerticallyAt(Rml::Context* context, Rml::Vector2f point);

// Touch scrolling of RmlUi's lists, as on a phone (Core::Input::IUiInputConsumer::BeginTouchScroll()):
// the box under a dragged finger follows it, a flick glides on (RmlUi's smooth scroll), and a finger
// landing on a gliding box stops it. Item cells, scrollbars, sliders and text fields keep the finger
// for the mouse, whose drag means something there; a slider only a finger setting off sideways, so a
// page of sliders still scrolls up and down.
//
//   RmlTouchScroll scroll;
//   if (scroll.Begin(context, landed))   // the finger moved off the point it landed on (and which way)
//       scroll.Move(from, to);           // each move after
//   scroll.End(lifted, velocity);
class RmlTouchScroll
{
public:
    // @return true if a scrollable box lies under the window point; it follows Move() until End().
    // `firstMove`: the finger's move off that point so far, which tells a slider's sideways drag from
    // a scroll up or down.
    bool Begin(Rml::Context* context, Rml::Vector2f point, Rml::Vector2f firstMove);
    // As above, with the move of the finger that plays the mouse (Core::Input::Touch::Pointer).
    bool Begin(Rml::Context* context, Rml::Vector2f point);
    // The finger moved between two window points: the box moves with it.
    void Move(Rml::Vector2f from, Rml::Vector2f to);
    // The finger lifted at a window point, moving at `velocity` window pixels a second.
    void End(Rml::Vector2f point, Rml::Vector2f velocity);
    // A finger landed: the box still gliding stops where it is. @return true if one was gliding.
    bool StopGlide();
    // Lets go of the boxes; before the context's elements go away.
    void Reset();

private:
    Rml::ObserverPtr<Rml::Element> m_Held;
    Rml::ObserverPtr<Rml::Element> m_Gliding;
    // The scroll offsets the glide ends at.
    Rml::Vector2f m_GlideTarget;
};
} // namespace Render::RmlUi
