#include "stdafx.h"
#include "GameLogic/Events/SeasonEchoStore.h"

#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Events/SeasonEchoLine.h"
#include "GameLogic/Events/SeasonMarks.h"
#include "I18N/All.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"

namespace GameLogic::SeasonEcho
{
namespace
{
void SendCommand(const wchar_t* command)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(command));
}

std::wstring_view HeroName()
{
    return Hero != nullptr ? std::wstring_view(Hero->ID) : std::wstring_view();
}
} // namespace

Store& Store::Instance()
{
    static Store store;
    return store;
}

bool Store::TakeMessage(const std::wstring& text)
{
    if (!std::wstring_view(text).starts_with(SeasonEchoLine::Prefix))
        return false;

    // A line with our tag stays out of the chat even when it can't be read.
    const auto line = SeasonEchoLine::Parse(text);
    if (!line)
        return true;

    if (line->owner)
    {
        m_ownerName = line->name;
        m_ownedSeason = line->season;
        m_ownHidden = line->hidden;
        m_hasOwnerLine = true;
    }
    else if (line->season > 0)
    {
        m_shown[line->name] = line->season;
    }
    else
    {
        m_shown.erase(line->name);
    }
    ++m_version;
    return true;
}

void Store::Reset()
{
    m_shown.clear();
    m_ownerName.clear();
    m_ownedSeason = 0;
    m_ownHidden = false;
    m_hasOwnerLine = false;
    ++m_version;
}

int Store::ShownSeasonOf(std::wstring_view name) const
{
    if (m_shown.empty() || name.empty())
        return 0;
    const auto it = m_shown.find(name);
    return it != m_shown.end() ? it->second : 0;
}

size_t Store::OthersShownCount() const
{
    // The own name's shown line doesn't count: the own echo is not looked for among the others.
    const std::wstring_view hero = HeroName();
    const bool heroShown = !hero.empty() && m_shown.find(hero) != m_shown.end();
    return m_shown.size() - (heroShown ? 1 : 0);
}

bool Store::OwnerLineIsOurs() const
{
    return m_hasOwnerLine && !m_ownerName.empty() && HeroName() == m_ownerName;
}

int Store::OwnedSeason() const
{
    return OwnerLineIsOurs() ? m_ownedSeason : 0;
}

bool Store::OwnHidden() const
{
    return OwnerLineIsOurs() && m_ownHidden;
}

int Store::OwnShownSeason() const
{
    if (OwnerLineIsOurs())
        return m_ownedSeason > 0 && !m_ownHidden ? m_ownedSeason : 0;
    return ShownSeasonOf(HeroName());
}

void Store::SetShown(bool shown) const
{
    SendCommand(shown ? L"/bpecho on" : L"/bpecho off");
}

std::wstring EchoName(int season)
{
    wchar_t text[160] = {};
    const std::wstring name = SeasonMarks::Marks().NameOf(season);
    if (name.empty())
        mu_swprintf_s(text, I18N::SeasonEcho::EchoNumbered, season);
    else
        mu_swprintf_s(text, I18N::SeasonEcho::EchoNamed, name.c_str());
    return text;
}

std::wstring EchoAbout()
{
    return I18N::SeasonEcho::EchoAbout;
}

namespace
{
// A new character: drop the last one's echo lines. SeasonMarks asks "/bpmarks 3" right after (its enteredWorld runs
// later: the handlers go in the order of their first prefixes, "#BPE|" before "#BPM|"), and the answer brings them.
void Forget()
{
    Echoes().Reset();
}

// The echo lines stay out of the chat (Network/Server/HiddenChatLines.h).
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"SeasonEcho",
    .prefixes = {SeasonEchoLine::Prefix},
    .take = [](const std::wstring& text) { return Echoes().TakeMessage(text); },
    .enteredWorld = Forget,
});
} // namespace
} // namespace GameLogic::SeasonEcho
