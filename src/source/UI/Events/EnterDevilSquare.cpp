
#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/EnterDevilSquare.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Core/WindowGeometry.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Character/CharacterManager.h"
#include "Audio/DSPlaySound.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "GameLogic/Events/EntryLevels.h"

#include <string>
#include <vector>

using namespace SEASON3B;
using namespace mu::ui::window;

CEnterDevilSquare::CEnterDevilSquare() : m_View("devil_square_enter", "Data/Interface/RmlUi/devil_square_enter.rml")
{
    m_pNewUIMng = NULL;

    m_iNumActiveBtn = 1;
}

CEnterDevilSquare::~CEnterDevilSquare()
{
    Release();
}

bool CEnterDevilSquare::Create(CManager* pNewUIMng)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_DEVILSQUARE, this);

    m_View.Build();

    Show(false);

    return true;
}

void CEnterDevilSquare::Release()
{
    m_View.Release();

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

bool CEnterDevilSquare::UpdateMouseEvent()
{
    return !m_View.IsPointerOver();
}

bool CEnterDevilSquare::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_DEVILSQUARE) == true)
    {
        if (mu::ui::window::IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_DEVILSQUARE);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }

    return true;
}

bool CEnterDevilSquare::Update()
{
    m_View.Sync(IsVisible());

    // Clicks RmlUi reported: the exit button hides the window, the enabled level button asks the
    // server to enter that level band (the original's BtnProcess()).
    const bool exitPressed = m_View.TakeExitPressed();
    const int pressed = m_View.TakePressedButton();
    if (!IsVisible())
        return true;
    if (exitPressed)
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_DEVILSQUARE);
        return true;
    }
    if (pressed >= 0 && pressed == m_iNumActiveBtn)
    {
        SocketClient->ToGameServer()->SendDevilSquareEnterRequest(m_iNumActiveBtn, 0xFF);
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_DEVILSQUARE);
    }

    return true;
}

bool CEnterDevilSquare::Render()
{
    // Nothing native left: the frame, the texts and the buttons are RmlUi. Kept because CObject
    // requires the override.
    return true;
}

float CEnterDevilSquare::GetLayerDepth()
{
    return 4.0f;
}

int CEnterDevilSquare::CheckLimitLV()
{
    // Below the first band the first button stays enabled, as before (the server refuses).
    const int grade = GameLogic::Events::EntryLevels::HeroGrade(GameLogic::Events::EntryLevels::Event::DevilSquare);
    return grade < 0 ? 0 : grade;
}

void CEnterDevilSquare::OpenningProcess()
{
    SocketClient->ToGameServer()->SendCloseNpcRequest();

    m_iNumActiveBtn = CheckLimitLV();

    // Every button locked (grey, never hovered) but the hero's band.
    std::vector<EventEntryView::Button> buttons(MAX_ENTER_GRADE);
    for (int i = 0; i < MAX_ENTER_GRADE; i++)
        buttons[i].label = GameLogic::Events::EntryLevels::BandLabel(GameLogic::Events::EntryLevels::Event::DevilSquare, i);
    buttons[m_iNumActiveBtn].enabled = true;

    SetViewContent(buttons);
}

void CEnterDevilSquare::ClosingProcess()
{
    SocketClient->ToGameServer()->SendCloseNpcRequest();
}


void CEnterDevilSquare::SetViewContent(const std::vector<EventEntryView::Button>& buttons)
{
    // Keep all six translated fragments in their original order.
    // Before them, the ticket of the hero's square: square N takes the Devil's Invitation +N.
    const std::vector<EventEntryView::Line> lines = {
        EventEntryView::TicketLine(ITEM_DEVILS_INVITATION, m_iNumActiveBtn + 1),
        {I18N::Game::YouVeBeenGivenAChanceToProveYourBravery},
        {I18N::Game::NoOneHasEverEnteredTheDevilSquareYet},
        {I18N::Game::NoHumanHasEverGoneThere},
        {I18N::Game::DoNotBelieveAnythingYouSeeInThere},
        {I18N::Game::OnlyTrustYourBraveryAndStrength},
        {I18N::Game::OnlyYourBraveryAndStrengthWillKeepYouAlive},
    };

    m_View.SetContent(I18N::Game::DevilSquare, lines, buttons);
}
