#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

// The battle pass mark «Echo» (MU Skynet, ghinzuro 2026-11-22, variant 3; Season 2 «Winter II», level 50): violet
// afterimages of the hero and its mount follow it while it moves (drawn by Render/Effects/SeasonEcho.h). The server's
// add-on sends the "#BPE" lines (GameLogic/Events/SeasonEchoLine.h) to a client that asked "/bpmarks 3"
// (GameLogic/Events/SeasonMarks.cpp asks once the character entered the world; a server before the echo answers it
// as "/bpmarks 2" and sends none). This takes them out of the chat and keeps:
//   - each character's shown echo season ("#BPE|name|season", 0 = none or hidden by its owner);
//   - the own character's newest owned echo season and its switch (the owner line "#BPE|name|season|hidden").
// The owner turns its echo on or off in the Titles window, which sends "/bpecho on" or "/bpecho off" through
// SetShown(); the server's next owner line shows the result.
namespace GameLogic::SeasonEcho
{
class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was an echo line, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Forgets every echo, e.g. when the character changed; the next "/bpmarks 3" answer brings them again.
    void Reset();

    // The season of the echo a character shows (by its name as the game shows it), 0 for none.
    int ShownSeasonOf(std::wstring_view name) const;

    // The own character's newest echo season (owner line), 0 when it owns none or no owner line came yet.
    int OwnedSeason() const;

    // The own character turned its echo off (owner line).
    bool OwnHidden() const;

    // The season of the own character's echo to draw, 0 for none: the owner line's when one came (owned and not
    // hidden), else the own name's shown line.
    int OwnShownSeason() const;

    // Asks the server to show or hide the own echo ("/bpecho on" / "/bpecho off", not shown in the chat).
    void SetShown(bool shown) const;

    // How many other characters than the one played now show an echo.
    size_t OthersShownCount() const;

    // Grows whenever something known changes, so a window can tell it has to refresh.
    unsigned Version() const
    {
        return m_version;
    }

private:
    struct NameHash
    {
        using is_transparent = void;
        size_t operator()(std::wstring_view name) const noexcept
        {
            return std::hash<std::wstring_view>{}(name);
        }
    };

    // Whether the owner line is for the character played now.
    bool OwnerLineIsOurs() const;

    // The shown echo seasons, only the ones above 0.
    std::unordered_map<std::wstring, int, NameHash, std::equal_to<>> m_shown;
    // The last owner line: whose it was, the newest owned season and the switch.
    std::wstring m_ownerName;
    int m_ownedSeason = 0;
    bool m_ownHidden = false;
    bool m_hasOwnerLine = false;
    unsigned m_version = 0;
};

inline Store& Echoes()
{
    return Store::Instance();
}

// «Echo «Winter II»» (the battle pass level 50 reward of Season 2); «Season N echo» for a season
// Data/Seasons.json doesn't have.
std::wstring EchoName(int season);

// The echo's one-line description (the battle pass window's hint).
std::wstring EchoAbout();
} // namespace GameLogic::SeasonEcho
