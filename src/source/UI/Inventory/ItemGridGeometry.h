#pragma once

// Where an item grid's cells are, in window pixels: the theme's .item-grid box and
// .item-cell pitch (CInventoryCtrl::FollowGridPx()), or the original's 20-unit cells until the
// document has laid out.
namespace UI::Items
{
struct GridRect
{
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;

    bool operator==(const GridRect&) const = default;
};

class GridGeometry
{
public:
    static constexpr float DefaultPitch = 20.f;

    GridGeometry() = default;
    GridGeometry(float left, float top, float pitchX, float pitchY, int columns, int rows);

    float Left() const { return m_Left; }
    float Top() const { return m_Top; }
    float PitchX() const { return m_PitchX; }
    float PitchY() const { return m_PitchY; }
    int Columns() const { return m_Columns; }
    int Rows() const { return m_Rows; }
    GridRect Bounds() const { return {m_Left, m_Top, m_Columns * m_PitchX, m_Rows * m_PitchY}; }
    // The part of the grid on view, in window pixels, where a theme shows only some of its rows
    // (a .item-grid-viewport): a point outside it misses the grid.
    void SetVisibleRect(const GridRect& visible);

    bool Contains(float x, float y) const;
    // The cell holding (x, y); false outside the grid.
    bool CellAt(float x, float y, int& column, int& row) const;
    // The cell (x, y) falls in, also outside the grid, one further out on the negative side as
    // the original counted it: where an item hanging off the grid starts.
    void CellOf(float x, float y, int& column, int& row) const;
    // The box of `width` x `height` cells from (column, row).
    GridRect CellsRect(int column, int row, int width, int height) const;

    bool operator==(const GridGeometry&) const = default;

private:
    float m_Left = 0.f;
    float m_Top = 0.f;
    float m_PitchX = DefaultPitch;
    float m_PitchY = DefaultPitch;
    int m_Columns = 0;
    int m_Rows = 0;
    bool m_HasVisibleRect = false;
    GridRect m_VisibleRect;
};

// Where the pointer holds a dragged item, in cells from its top-left, so a drag keeps its hold
// across grids of different pitches.
struct PickupAnchor
{
    float column = 0.f;
    float row = 0.f;
};

// The item's top-left under the pointer, in a grid of the given pitch.
void AnchoredTopLeft(float pointerX, float pointerY, const PickupAnchor& anchor, float pitchX, float pitchY,
                     int& left, int& top);

// The middle of the item's top-left square under the pointer: the cell under it is where the item
// goes, as in the original (which held items by their middle). The item then lands where it covers
// the most; its top-left corner lies on a cell border when it is held by its middle, so a drop that
// looks at the corner lands a cell short, or outside the grid on its first row or column.
void AnchoredTargetPoint(float pointerX, float pointerY, const PickupAnchor& anchor, float pitchX, float pitchY,
                         int& x, int& y);
} // namespace UI::Items
