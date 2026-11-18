#include "stdafx.h"

#include "UI/Windows/LoginSceneUpdates.h"

#include "App/Control/ControlServer.h"
#include "Character/CharInfoBalloonMng.h"
#include "Character/CharSelMainWin.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Network/Server/ServerGroup.h"
#include "Network/Server/ServerListManager.h"
#include "UI/Core/SceneUICoordinator.h"
#include "UI/Windows/CreditWin.h"
#include "UI/Windows/LoginMainWin.h"
#include "UI/Windows/LoginWin.h"
#include "UI/Windows/ServerSelWin.h"

#include <cstdlib>
#include <string>

namespace
{
// MU Skynet runs one server: the first filled list of a client start connects to it without the two
// clicks (group, then server). Later lists (back to server selection, a failed connect) show the list.
bool s_singleServerListSeen = false;

// The one group's server count, or -1 when there is not exactly one group.
int ServersOfTheOnlyGroup()
{
    if (g_ServerListManager->GetServerGroupSize() != 1)
    {
        return -1;
    }
    CServerGroup* group = nullptr;
    g_ServerListManager->SetFirst();
    if (!g_ServerListManager->GetNext(group) || group == nullptr)
    {
        return -1;
    }
    return group->GetServerSize();
}

// Never while the control socket runs: the stand scripts pick the server through it.
void ConnectToTheOnlyServerOnce()
{
    if (s_singleServerListSeen || App::Control::ControlServer::Instance().IsRunning())
    {
        return;
    }
    const int servers = ServersOfTheOnlyGroup();
    if (servers == 0)
    {
        return; // the group's servers have not arrived yet
    }
    s_singleServerListSeen = true;
    if (servers == 1)
    {
        g_ServerSelWin.SelectServer(nullptr, 0);
    }
}
} // namespace

namespace UI::LoginScene
{
void ServerListReceived()
{
    if (std::getenv("MU_INPUT_DIAGNOSTICS") != nullptr)
    {
        mu::log::Get("input")->info(
            "[InputDiag] server-list groups={} selector(show={},active={}) login-main(show={},active={}) credits={}",
            g_ServerListManager->GetServerGroupSize(), g_ServerSelWin.IsVisible(), g_ServerSelWin.IsActive(),
            g_LoginMainWin.IsVisible(), g_LoginMainWin.IsActive(), g_CreditWin.IsVisible());
    }
    if (!g_CreditWin.IsVisible())
    {
        g_ServerSelWin.Show(true);
        g_ServerSelWin.UpdateDisplay();
        g_LoginMainWin.Show(true);
        ConnectToTheOnlyServerOnce();
    }
}

void ShowLoginWindow()
{
    g_LoginWin.Show(true);
    g_LoginWin.FocusUsername();
}

void HideLoginWindow()
{
    g_LoginWin.Show(false);
}

void ShowMessage(int messageCode)
{
    CSceneUICoordinator::Instance().PopUpMsgWin(messageCode);
}

void CloseMessage()
{
    CSceneUICoordinator::Instance().CloseMsgWin();
}

void AddServerMessage(std::wstring_view text)
{
    std::wstring message(text);
    CSceneUICoordinator::Instance().AddServerMsg(message.data());
}

void CharacterCreated()
{
    CSceneUICoordinator::Instance().CloseMsgWin();
    g_CharSelMainWin.UpdateDisplay();
    g_CharInfoBalloonMng.UpdateDisplay();
}
}
