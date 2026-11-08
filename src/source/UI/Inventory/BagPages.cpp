#include "stdafx.h"
#include "UI/Inventory/BagPages.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/HiddenChatLines.h"
#include "Network/Server/WSclient.h"

#include <cwchar>
#include <string_view>

namespace UI::Inventory
{
namespace
{
constexpr std::wstring_view InfoAnswer = L"#BGP|";
constexpr std::wstring_view BuyAnswer = L"#BGB|";
// Two OpenMU inventory extensions make one bag page.
constexpr int ExtensionsPerPage = 2;
// A purchase without an answer this long is given up, so the button works again.
constexpr auto BuyTimeout = std::chrono::seconds(6);

void SendCommand(const std::wstring& command)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(command.c_str()));
}
} // namespace

BagPages& BagPages::Instance()
{
    static BagPages pages;
    return pages;
}

int BagPages::OwnedPages() const
{
    const int extensions = CharacterAttribute != nullptr ? CharacterAttribute->InventoryExtensions : 0;
    return 1 + extensions / ExtensionsPerPage;
}

bool BagPages::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    if (view.starts_with(BuyAnswer))
    {
        m_buyAskedAt = {};
        return true;
    }

    if (!view.starts_with(InfoAnswer))
        return false;

    // "pages|max|price"
    const std::wstring rest(view.substr(InfoAnswer.size()));
    wchar_t* end = nullptr;
    const long pages = std::wcstol(rest.c_str(), &end, 10);
    if (end == nullptr || *end != L'|')
        return true;
    const long maxPages = std::wcstol(end + 1, &end, 10);
    if (end == nullptr || *end != L'|')
        return true;
    const long long price = std::wcstoll(end + 1, nullptr, 10);
    if (pages < 1 || maxPages < pages)
        return true;

    m_maxPages = static_cast<int>(maxPages);
    m_price = price;

    // A purchase opens the page at once: the server built the extensions, the client shows them.
    if (CharacterAttribute != nullptr)
    {
        const int extensions = static_cast<int>(pages - 1) * ExtensionsPerPage;
        if (CharacterAttribute->InventoryExtensions < extensions)
            CharacterAttribute->InventoryExtensions = static_cast<BYTE>(extensions);
    }

    return true;
}

void BagPages::RequestInfoOnce()
{
    if (m_asked)
        return;
    m_asked = true;
    SendCommand(L"/bag");
}

void BagPages::Buy()
{
    if (IsBuying())
        return;

    m_buyAskedAt = std::chrono::steady_clock::now();
    SendCommand(L"/bag buy");
}

bool BagPages::IsBuying() const
{
    return m_buyAskedAt != std::chrono::steady_clock::time_point{}
        && std::chrono::steady_clock::now() - m_buyAskedAt < BuyTimeout;
}

void BagPages::Reset()
{
    m_maxPages = 0;
    m_price = 0;
    m_asked = false;
    m_buyAskedAt = {};
}

namespace
{
// The bag pages' lines stay out of the chat (Network/Server/HiddenChatLines.h); a new world entry asks again.
[[maybe_unused]] const bool Registered = Network::HiddenChatLines::Register({
    .name = L"BagPages",
    .prefixes = {InfoAnswer, BuyAnswer},
    .take = [](const std::wstring& text) { return Bag().TakeMessage(text); },
    .enteredWorld = [] { Bag().Reset(); },
});
} // namespace
} // namespace UI::Inventory
