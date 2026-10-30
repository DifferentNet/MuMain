#include "stdafx.h"
#include "GameLogic/Events/SeasonMarks.h"

#include "Core/Text/Utf8.h"
#include "Data/GameConfig/GameConfig.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "I18N/All.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"
#include "Render/Sprites/GlobalBitmap.h"
#include "Scenes/SceneCore.h"

#include "json.hpp"

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace GameLogic::SeasonMarks
{
namespace
{
constexpr std::wstring_view Line = L"#BPM|";
// "#BPF|name|season": the season of the seasonal Fenrir a character rides, 0 for none.
constexpr std::wstring_view FenrirLine = L"#BPF|";
// Version 2 of the marks: the #BPM lines and the #BPF lines. A server before the season themes ignores the "2".
constexpr const wchar_t* MarksCommand = L"/bpmarks 2";
// The gold Horn of Fenrir's excellent option (the seasonal Fenrir).
constexpr int GoldFenrirOption = 0x04;

// Splits "a|b|c" into its fields.
std::vector<std::wstring> Split(std::wstring_view text)
{
    std::vector<std::wstring> fields;
    size_t start = 0;
    while (true)
    {
        const size_t bar = text.find(L'|', start);
        fields.emplace_back(text.substr(start, bar == std::wstring_view::npos ? std::wstring_view::npos : bar - start));
        if (bar == std::wstring_view::npos)
            break;
        start = bar + 1;
    }
    return fields;
}

int ToInt(const std::wstring& field)
{
    return static_cast<int>(std::wcstol(field.c_str(), nullptr, 10));
}

void SendCommand(const wchar_t* command)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(command));
}

// Reads [r, g, b] or [r, g, b, a] of whole numbers 0-255.
void ReadBytes(const nlohmann::json& json, const char* key, unsigned char* target, size_t count)
{
    const auto it = json.find(key);
    if (it == json.end() || !it->is_array())
        return;

    for (size_t i = 0; i < count && i < it->size(); ++i)
    {
        if ((*it)[i].is_number())
            target[i] = static_cast<unsigned char>(std::clamp((*it)[i].get<int>(), 0, 255));
    }
}

// Reads [r, g, b] of light factors (0 to about 2).
void ReadLight(const nlohmann::json& json, const char* key, float* target)
{
    const auto it = json.find(key);
    if (it == json.end() || !it->is_array())
        return;

    for (size_t i = 0; i < 3 && i < it->size(); ++i)
    {
        if ((*it)[i].is_number())
            target[i] = std::clamp((*it)[i].get<float>(), 0.f, 4.f);
    }
}

std::wstring Widen(const std::string& text)
{
    return Core::Text::FromUtf8(text);
}

// Reads "name": "Winter I" (every language) or {"en": "Winter I", "ru": "Зима I"}; the same for "hero".
void ReadNames(const nlohmann::json& entry, const char* key, std::map<std::wstring, std::wstring>& names)
{
    const auto it = entry.find(key);
    if (it == entry.end())
        return;

    if (it->is_string())
    {
        names[L"en"] = Widen(it->get<std::string>());
        return;
    }
    if (!it->is_object())
        return;

    for (const auto& [locale, name] : it->items())
    {
        if (name.is_string())
            names[Widen(locale)] = Widen(name.get<std::string>());
    }
}

void ReadFenrir(const nlohmann::json& entry, FenrirLook& fenrir)
{
    const auto it = entry.find("fenrir");
    if (it == entry.end() || !it->is_object())
        return;

    fenrir.Present = true;
    if (const auto texture = it->find("texture"); texture != it->end() && texture->is_string())
    {
        fenrir.Texture = Widen(texture->get<std::string>());
        std::replace(fenrir.Texture.begin(), fenrir.Texture.end(), L'/', L'\\');
    }
    ReadLight(*it, "bodyLight", fenrir.Body);
    ReadLight(*it, "glowLight", fenrir.Glow);
    ReadLight(*it, "eyeLight", fenrir.Eyes);
    ReadLight(*it, "chestLight", fenrir.Chest);
    ReadLight(*it, "lightning", fenrir.Lightning);
    ReadLight(*it, "pawLightning", fenrir.PawLightning);
    ReadLight(*it, "sparks", fenrir.Sparks);
}
// A text in the game's UI language, else in English, else in any; empty when there is none.
std::wstring InUILanguage(const std::map<std::wstring, std::wstring>& texts)
{
    if (texts.empty())
        return {};

    const auto it = texts.find(GameConfig::GetInstance().GetUILocale());
    if (it != texts.end())
        return it->second;
    const auto english = texts.find(L"en");
    return english != texts.end() ? english->second : texts.begin()->second;
}
} // namespace

std::wstring SeasonLook::Name() const
{
    return InUILanguage(Names);
}

std::wstring SeasonLook::HeroName() const
{
    return InUILanguage(HeroNames);
}

Store& Store::Instance()
{
    static Store store;
    return store;
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (view.starts_with(FenrirLine))
    {
        TakeFenrir(view.substr(FenrirLine.size()));
        return true;
    }
    if (!view.starts_with(Line))
        return false;

    // "#BPM|name|season|title|aura"
    const auto fields = Split(view.substr(Line.size()));
    if (fields.size() < 4 || fields[0].empty())
        return true;

    const std::wstring& name = fields[0];
    const int season = ToInt(fields[1]);
    SeasonMark mark;
    mark.Title = ToInt(fields[2]);
    mark.Aura = fields[3] == L"1";

    auto& seasons = m_marks[name];
    if (mark.Title > 0 || mark.Aura)
        seasons[season] = mark;
    else
        seasons.erase(season);

    Recompute(name);
    return true;
}

// "name|season" of a #BPF line: a season above 0 replaces what is known of that rider, 0 forgets it.
void Store::TakeFenrir(std::wstring_view line)
{
    const auto fields = Split(line);
    if (fields.size() < 2 || fields[0].empty())
        return;

    const int season = ToInt(fields[1]);
    if (season > 0)
        m_fenrirs[fields[0]] = season;
    else
        m_fenrirs.erase(fields[0]);
}

void Store::Recompute(const std::wstring& name)
{
    const auto marks = m_marks.find(name);
    Worn worn;
    if (marks != m_marks.end())
    {
        for (const auto& [season, mark] : marks->second)
        {
            // The map is ordered by season, so the last one with a title or an aura wins.
            if (mark.Title > 0)
            {
                worn.TitleSeason = season;
                worn.Title = mark.Title;
            }
            if (mark.Aura)
                worn.AuraSeason = season;
        }
    }

    if (worn.TitleSeason < 0 && worn.AuraSeason < 0)
    {
        m_worn.erase(name);
        if (marks != m_marks.end())
            m_marks.erase(marks);
        return;
    }

    m_worn[name] = worn;
}

void Store::Request()
{
    SendCommand(MarksCommand);
}

void Store::Reset()
{
    m_marks.clear();
    m_worn.clear();
    m_fenrirs.clear();
}

Worn Store::Of(std::wstring_view name) const
{
    if (m_worn.empty())
        return {};

    const auto it = m_worn.find(name);
    return it == m_worn.end() ? Worn{} : it->second;
}

const SeasonLook& Store::Look(int season)
{
    if (!m_looksLoaded)
        LoadLooks();

    // The closest season at or before this one; the first one for a season before all of them.
    const SeasonLook* best = nullptr;
    for (const SeasonLook& look : m_looks)
    {
        if (look.Season <= season && (best == nullptr || look.Season > best->Season))
            best = &look;
    }
    if (best == nullptr && !m_looks.empty())
        best = &*std::min_element(m_looks.begin(), m_looks.end(),
                                  [](const SeasonLook& a, const SeasonLook& b) { return a.Season < b.Season; });
    return best != nullptr ? *best : m_defaultLook;
}

void Store::LoadLooks()
{
    m_looksLoaded = true;
    m_looks.clear();

    std::ifstream file(std::filesystem::path(L"Data") / L"Seasons.json", std::ios::binary);
    if (!file)
        return;

    std::stringstream text;
    text << file.rdbuf();
    const auto json = nlohmann::json::parse(text.str(), nullptr, false);
    if (json.is_discarded() || !json.is_object() || !json.contains("seasons") || !json["seasons"].is_array())
        return;

    for (const auto& entry : json["seasons"])
    {
        if (!entry.is_object() || !entry.contains("season") || !entry["season"].is_number_integer())
            continue;

        SeasonLook look;
        look.Season = entry["season"].get<int>();
        ReadNames(entry, "name", look.Names);
        ReadNames(entry, "hero", look.HeroNames);

        if (const auto title = entry.find("title"); title != entry.end() && title->is_object())
        {
            ReadBytes(*title, "text", look.TitleText, 4);
            ReadBytes(*title, "background", look.TitleBack, 4);
            ReadBytes(*title, "topText", look.TopText, 4);
            ReadBytes(*title, "topBackground", look.TopBack, 4);
        }

        if (const auto aura = entry.find("aura"); aura != entry.end() && aura->is_object())
        {
            if (const auto circle = aura->find("circle"); circle != aura->end() && circle->is_string())
                look.CircleTexture = circle->get<std::string>() == "runes" ? Circle::Runes : Circle::None;
            ReadLight(*aura, "circleColor", look.CircleColor);
            if (const auto particles = aura->find("particles"); particles != aura->end() && particles->is_string())
                look.ParticleKind = particles->get<std::string>() == "snow" ? Particles::Snow : Particles::None;
            ReadLight(*aura, "particleColor", look.ParticleColor);
        }

        ReadFenrir(entry, look.Fenrir);

        m_looks.push_back(std::move(look));
    }
}

const SeasonLook* Store::FenrirSeason()
{
    if (!m_looksLoaded)
        LoadLooks();

    const SeasonLook* newest = nullptr;
    for (const SeasonLook& look : m_looks)
    {
        if (look.Fenrir.Present && (newest == nullptr || look.Season > newest->Season))
            newest = &look;
    }
    return newest;
}

int Store::FenrirSeasonOf(std::wstring_view name) const
{
    if (m_fenrirs.empty())
        return 0;

    const auto it = m_fenrirs.find(name);
    return it == m_fenrirs.end() ? 0 : it->second;
}

int Store::RiderFenrirSeason(const OBJECT* mount) const
{
    if (mount == nullptr || mount->Owner == nullptr || SceneFlag != MAIN_SCENE)
        return 0;

    // The own horn's level is its season, known before the server's line comes.
    if (Hero != nullptr && mount->Owner == &Hero->Object && CharacterMachine != nullptr)
    {
        const ITEM& horn = CharacterMachine->Equipment[EQUIPMENT_HELPER];
        if (horn.Type == ITEM_HORN_OF_FENRIR && (horn.ExcellentFlags & 63) == GoldFenrirOption && horn.Level > 0)
            return horn.Level;
    }

    if (m_fenrirs.empty() || CharactersClient == nullptr)
        return 0;

    for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
    {
        const CHARACTER& rider = CharactersClient[i];
        if (&rider.Object == mount->Owner)
            return FenrirSeasonOf(rider.ID);
    }
    return 0;
}

const SeasonLook* Store::FenrirLookOf(int season)
{
    if (!m_looksLoaded)
        LoadLooks();

    const SeasonLook* best = nullptr;
    if (season > 0)
    {
        for (const SeasonLook& look : m_looks)
        {
            if (look.Fenrir.Present && look.Season <= season && (best == nullptr || look.Season > best->Season))
                best = &look;
        }
    }
    return best != nullptr ? best : FenrirSeason();
}

unsigned int Store::FenrirTexture(const SeasonLook& look)
{
    // Loaded once a season; the game frees its textures only when it closes.
    if (const auto known = m_fenrirTextures.find(look.Season); known != m_fenrirTextures.end())
        return known->second;

    unsigned int texture = 0;
    if (!look.Fenrir.Texture.empty())
    {
        // As the models' own textures load (Data/DataHandler/LoadData.cpp: GL_NEAREST, GL_REPEAT).
        const GLuint index = Bitmaps.LoadImage(L"Data\\" + look.Fenrir.Texture, GL_NEAREST, GL_REPEAT);
        texture = index == BITMAP_UNKNOWN ? 0 : index;
    }
    m_fenrirTextures.emplace(look.Season, texture);
    return texture;
}

std::wstring Store::NameOf(int season)
{
    const SeasonLook& look = Look(season);
    return look.Season == season ? look.Name() : std::wstring{};
}

std::wstring Store::HeroNameOf(int season)
{
    const SeasonLook& look = Look(season);
    return look.Season == season ? look.HeroName() : std::wstring{};
}

std::wstring TrophyName(int season)
{
    wchar_t text[160] = {};
    const std::wstring name = Marks().NameOf(season);
    if (name.empty())
        mu_swprintf_s(text, I18N::Game::SeasonTrophyName, season);
    else
        mu_swprintf_s(text, I18N::Game::SeasonTrophyNamed, name.c_str());
    return text;
}

std::wstring FenrirName(int season)
{
    wchar_t text[160] = {};
    const std::wstring name = Marks().NameOf(season);
    if (name.empty())
        mu_swprintf_s(text, I18N::Game::SeasonFenrirName, season);
    else
        mu_swprintf_s(text, I18N::Game::SeasonFenrirNamed, name.c_str());
    return text;
}

namespace
{
// A new character: drop the last one's lines and ask the server for this one's.
void ForgetAndAsk()
{
    Marks().Reset();
    Marks().Request();
}

// The battle pass' auras and titles lines and the seasonal Fenrirs' lines stay out of the chat
// (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"SeasonMarks",
    .prefixes = {Line, FenrirLine},
    .take = [](const std::wstring& text) { return Marks().TakeMessage(text); },
    .enteredWorld = ForgetAndAsk,
});
} // namespace
} // namespace GameLogic::SeasonMarks
