#pragma once

#include <chrono>
#include <string>

// The vault's pages, from our server's add-on (MU Moldova, MultiVault): an account has several vaults, and the
// vault window switches between them with the arrows beside its title.
//
// The window asks "/vault" when it opens; the server answers "#VLT|page|pages". An arrow sends "/vault n"; the
// server answers "#VLS|page|pages" and then sends page n's items and the vault zen, so on "#VLS" the vault grids
// are emptied before the new items come. Both answers come as blue server messages, which this takes out of the
// chat. A server without the add-on never answers, and the window then shows no page and no arrows.
namespace UI::Storage
{
class VaultPages
{
public:
    static VaultPages& Instance();

    // Takes a server message. True when it was a vault page line, which then stays out of the chat.
    bool TakeMessage(const std::wstring& text);

    // Asks which page the open vault shows.
    void RequestInfo();

    // Asks for page `page` (from 1). Nothing happens while an earlier switch waits for its answer.
    void RequestPage(int page);

    bool HasInfo() const { return m_pages > 0; }
    int Page() const { return m_page; }
    int Pages() const { return m_pages; }

    // A switch was asked for and its answer hasn't come yet (for a few seconds at most).
    bool IsSwitching() const;

    // Forgets the page, when the vault closes.
    void Reset();

private:
    int m_page = 0;
    int m_pages = 0;
    std::chrono::steady_clock::time_point m_switchAskedAt{};
};

inline VaultPages& Pages()
{
    return VaultPages::Instance();
}
} // namespace UI::Storage
