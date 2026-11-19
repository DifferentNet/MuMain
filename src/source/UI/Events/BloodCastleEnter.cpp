
#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/BloodCastleEnter.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Core/WindowGeometry.h"

#include "Character/CharacterManager.h"
#include "Audio/DSPlaySound.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "GameLogic/Events/EntryLevels.h"

#include <string>
#include <vector>

using namespace SEASON3B;
using namespace mu::ui::window;

CEnterBloodCastle::CEnterBloodCastle() : m_View("blood_castle_enter", "Data/Interface/RmlUi/blood_castle_enter.rml")
{
    m_pNewUIMng = NULL;

    m_iNumActiveBtn = 1;
}

CEnterBloodCastle::~CEnterBloodCastle()
{
    Release();
}

//---------------------------------------------------------------------------------------------
// Create
bool CEnterBloodCastle::Create(CManager* pNewUIMng)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_BLOODCASTLE, this);

    m_View.Build();

    Show(false);

    return true;
}

//---------------------------------------------------------------------------------------------
// Release
void CEnterBloodCastle::Release()
{
    m_View.Release();

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

//---------------------------------------------------------------------------------------------
// SetPos
//---------------------------------------------------------------------------------------------
// UpdateMouseEvent
bool CEnterBloodCastle::UpdateMouseEvent()
{
    return !m_View.IsPointerOver();
}

//---------------------------------------------------------------------------------------------
// UpdateKeyEvent
bool CEnterBloodCastle::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_BLOODCASTLE) == true)
    {
        if (mu::ui::window::IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_BLOODCASTLE);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }

    return true;
}

int CEnterBloodCastle::CheckLimitLV()
{
    // Below the first band the first button stays enabled, as before (the server refuses).
    const int grade = GameLogic::Events::EntryLevels::HeroGrade(GameLogic::Events::EntryLevels::Event::BloodCastle);
    return grade < 0 ? 0 : grade;
}

bool CEnterBloodCastle::Update()
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
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_BLOODCASTLE);
        return true;
    }
    if (pressed >= 0 && pressed == m_iNumActiveBtn)
    {
        SocketClient->ToGameServer()->SendBloodCastleEnterRequest(m_iNumActiveBtn + 1, 0xFF);
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_BLOODCASTLE);
    }

    return true;
}

bool CEnterBloodCastle::Render()
{
    // Nothing native left: the frame, the texts and the buttons are RmlUi. Kept because CObject
    // requires the override.
    return true;
}

float CEnterBloodCastle::GetLayerDepth()
{
    return 4.1f;
}

void CEnterBloodCastle::OpenningProcess()
{
    SocketClient->ToGameServer()->SendCloseNpcRequest();

    m_iNumActiveBtn = CheckLimitLV();

    // Every button locked (grey, never hovered) but the hero's band.
    std::vector<EventEntryView::Button> buttons(MAX_ENTER_GRADE);
    for (int i = 0; i < MAX_ENTER_GRADE; i++)
        buttons[i].label = GameLogic::Events::EntryLevels::BandLabel(GameLogic::Events::EntryLevels::Event::BloodCastle, i);
    buttons[m_iNumActiveBtn].enabled = true;

    SetViewContent(buttons);
}

void CEnterBloodCastle::ClosingProcess()
{
    SocketClient->ToGameServer()->SendCloseNpcRequest();
}

void CEnterBloodCastle::SetViewContent(const std::vector<EventEntryView::Button>& buttons)
{
    // The ticket of the hero's castle first: castle N takes the Cloak of Invisibility +N.
    const std::vector<EventEntryView::Line> lines = {
        EventEntryView::TicketLine(ITEM_INVISIBILITY_CLOAK, m_iNumActiveBtn + 1),
        {I18N::Game::YourWillToHelpTheArchangel},
    };
    m_View.SetContent(I18N::Game::MessengerOfArchangel, lines, buttons);
}
