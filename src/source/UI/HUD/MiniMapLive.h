#pragma once

#include <chrono>
#include <string>
#include <vector>

// The live event monsters of the big map (Tab), MU Moldova: the Golden Invasion's goldens, the White Wizard and
// the world boss of the hero's map, as our server's add-on (LiveMapMarkers.cs) answers "/mmlive". Only the square
// of `cell` x `cell` tiles each is in is known (ghinzuro 2026-11-07: rounded, no treasure goblins), so the map
// draws the monster's skull at the square's middle with a circle around the square.
//
// The answer is hidden blue messages: "#MML0|map|cell", then "#MML|kind|x|y|count|name" per square (kind g, w or
// b), then "#MML.|". The map asks every 5 s while it is open and forgets the list when it closes.
namespace UI::MiniMapLive
{
struct Entry
{
    wchar_t kind = L'g'; // g golden, w White Wizard, b world boss
    int x = 0, y = 0;    // the square's middle, tiles
    int count = 1;
    std::wstring name;
};

class Store
{
public:
    static Store& Instance();

    bool TakeMessage(const std::wstring& text);

    // While the map is open: asks now if it never did since it opened, then every 5 s.
    void Request();
    // When the map closes or the character changes.
    void Reset();

    // The last whole answer, if it was for this map.
    const std::vector<Entry>* EntriesFor(int map) const;
    int Cell() const { return m_cell; }
    unsigned Version() const { return m_version; }

private:
    std::vector<Entry> m_entries, m_pending;
    int m_map = -1, m_pendingMap = -1;
    int m_cell = 16, m_pendingCell = 16;
    bool m_receiving = false;
    unsigned m_version = 0;
    std::chrono::steady_clock::time_point m_lastRequest{};
};

inline Store& Live()
{
    return Store::Instance();
}
} // namespace UI::MiniMapLive
