#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class OBJECT;

// The marks of the battle pass' seasons (stage 6): the season's title above a character's name (drawn by the
// titles now, UI/Chat/Chat.cpp) and the season's aura, a rune circle under the feet with the season's falling
// particles (Render/Effects/SeasonAura.cpp).
//
// Our server's add-on (MU Moldova, BattlePassMarks) answers the hidden chat command "/bpmarks 2", which this sends
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
// as its level, the level 50 reward; the trophy (13:128) is named by its level too. Other players' clients learn
// only "gold" from the appearance packet (no pet level), so the server (the season themes, 2026-10-29) also sends a
// client that asked "/bpmarks 2" the season of every rider's seasonal Fenrir: in the answer, when such a character
// enters the world, when a pet slot changes, and 0 when it holds none any more or the character left:
//   "#BPF|name|season"
// A gold Fenrir wears its rider's season's look (Engine/Object/ZzzObject.cpp): the own one by the horn in the own
// pet slot, the others' by these lines. One whose season isn't known (a server before the themes, which takes
// "/bpmarks 2" as "/bpmarks"; the character select; the photo viewer) wears the newest season's Fenrir look.
//
// The seasons go in pairs of themes: Season 1 «Winter I», 2 «Winter II», then Spring, Summer and Autumn. Their names
// in Seasons.json name the trophy, the seasonal Fenrir and the battle pass window's marks; a season Seasons.json
// doesn't have is named by its number.
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
    // The season's title («Hero of Winter I») by UI locale, for the battle pass window.
    std::map<std::wstring, std::wstring> HeroNames;

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

    // The season's title in the game's UI language; empty when it has none.
    std::wstring HeroName() const;
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

    // Takes a server message. True when it was a marks or Fenrir line, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks the server for the marks and the seasonal Fenrirs of the characters in the game ("/bpmarks 2").
    void Request();

    // Forgets the marks and the Fenrirs, e.g. when the character changed; the next Request() brings them again.
    void Reset();

    // What a character wears (by its name as the game shows it).
    Worn Of(std::wstring_view name) const;

    // How a season looks; read from Data/Seasons.json on first use.
    const SeasonLook& Look(int season);

    // The newest season that has a Fenrir look: a gold Fenrir whose season isn't known wears it. Null when none has
    // one.
    const SeasonLook* FenrirSeason();

    // The season of the seasonal Fenrir a character rides (by its name as the game shows it; #BPF), 0 when not known.
    int FenrirSeasonOf(std::wstring_view name) const;

    // The season of a gold Fenrir mount: for the own character the level of the horn in its pet slot, else the
    // rider's #BPF line; 0 when not known (outside the game world, a rider that isn't a character in the game).
    int RiderFenrirSeason(const OBJECT* mount) const;

    // The Fenrir look of a season: the closest season at or before it that has one; the newest one for 0 (not known)
    // and for a season before all of them. Null when none has one.
    const SeasonLook* FenrirLookOf(int season);

    // The texture of a season look's Fenrir (loaded on first use, once a season), 0 for none: the model keeps its own.
    unsigned int FenrirTexture(const SeasonLook& look);

    // A season's name in the UI language when Data/Seasons.json has that very season, else empty.
    std::wstring NameOf(int season);

    // A season's title («Hero of Winter I») in the UI language when Data/Seasons.json has that very season, else
    // empty.
    std::wstring HeroNameOf(int season);

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
    void TakeFenrir(std::wstring_view fields);
    void LoadLooks();

    std::unordered_map<std::wstring, std::map<int, SeasonMark>, NameHash, std::equal_to<>> m_marks;
    std::unordered_map<std::wstring, Worn, NameHash, std::equal_to<>> m_worn;
    // The season of each rider's seasonal Fenrir (#BPF), only the ones above 0.
    std::unordered_map<std::wstring, int, NameHash, std::equal_to<>> m_fenrirs;
    std::vector<SeasonLook> m_looks;
    SeasonLook m_defaultLook;
    bool m_looksLoaded = false;
    // The Fenrir textures by the season of their look (0: none, the model keeps its own).
    std::map<int, unsigned int> m_fenrirTextures;
};

inline Store& Marks()
{
    return Store::Instance();
}

// «Trophy «Winter I»» (13:128 at level N); «Trophy of Season N» for a season Data/Seasons.json doesn't have.
std::wstring TrophyName(int season);

// «Seasonal Fenrir «Winter I»» (a gold Horn of Fenrir at level N); «Seasonal Fenrir · Season N» for a season
// Data/Seasons.json doesn't have.
std::wstring FenrirName(int season);
} // namespace GameLogic::SeasonMarks
