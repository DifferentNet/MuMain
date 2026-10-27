#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The marks of the battle pass' seasons (stage 6): the title «Hero of Season N» above a character's name
// (UI/Chat/Chat.cpp) and the season's aura, a rune circle under the feet with the season's falling particles
// (Render/Effects/SeasonAura.cpp).
//
// Our server's add-on (MU Moldova, BattlePassMarks) answers the hidden chat command "/bpmarks", which this sends
// once the character entered the world, with a blue server message per marked character in the game, and from then
// on sends one for each character that enters the world and for each new mark. This takes them out of the chat:
//   "#BPM|name|season|title 0/1/2|aura 0/1"
// A line replaces what is known about that character in that season (both 0: nothing left of it). A character
// wears the title of its latest season with one and the aura of its latest season with one. Title 2 is the gold
// one of the season's Hall of Fame (its first ten accounts at the last level); 1 is the usual, ice blue one.
//
// How each season looks comes from Data/Seasons.json (the circle's texture and colour, the particles, the title's
// colours; stage 7 adds the seasonal Fenrir's palette). A season missing there looks like the closest season
// before it that is there.
namespace GameLogic::SeasonMarks
{
enum class Circle
{
    None,
    // Effect/mzine_typer2 (BITMAP_MAGIC_ZIN): runes in a ring; only the Summoner's curses use it otherwise.
    Runes,
};

enum class Particles
{
    None,
    // Santa's snow (Effect/snowseff01, snowseff02), falling.
    Snow,
};

// How a season's marks look.
struct SeasonLook
{
    int Season = 0;
    std::wstring Name;

    // The title line: text and background, 0-255.
    unsigned char TitleText[4] = {165, 225, 255, 255};
    unsigned char TitleBack[4] = {0, 22, 45, 190};
    // The gold title of the Hall of Fame (title 2).
    unsigned char TopText[4] = {255, 214, 110, 255};
    unsigned char TopBack[4] = {30, 20, 0, 190};

    Circle CircleTexture = Circle::Runes;
    float CircleColor[3] = {0.35f, 0.65f, 1.0f};
    Particles ParticleKind = Particles::Snow;
    float ParticleColor[3] = {0.75f, 0.9f, 1.0f};
};

// What a character wears: the seasons of its title and aura, -1 for none; Title 1 the usual title, 2 the gold one.
struct Worn
{
    int TitleSeason = -1;
    int Title = 0;
    int AuraSeason = -1;
};

class Store
{
public:
    static Store& Instance();

    // Takes a server message. True when it was a marks line, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server for the marks of the characters in the game ("/bpmarks").
    void Request();

    // Forgets the marks, e.g. when the character changed; the next Request() brings them again.
    void Reset();

    // What a character wears (by its name as the game shows it).
    Worn Of(std::wstring_view name) const;

    // How a season looks; read from Data/Seasons.json on first use.
    const SeasonLook& Look(int season);

    // How many characters are known to wear something.
    size_t Count() const { return m_worn.size(); }

private:
    struct NameHash
    {
        using is_transparent = void;
        size_t operator()(std::wstring_view name) const noexcept { return std::hash<std::wstring_view>{}(name); }
    };

    struct SeasonMark
    {
        int Title = 0;
        bool Aura = false;
    };

    void Recompute(const std::wstring& name);
    void LoadLooks();

    std::unordered_map<std::wstring, std::map<int, SeasonMark>, NameHash, std::equal_to<>> m_marks;
    std::unordered_map<std::wstring, Worn, NameHash, std::equal_to<>> m_worn;
    std::vector<SeasonLook> m_looks;
    SeasonLook m_defaultLook;
    bool m_looksLoaded = false;
};

inline Store& Marks()
{
    return Store::Instance();
}
} // namespace GameLogic::SeasonMarks
