#include "stdafx.h"
#include "UI/Inventory/VaultPages.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/StorageInventory.h"
#include "UI/Inventory/StorageInventoryExt.h"

#include <cwchar>
#include <string_view>

namespace UI::Storage
{
namespace
{
constexpr std::wstring_view InfoAnswer = L"#VLT|";
constexpr std::wstring_view SwitchAnswer = L"#VLS|";
// A switch without an answer this long is given up, so the arrows work again.
constexpr auto SwitchTimeout = std::chrono::seconds(4);

void SendCommand(const std::wstring& command)
{
    if (SocketClient == nullptr || Hero == nullptr)
        return;

    // Sent as a chat line, as typed commands are; the server takes lines starting with a slash as commands.
    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(command.c_str()));
}

// "page|pages" -> both, or false.
bool ParsePage(std::wstring_view text, int& page, int& pages)
{
    const size_t bar = text.find(L'|');
    if (bar == std::wstring_view::npos)
        return false;

    const std::wstring pageText(text.substr(0, bar));
    const std::wstring pagesText(text.substr(bar + 1));
    page = static_cast<int>(std::wcstol(pageText.c_str(), nullptr, 10));
    pages = static_cast<int>(std::wcstol(pagesText.c_str(), nullptr, 10));
    return page >= 1 && pages >= page;
}
} // namespace

VaultPages& VaultPages::Instance()
{
    static VaultPages pages;
    return pages;
}

bool VaultPages::TakeMessage(const std::wstring& text)
{
    const std::wstring_view view(text);
    const bool switched = view.starts_with(SwitchAnswer);
    if (!switched && !view.starts_with(InfoAnswer))
        return false;

    int page = 0;
    int pages = 0;
    if (!ParsePage(view.substr(InfoAnswer.size()), page, pages))
        return true;

    m_page = page;
    m_pages = pages;
    m_switchAskedAt = {};

    // The new page's items follow this line: empty both grids for them. An item on the cursor goes back first.
    if (switched && g_pNewUISystem != nullptr && g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_STORAGE))
    {
        g_pStorageInventory->ClearForPageSwitch();
        g_pStorageInventoryExt->ClearForPageSwitch();
    }

    return true;
}

void VaultPages::RequestInfo()
{
    SendCommand(L"/vault");
}

void VaultPages::RequestPage(int page)
{
    if (IsSwitching() || page < 1 || (m_pages > 0 && page > m_pages))
        return;

    m_switchAskedAt = std::chrono::steady_clock::now();
    SendCommand(L"/vault " + std::to_wstring(page));
}

bool VaultPages::IsSwitching() const
{
    return m_switchAskedAt != std::chrono::steady_clock::time_point{}
        && std::chrono::steady_clock::now() - m_switchAskedAt < SwitchTimeout;
}

void VaultPages::Reset()
{
    m_page = 0;
    m_pages = 0;
    m_switchAskedAt = {};
}
} // namespace UI::Storage
