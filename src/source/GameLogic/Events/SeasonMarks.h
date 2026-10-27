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
// colours, the seasonal Fenrir's texture and palette). A season missing there looks like the closest season before
// it that is there.
//
// The seasonal Fenrir (stage 7) is the gold Horn of Fenrir (13:37, excellent option 0x04) with the season's number
// as its level, the level 50 reward; the trophy (13:128) is named by its level too. Other players'
// clients learn only "gold" from the appearance packet (no pet level), so every gold Fenrir is drawn with the
// newest season's Fenrir look in Seasons.json (Engine/Object/ZzzObject.cpp); the horn's level only names the
// season in its tooltip.
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

// How a season's Fenrir looks: its body texture and lights as light factors [r, g, b] (1 = full).
struct FenrirLook
{
    bool Present = false;
    // The body's texture under Data (e.g. Skill/fenril_winter.jpg); empty keeps the gold one.
    std::wstring Texture;
    // The body's texture pass and its bright chrome pass (the glow).
    float Body[3] = {1.0f, 1.0f, 1.0f};
    float Glow[3] = {1.0f, 1.0f, 1.0f};
    // The two eye lights and the chest light.
    float Eyes[3] = {0.9f, 0.2f, 0.1f};
    float Chest[3] = {1.0f, 0.3f, 0.2f};
    // The lightning around the wolf, under its paws and the sparks of its skill.
    float Lightning[3] = {0.8f, 0.8f, 0.1f};
    float PawLightning[3] = {1.0f, 1.0f, 1.0f};
    float Sparks[3] = {1.0f, 0.0f, 0.0f};
};

// How a season's marks look.
struct SeasonLook
{
    int Season = 0;
    // The season's name by UI locale ("en", "ru", ...); "en" for any other.
    std::map<std::wstring, std::wstring> Names;

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

    FenrirLook Fenrir;

    // The name in the game's UI language; empty when it has none.
    std::wstring Name() const;
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

    // The newest season that has a Fenrir look: every gold Fenrir is drawn like this. Null when none has one.
    const SeasonLook* FenrirSeason();

    // The texture of the newest Fenrir look (loaded on first use), 0 for none: the model keeps its own.
    unsigned int FenrirTexture();

    // A season's name in the UI language when Data/Seasons.json has that very season, else empty.
    std::wstring NameOf(int season);

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
    unsigned int m_fenrirTexture = 0;
    bool m_fenrirTextureTried = false;
};

inline Store& Marks()
{
    return Store::Instance();
}

// «Trophy of Season N» (13:128 at level N).
std::wstring TrophyName(int season);

// «Seasonal Fenrir · Season N «name»» (a gold Horn of Fenrir at level N).
std::wstring FenrirName(int season);
} // namespace GameLogic::SeasonMarks
