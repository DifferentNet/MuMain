
#include "stdafx.h"
#include "UI/HUD/MuHelperBar.h"
#include "I18N/All.h"

#include "Audio/DSPlaySound.h"
#include "UI/Core/WindowSystem.h"
#include "World/MapInfra/MapManager.h"
#include "MUHelper/MuHelper.h"
#include "GameLogic/Character/PingHud.h"

#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlSyncField.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlWorkspaceParticipant.h"
#include "UI/Placement/WindowPlacement.h"
#include "Core/Utilities/StringUtils.h"
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

using namespace SEASON3B;
using namespace mu::ui::window;

CMuHelperBar::CMuHelperBar()
{
    m_pNewUIMng = NULL;
    m_CurHeroPosition.x = m_CurHeroPosition.y = 0;
}

CMuHelperBar::~CMuHelperBar()
{
    Release();
}

//---------------------------------------------------------------------------------------------
// Create
bool CMuHelperBar::Create(CManager* pNewUIMng)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_MU_HELPER_BAR, this);
    UI::RmlBridge::RegisterWorkspaceDocument("mu_helper_bar", [this] { return m_RmlView.Document(); }, "panel");

    // Guarded so the doc/model are created once, even though Create() re-runs on resolution change.
    if (!m_RmlView.Document() && RmlUiRuntime::Instance().IsCreated())
    {
        BuildRmlUi();
        
    }
        // Not Show()n here -- Create() runs before SceneFlag reaches MAIN_SCENE; SyncDocVisibility()
        // (called every frame) shows it once the scene gate allows it.

    Show(true);

    return true;
}

void CMuHelperBar::BindRmlModel(Rml::DataModelConstructor& c, MuHelperBarRmlModel& model)
{
    c.Bind("position_text", &model.positionText);
    c.Bind("mu_helper_active", &model.muHelperActive);
    c.Bind("config_hint", &model.configHint);
    c.Bind("start_hint", &model.startHint);
    c.Bind("stop_hint", &model.stopHint);
    c.Bind("ping_text", &model.pingText);
    c.Bind("ping_level", &model.pingLevel);
    c.Bind("ping_hint", &model.pingHint);

    c.BindEventCallback("mu_helper_config_click",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { RmlClickConfig(); });
    c.BindEventCallback("mu_helper_toggle_click",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { RmlClickToggle(); });
}

void CMuHelperBar::OnRmlReloaded()
{
    UI::Placement::Invalidate();
}

void CMuHelperBar::BuildRmlUi()
{
    m_RmlView.Ensure();
}

void CMuHelperBar::Release()
{
    UI::Placement::UnregisterParticipant("mu_helper_bar");
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }

    // Hide the doc directly since RmlUi renders last in the frame regardless of scene (see CLoginWin::PreRelease()).
    if (m_RmlView.Document())
        m_RmlView.Document()->Hide();

    m_RmlView.Release();
}

bool CMuHelperBar::UpdateMouseEvent()
{
    // RmlUi's own context does hit-testing now; never consumes the legacy mouse event.
    return true;
}

bool CMuHelperBar::UpdateKeyEvent()
{
    return true;
}

bool CMuHelperBar::Update()
{
    if (m_bRmlConfigClicked)
    {
        m_bRmlConfigClicked = false;
        g_pNewUISystem->Toggle(mu::ui::window::INTERFACE_MUHELPER);
        PlayBuffer(SOUND_CLICK01);
    }
    else if (m_bRmlToggleClicked)
    {
        m_bRmlToggleClicked = false;
        MUHelper::g_MuHelper.Toggle();
        PlayBuffer(SOUND_CLICK01);
    }

    if ((IsVisible() == true) && (Hero != NULL))
    {
        m_CurHeroPosition.x = (Hero->PositionX);
        m_CurHeroPosition.y = (Hero->PositionY);
    }

    SyncRmlModel();

    return true;
}

bool CMuHelperBar::Render()
{
    // RmlUi's #panel owns all visuals now; SyncRmlModel() (from Update()) keeps it current.
    return true;
}

void CMuHelperBar::SyncRmlModel()
{
    if (!m_RmlView.Document()) return;

    auto& model = m_RmlView.GetModel();

    wchar_t szText[255] = {};
    mu_swprintf(szText, L"%ls (%d , %d)", gMapManager.GetMapName(gMapManager.WorldActive), m_CurHeroPosition.x, m_CurHeroPosition.y);
    const std::string positionUtf8 = StringUtils::WideToNarrow(szText);
    if (model.positionText != positionUtf8)
    {
        model.positionText = positionUtf8;
        m_RmlView.MarkDirty("position_text");
    }

    const bool active = MUHelper::g_MuHelper.IsActive();
    if (model.muHelperActive != active)
    {
        model.muHelperActive = active;
        m_RmlView.MarkDirty("mu_helper_active");
    }

    SyncField(m_RmlView.Binder(), &MuHelperBarRmlModel::configHint, "config_hint",
              StringUtils::WideToNarrow(I18N::Game::OfficialMUHelperSetting));
    SyncField(m_RmlView.Binder(), &MuHelperBarRmlModel::startHint, "start_hint",
              StringUtils::WideToNarrow(I18N::Game::StartOfficialMUHelper));
    SyncField(m_RmlView.Binder(), &MuHelperBarRmlModel::stopHint, "stop_hint",
              StringUtils::WideToNarrow(I18N::Game::StopOfficialMUHelper));

    // The ping label: the server's last round trip, green below 100 ms, yellow below 200, red above, grey while
    // the server waits for a late answer. Hidden on a server without the add-on (no line ever came).
    const auto& ping = GameLogic::PingHud::Ping();
    std::string pingText;
    std::string pingLevel;
    std::string pingHint;
    if (ping.HasReading())
    {
        const auto& reading = ping.Current();
        wchar_t label[64] = {};
        mu_swprintf(label, I18N::Game::PingHudLabel, reading.lastMs);
        pingText = StringUtils::WideToNarrow(label);
        pingLevel = ping.IsStale() ? "stale" : reading.lastMs < 100 ? "good" : reading.lastMs < 200 ? "fair" : "bad";

        wchar_t hint[256] = {};
        if (reading.networkMs >= 0)
            mu_swprintf(hint, I18N::Game::PingHudHintNetwork, reading.lastMs, reading.medianMs, reading.networkMs);
        else
            mu_swprintf(hint, I18N::Game::PingHudHint, reading.lastMs, reading.medianMs);
        pingHint = StringUtils::WideToNarrow(hint);
    }
    SyncField(m_RmlView.Binder(), &MuHelperBarRmlModel::pingText, "ping_text", pingText);
    SyncField(m_RmlView.Binder(), &MuHelperBarRmlModel::pingLevel, "ping_level", pingLevel);
    SyncField(m_RmlView.Binder(), &MuHelperBarRmlModel::pingHint, "ping_hint", pingHint);
}

float CMuHelperBar::GetLayerDepth()
{
    return 4.3f;
}

void CMuHelperBar::SyncDocVisibility(bool sceneAllowsShow)
{
    UI::RmlBridge::SyncDocumentVisibility(m_RmlView.Document(), IsVisible() && sceneAllowsShow);
}

void CMuHelperBar::OpenningProcess()
{
}

void CMuHelperBar::ClosingProcess()
{
}
