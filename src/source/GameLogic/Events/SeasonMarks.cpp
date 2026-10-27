#include "stdafx.h"
#include "GameLogic/Events/SeasonMarks.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"

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
constexpr const wchar_t* MarksCommand = L"/bpmarks";

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
    std::wstring wide;
    for (const unsigned char ch : text)
        wide.push_back(ch < 0x80 ? static_cast<wchar_t>(ch) : L'?');
    return wide;
}
} // namespace

Store& Store::Instance()
{
    static Store store;
    return store;
}

bool Store::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
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
        if (entry.contains("name") && entry["name"].is_string())
            look.Name = Widen(entry["name"].get<std::string>());

        if (const auto title = entry.find("title"); title != entry.end() && title->is_object())
        {
            ReadBytes(*title, "text", look.TitleText, 4);
            ReadBytes(*title, "background", look.TitleBack, 4);
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

        m_looks.push_back(std::move(look));
    }
}
} // namespace GameLogic::SeasonMarks
