
#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Party/PartyInfoWindow.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Core/WindowGeometry.h"
#include "UI/HUD/TouchControls.h"
#include "UI/Mobile/InterfaceMode.h"
#include "UI/Scaling/UITransform.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlWindowClose.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Core/Utilities/StringUtils.h"
#include "GameLogic/Events/CSChaosCastle.h"
#include "Audio/DSPlaySound.h"
#include "GameLogic/Events/w_CursedTemple.h"
#include "World/MapInfra/MapManager.h"
#include "UI/Dialogs/GenericConfirmDialog.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include <array>
#include <span>

using namespace SEASON3B;
using namespace mu::ui::window;

namespace
{
// The hint with no party: how to make one with the mouse (lines 0-4), then why (5-6). The phone view leaves the
// mouse lines out.
constexpr std::size_t kMouseHintLines = 5;

std::array<const wchar_t*, 7> EmptyStateTexts()
{
    return {I18N::Game::TypePartyWithTheMouseCursorOn, I18N::Game::ThePlayerYouWouldLike,
            I18N::Game::ToCreateAPartyWith,            I18N::Game::AndYouCanCreate,
            I18N::Game::APartyWithThem,                I18N::Game::YouCanShareMoreExpWith,
            I18N::Game::YourPartyMembersBasedOnLevel};
}

// The hint's lines for the layout: all of them, or in the phone HUD those after the mouse lines.
template <typename Line> std::vector<Line> EmptyStateLines(bool phoneHud)
{
    const auto texts = EmptyStateTexts();
    std::span<const wchar_t* const> shown(texts);
    if (phoneHud)
        shown = shown.subspan(kMouseHintLines);
    std::vector<Line> lines;
    for (const wchar_t* text : shown)
        lines.push_back({StringUtils::WideToNarrow(text)});
    return lines;
}
} // namespace

CPartyInfoWindow::CPartyInfoWindow()
{
    m_pNewUIMng = NULL;
    m_bParty = false;
    m_iSelectedCharID = -1;
}

CPartyInfoWindow::~CPartyInfoWindow()
{
    Release();
}

bool CPartyInfoWindow::Create(CManager* pNewUIMng)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_PARTY, this);

    LoadImages();

    if (RmlUiRuntime::Instance().IsCreated())
    {
        BuildRmlUi();
    }

    Show(false);

    return true;
}

void CPartyInfoWindow::BindRmlModel(Rml::DataModelConstructor& c, PartyInfoRmlModel& model)
{
    c.Bind("text_px", &model.textPx);

    c.Bind("has_party", &model.hasParty);
    c.Bind("phone_hud", &model.phoneHud);
    c.Bind("social_label", &model.socialLabel);
    c.Bind("window_title", &model.windowTitle);
    c.Bind("exit_tooltip", &model.exitTooltip);

    auto textLine = c.RegisterStruct<TextLine>();
    textLine.RegisterMember("text", &TextLine::text);
    c.RegisterArray<std::vector<TextLine>>();
    c.Bind("empty_state_lines", &model.emptyStateLines);

    auto member = c.RegisterStruct<PartyMemberRow>();
    member.RegisterMember("name", &PartyMemberRow::name);
    member.RegisterMember("map_text", &PartyMemberRow::mapText);
    member.RegisterMember("coord_text", &PartyMemberRow::coordText);
    member.RegisterMember("hp_text", &PartyMemberRow::hpText);
    member.RegisterMember("hp_percent", &PartyMemberRow::hpPercent);
    member.RegisterMember("is_leader", &PartyMemberRow::isLeader);
    member.RegisterMember("show_kick", &PartyMemberRow::showKick);
    member.RegisterMember("index", &PartyMemberRow::index);
    c.RegisterArray<std::vector<PartyMemberRow>>();
    c.Bind("members", &model.members);

    UI::RmlBridge::BindWindowClose(c, mu::ui::window::INTERFACE_PARTY);
    c.BindEventCallback("party_kick_member",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
        {
            if (arguments.size() == 1)
                RmlClickKickMember(arguments[0].Get<int>(-1));
        });
    c.BindEventCallback("party_open_social",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { RmlClickOpenSocial(); });

    model.windowTitle = StringUtils::WideToNarrow(I18N::Game::Party);
    model.exitTooltip = StringUtils::WideToNarrow(I18N::Game::ClosePartyWindowP);
    model.socialLabel = StringUtils::WideToNarrow(I18N::Game::TouchSocialTitle);

    model.emptyStateLines = EmptyStateLines<TextLine>(model.phoneHud);
}

void CPartyInfoWindow::BuildRmlUi()
{
    m_RmlView.Ensure();
}

void CPartyInfoWindow::Release()
{
    UnloadImages();

    m_RmlView.Release();

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

void CPartyInfoWindow::OpenningProcess()
{
    // A click left from an earlier opening never opens the Social panel later.
    m_openSocialAsked = false;
    SocketClient->ToGameServer()->SendPartyListRequest();
}

void CPartyInfoWindow::ClosingProcess()
{
    m_openSocialAsked = false;
}

void CPartyInfoWindow::RmlClickOpenSocial()
{
    m_openSocialAsked = true;
}

void CPartyInfoWindow::OpenSocialIfAsked()
{
    if (!m_openSocialAsked)
        return;
    m_openSocialAsked = false;
    if (!UI::Mobile::ReplacesPcHud())
        return;
    g_pNewUISystem->Hide(mu::ui::window::INTERFACE_PARTY);
    UI::HUD::TouchControls::ShowSocialPanel();
}

void CPartyInfoWindow::RmlClickKickMember(int index)
{
    if (index < 0 || index >= PartyNumber)
        return;

    ConfirmLeaveParty(index);
}

void CPartyInfoWindow::ConfirmLeaveParty(int index)
{
    if (index < 0 || index >= PartyNumber)
        return;

    // Leaving is refused in Cursed Temple anyway (LeaveParty sends nothing there): no question.
    if (gMapManager.IsCursedTemple())
    {
        LeaveParty(index);
        return;
    }

    const std::wstring memberName = Party[index].Name;
    const bool leavingSelf = memberName == Hero->ID;

    wchar_t question[MAX_USERNAME_SIZE + 128] = {};
    if (leavingSelf)
        mu_swprintf_s(question, L"%ls", I18N::Game::PartyLeaveConfirm);
    else
        mu_swprintf_s(question, I18N::Game::PartyKickConfirm, memberName.c_str());

    mu::ui::window::GenericDialogConfig cfg;
    cfg.showCancel = true;
    cfg.lines.push_back({ question, true });
    cfg.onPrimary = [memberName]
    {
        // The window is looked up again: the answer may come after it is gone. The party may have changed
        // too, so the member is found by name and the kick allowed only as before (leader, or yourself).
        CSystem* system = CSystem::GetInstance();
        CPartyInfoWindow* window = system != nullptr ? system->GetUI_NewPartyInfoWindow() : nullptr;
        if (window == nullptr || Hero == nullptr)
            return;
        const int member = FindPartyMember(memberName);
        if (member < 0)
            return;
        const bool allowed = !wcscmp(Party[0].Name, Hero->ID) || !wcscmp(Party[member].Name, Hero->ID);
        if (allowed)
            window->LeaveParty(member);
    };
    mu::ui::window::g_pGenericConfirmDialog->Show(std::move(cfg));
}

int CPartyInfoWindow::FindPartyMember(const std::wstring& name)
{
    for (int i = 0; i < PartyNumber; ++i)
    {
        if (name == Party[i].Name)
            return i;
    }
    return -1;
}

bool CPartyInfoWindow::UpdateMouseEvent()
{
    // RmlUi handles both close targets; over the panel, the windows below get no mouse.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool CPartyInfoWindow::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_PARTY) == true)
    {
        if (mu::ui::window::IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_PARTY);
            PlayBuffer(SOUND_CLICK01);

            return false;
        }
    }

    return true;
}

bool CPartyInfoWindow::Update()
{
    SetParty(PartyNumber > 0);
    SyncRmlModel();
    OpenSocialIfAsked();
    return true;
}

bool CPartyInfoWindow::Render()
{
    // RmlUi's #panel owns all chrome/text/button rendering now; nothing left to draw natively.
    return true;
}

bool CPartyInfoWindow::LeaveParty(const int iIndex)
{
    if (!gMapManager.IsCursedTemple())
    {
        PlayBuffer(SOUND_CLICK01);
        SocketClient->ToGameServer()->SendPartyPlayerKickRequest(Party[iIndex].Number);
    }

    SetParty(false);

    return true;
}

void CPartyInfoWindow::Show(bool bShow)
{
    mu::ui::window::CObject::Show(bShow);
    if (m_RmlView.Document())
    {
        if (bShow) m_RmlView.Document()->Show();
        else m_RmlView.Document()->Hide();
    }
}

void CPartyInfoWindow::SetParty(bool bParty)
{
    m_bParty = bParty;
}

float CPartyInfoWindow::GetLayerDepth()
{
    return 2.4f;
}

void CPartyInfoWindow::SyncRmlModel()
{
    if (!m_RmlView.Document())
        return;

    auto& model = m_RmlView.GetModel();

    UI::RmlBridge::SyncNativeTextSize(m_RmlView.Binder());
    SyncPhoneHud();

    if (model.hasParty != m_bParty)
    {
        model.hasParty = m_bParty;
        m_RmlView.MarkDirty("has_party");
    }

    if (!m_bParty)
        return;

    wchar_t szText[256] = { 0, };
    std::vector<PartyMemberRow> members;
    members.reserve(PartyNumber);

    for (int i = 0; i < PartyNumber; i++)
    {
        PARTY_t* pMember = &Party[i];

        PartyMemberRow row;
        row.index = i;
        row.name = StringUtils::WideToNarrow(pMember->Name);
        row.mapText = StringUtils::WideToNarrow(gMapManager.GetMapName(pMember->Map));

        mu_swprintf(szText, L"(%d,%d)", pMember->x, pMember->y);
        row.coordText = StringUtils::WideToNarrow(szText);

        row.hpPercent = pMember->maxHP > 0 ? (pMember->currHP * 100.f) / pMember->maxHP : 0.f;
        mu_swprintf(szText, L"%d %ls %d", pMember->currHP, I18N::Game::Text2374, pMember->maxHP);
        row.hpText = StringUtils::WideToNarrow(szText);

        row.isLeader = (i == 0);
        row.showKick = !wcscmp(Party[0].Name, Hero->ID) || !wcscmp(Party[i].Name, Hero->ID);

        members.push_back(row);
    }

    model.members = std::move(members);
    m_RmlView.MarkDirty("members");
}

// The phone layout's hint without the mouse lines, and its Social button; read every frame, so a switch of the
// layout shows at once.
void CPartyInfoWindow::SyncPhoneHud()
{
    auto& model = m_RmlView.GetModel();
    const bool phoneHud = UI::Mobile::ReplacesPcHud();
    if (model.phoneHud == phoneHud)
        return;
    model.phoneHud = phoneHud;
    m_RmlView.MarkDirty("phone_hud");

    model.emptyStateLines = EmptyStateLines<TextLine>(phoneHud);
    m_RmlView.MarkDirty("empty_state_lines");
}

void CPartyInfoWindow::LoadImages()
{
    LoadBitmap(L"Interface\\newui_msgbox_back.jpg", IMAGE_PARTY_BASE_WINDOW_BACK, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back01.tga", IMAGE_PARTY_BASE_WINDOW_TOP, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back02-L.tga", IMAGE_PARTY_BASE_WINDOW_LEFT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back02-R.tga", IMAGE_PARTY_BASE_WINDOW_RIGHT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back03.tga", IMAGE_PARTY_BASE_WINDOW_BOTTOM, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_exit_00.tga", IMAGE_PARTY_BASE_WINDOW_BTN_EXIT, GL_LINEAR);		// Exit Button

    LoadBitmap(L"Interface\\newui_item_table01(L).tga", IMAGE_PARTY_TABLE_TOP_LEFT);
    LoadBitmap(L"Interface\\newui_item_table01(R).tga", IMAGE_PARTY_TABLE_TOP_RIGHT);
    LoadBitmap(L"Interface\\newui_item_table02(L).tga", IMAGE_PARTY_TABLE_BOTTOM_LEFT);
    LoadBitmap(L"Interface\\newui_item_table02(R).tga", IMAGE_PARTY_TABLE_BOTTOM_RIGHT);
    LoadBitmap(L"Interface\\newui_item_table03(Up).tga", IMAGE_PARTY_TABLE_TOP_PIXEL);
    LoadBitmap(L"Interface\\newui_item_table03(Dw).tga", IMAGE_PARTY_TABLE_BOTTOM_PIXEL);
    LoadBitmap(L"Interface\\newui_item_table03(L).tga", IMAGE_PARTY_TABLE_LEFT_PIXEL);
    LoadBitmap(L"Interface\\newui_item_table03(R).tga", IMAGE_PARTY_TABLE_RIGHT_PIXEL);

    LoadBitmap(L"Interface\\newui_party_lifebar01.jpg", IMAGE_PARTY_HPBAR_BACK, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_party_lifebar02.jpg", IMAGE_PARTY_HPBAR, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_party_flag.tga", IMAGE_PARTY_FLAG, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_party_x.tga", IMAGE_PARTY_EXIT, GL_LINEAR);
}

void CPartyInfoWindow::UnloadImages()
{
    DeleteBitmap(IMAGE_PARTY_BASE_WINDOW_BACK);
    DeleteBitmap(IMAGE_PARTY_BASE_WINDOW_TOP);
    DeleteBitmap(IMAGE_PARTY_BASE_WINDOW_LEFT);
    DeleteBitmap(IMAGE_PARTY_BASE_WINDOW_RIGHT);
    DeleteBitmap(IMAGE_PARTY_BASE_WINDOW_BOTTOM);
    DeleteBitmap(IMAGE_PARTY_BASE_WINDOW_BTN_EXIT);

    DeleteBitmap(IMAGE_PARTY_TABLE_RIGHT_PIXEL);
    DeleteBitmap(IMAGE_PARTY_TABLE_LEFT_PIXEL);
    DeleteBitmap(IMAGE_PARTY_TABLE_BOTTOM_PIXEL);
    DeleteBitmap(IMAGE_PARTY_TABLE_TOP_PIXEL);
    DeleteBitmap(IMAGE_PARTY_TABLE_BOTTOM_RIGHT);
    DeleteBitmap(IMAGE_PARTY_TABLE_BOTTOM_LEFT);
    DeleteBitmap(IMAGE_PARTY_TABLE_TOP_RIGHT);
    DeleteBitmap(IMAGE_PARTY_TABLE_TOP_LEFT);

    DeleteBitmap(IMAGE_PARTY_HPBAR_BACK);
    DeleteBitmap(IMAGE_PARTY_HPBAR);
    DeleteBitmap(IMAGE_PARTY_FLAG);
    DeleteBitmap(IMAGE_PARTY_EXIT);
}
