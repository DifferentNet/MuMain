
#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Party/PartyInfoWindow.h"
#include "UI/Party/PartyListRefresh.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Core/WindowGeometry.h"
#include "UI/HUD/HeroGauges.h"
#include "UI/HUD/KeyHint.h"
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
#include "UI/HUD/ChatInputBox.h"
#include "UI/HUD/CommandWindow.h"
#include "Engine/Object/ZzzInventory.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include <string>

using namespace SEASON3B;
using namespace mu::ui::window;

namespace
{
// The hint with no party: the ways to invite that work now, then why. The phone view has no command window key:
// its button opens the players nearby instead, so that line is left out there.
std::vector<std::wstring> EmptyStateTexts(bool phoneHud)
{
    std::vector<std::wstring> texts;
    texts.emplace_back(I18N::Game::PartyHintInviteByName);
    if (!phoneHud)
    {
        wchar_t line[256] = {};
        const std::wstring key = UI::KeyHint::KeyLabel(Core::Input::KeyBindings::Action::Command);
        mu_swprintf(line, I18N::Game::PartyHintInviteFromCommandWindow, key.c_str(), I18N::Game::Party);
        texts.emplace_back(line);
    }
    texts.emplace_back(I18N::Game::PartyHintSharedExperience);
    return texts;
}

// The hint's lines for the layout; the last one (why) stands a little apart.
template <typename Line> std::vector<Line> EmptyStateLines(bool phoneHud)
{
    std::vector<Line> lines;
    for (const std::wstring& text : EmptyStateTexts(phoneHud))
        lines.push_back({StringUtils::WideToNarrow(text.c_str())});
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
    c.Bind("in_chaos_castle", &model.inChaosCastle);
    c.Bind("social_label", &model.socialLabel);
    c.Bind("window_title", &model.windowTitle);
    c.Bind("exit_tooltip", &model.exitTooltip);

    auto textLine = c.RegisterStruct<TextLine>();
    textLine.RegisterMember("text", &TextLine::text);
    c.RegisterArray<std::vector<TextLine>>();
    c.Bind("empty_state_lines", &model.emptyStateLines);
    c.Bind("empty_paragraph_at", &model.emptyParagraphAt);

    auto member = c.RegisterStruct<PartyMemberRow>();
    member.RegisterMember("name", &PartyMemberRow::name);
    member.RegisterMember("map_text", &PartyMemberRow::mapText);
    member.RegisterMember("coord_text", &PartyMemberRow::coordText);
    member.RegisterMember("hp_text", &PartyMemberRow::hpText);
    member.RegisterMember("hp_fraction", &PartyMemberRow::hpFraction);
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
    c.BindEventCallback("party_whisper_member",
        [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments)
        {
            if (arguments.size() == 1)
                WhisperMember(arguments[0].Get<int>(-1), event, false);
        });
    c.BindEventCallback("party_tap_member",
        [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments)
        {
            if (arguments.size() == 1)
                WhisperMember(arguments[0].Get<int>(-1), event, true);
        });
    c.BindEventCallback("party_open_social",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { RmlClickOpenSocial(); });

    model.windowTitle = StringUtils::WideToNarrow(I18N::Game::Party);
    model.exitTooltip = StringUtils::WideToNarrow(
        UI::KeyHint::WithKey(I18N::Game::ClosePartyWindowP, Core::Input::KeyBindings::Action::Party).c_str());
    model.socialLabel = StringUtils::WideToNarrow(I18N::Game::TouchSocialTitle);

    FillEmptyState(model);
}

void CPartyInfoWindow::FillEmptyState(PartyInfoRmlModel& model)
{
    model.emptyStateLines = EmptyStateLines<TextLine>(model.phoneHud);
    model.emptyParagraphAt = static_cast<int>(model.emptyStateLines.size()) - 1;
}

void CPartyInfoWindow::RebuildEmptyState()
{
    FillEmptyState(m_RmlView.GetModel());
    m_RmlView.MarkDirty("empty_state_lines");
    m_RmlView.MarkDirty("empty_paragraph_at");
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
    // The command window's key may have been rebound since.
    RebuildEmptyState();
    SocketClient->ToGameServer()->SendPartyListRequest();
    UI::Party::ListRefresh::NoteAsked();
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
    if (UI::Mobile::ReplacesPcHud())
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_PARTY);
        UI::HUD::TouchControls::ShowSocialPanel();
        return;
    }
    OpenCommandWindowForParty();
}

void CPartyInfoWindow::OpenCommandWindowForParty()
{
    // The command window's own rule (the D key, UI/HUD/HotKey.cpp): nothing in Chaos Castle (the button
    // hides there, SyncRmlModel()), one line where players fight.
    if (gMapManager.InChaosCastle())
        return;
    if (::IsStrifeMap(gMapManager.WorldActive))
    {
        if (g_pSystemLogBox->CheckChatRedundancy(I18N::Game::TheCommandWindowCannotBeActivatedInBattleZone) == FALSE)
            g_pSystemLogBox->AddText(I18N::Game::TheCommandWindowCannotBeActivatedInBattleZone,
                                     mu::ui::window::TYPE_SYSTEM_MESSAGE);
        return;
    }
    g_pNewUISystem->Hide(mu::ui::window::INTERFACE_PARTY);
    g_pNewUISystem->Show(mu::ui::window::INTERFACE_COMMAND);
    if (g_pCommandWindow != nullptr)
        g_pCommandWindow->SelectCommand(COMMAND_PARTY);
    PlayBuffer(SOUND_CLICK01);
}

void CPartyInfoWindow::WhisperMember(int index, const Rml::Event& event, bool tap)
{
    // Touch taps, the mouse double-clicks: a tap pair would otherwise also send a double click.
    if (tap != UI::Mobile::IsTouchControls())
        return;
    if (index < 0 || index >= PartyNumber || g_pChatInputBox == nullptr || Hero == nullptr)
        return;
    const Rml::Element* target = event.GetTargetElement();
    if (target != nullptr && target->IsClassSet("party-kick-btn"))
        return;

    const wchar_t* name = Party[index].Name;
    if (name[0] == L'\0' || wcscmp(name, Hero->ID) == 0)
        return;
    // Open the chat input first, otherwise only the whisper target changes and nothing shows.
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_CHATINPUTBOX))
        g_pNewUISystem->Show(mu::ui::window::INTERFACE_CHATINPUTBOX);
    g_pChatInputBox->StartWhisperTo(name);
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
    // The members' maps and places come with the party list, asked again while the window shows.
    UI::Party::ListRefresh::Tick(IsVisible() && PartyNumber > 0);
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

    if (const bool inChaosCastle = gMapManager.InChaosCastle(); model.inChaosCastle != inChaosCastle)
    {
        model.inChaosCastle = inChaosCastle;
        m_RmlView.MarkDirty("in_chaos_castle");
    }

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

        const double health = pMember->maxHP > 0 ? static_cast<double>(pMember->currHP) / pMember->maxHP : 0.0;
        row.hpFraction = static_cast<float>(UI::HUD::HeroGauges::UnitFraction(health));
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

    RebuildEmptyState();
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
