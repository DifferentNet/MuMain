#pragma once

namespace Rml
{
class ElementDocument;
}

// Paging buttons for an item grid a theme shows only part of (the phone view's 15-row shop and
// vault): a .item-grid-scroll element with data-grid-scroll="-1" or "1" moves its document's
// #grid_viewport up or down by the rows on view, less one that stays in sight (or the viewport's
// data-grid-rows-kept). The grid's hit tests follow the scrolled cells (UI::Items::DrawnGridGeometry()).
// A button that cannot move the viewport any further (up at the first rows, down at the last) has the
// class at-end, which the theme dims.
namespace UI::Items::GridScroll
{
// Listens for the document's paging buttons, if it has any.
void Attach(Rml::ElementDocument* document);
// Marks the buttons' ends again, for a grid whose height changed with the document already showing.
void Sync(Rml::ElementDocument* document);
} // namespace UI::Items::GridScroll
