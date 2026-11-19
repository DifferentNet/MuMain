
#include "stdafx.h"
#include "UI/HUD/MoveCommandWindow.h"
#include "UI/Core/WindowSystem.h"
#include "GameLogic/Items/ChangeRingManager.h"
#include "GameLogic/Items/ItemCategories.h"
#include "Core/Utilities/KeyGenerator.h"
#include "Network/Server/ServerListManager.h"
#include "Engine/Object/ZzzOpenData.h"
#include "World/MapInfra/MapManager.h"
#include "Character/CharacterManager.h"
#include "Audio/DSPlaySound.h"
#include "I18N/All.h"

#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "Core/Utilities/StringUtils.h"
#include "UI/Text/GroupedNumberText.h"
#include "UI/Chat/WarpUnlocks.h"
#include "UI/HUD/WarpListMarks.h"
#include "Engine/Object/ZzzInfomation.h"
#include "GameLogic/Combat/DuelMgr.h"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <cmath>

using namespace SEASON3B;
using namespace mu::ui::window;

namespace
{
    constexpr int MapNameCount = 6;
    // The fill slot's smallest panel: the header, the close bar and three rows.
    constexpr float kFillMinimumHeight = 102.f;

    const std::wstring MapName[MapNameCount] =
    {
        L"Lorencia",
        L"Noria",
        L"Elbeland",
        L"Dungeon",
        L"Devias",
        L"LostTower",
    };

    bool IsLuckySeal(const std::wstring& name)
    {
        if (name.size() != 0) {
            for (int i = 0; i < MapNameCount; ++i) {
                if (name == MapName[i])
                {
                    return true;
                }
            }
        }
        return false;
    }

    // Where a warp list row lands: the world and the middle of its gate's area (Data\Gate.bmd). A gate the client
    // does not know gives no spot, so its row is never marked.
    UI::MoveCommand::Marks::RowSpot RowSpotOf(const SEASON3B::CMoveCommandData::MOVEINFODATA& moveInfo)
    {
        UI::MoveCommand::Marks::RowSpot spot;
        const int gate = moveInfo._ReqInfo.iGateNum;
        if (GateAttribute == nullptr || gate < 0 || gate >= MAX_GATES)
            return spot;
        const GATE_ATTRIBUTE& attribute = GateAttribute[gate];
        const bool knownGate = attribute.x2 > 0 && attribute.y2 > 0
            && attribute.x2 >= attribute.x1 && attribute.y2 >= attribute.y1;
        if (!knownGate)
            return spot;
        spot.world = attribute.Map;
        spot.x = (attribute.x1 + attribute.x2) / 2;
        spot.y = (attribute.y1 + attribute.y2) / 2;
        return spot;
    }
};

CMoveCommandWindow::CMoveCommandWindow()
{
    m_pNewUIMng = NULL;
    m_dwMoveCommandKey = 0;

}

CMoveCommandWindow::~CMoveCommandWindow()
{
    Release();
}

bool mu::ui::window::CMoveCommandWindow::Create(CManager* pNewUIMng)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_MOVEMAP, this);

    RefreshData();

    BuildRmlUi();

    Show(false);

    return true;
}

void mu::ui::window::CMoveCommandWindow::Release()
{

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }

    m_RmlView.Release();
}

void mu::ui::window::CMoveCommandWindow::RefreshData()
{
    m_listMoveInfoData = CMoveCommandData::GetInstance()->GetMoveCommandDatalist();
    m_RowSpots.clear();
    m_RowSpots.reserve(m_listMoveInfoData.size());
    for (const auto* moveInfo : m_listMoveInfoData)
        m_RowSpots.push_back(RowSpotOf(*moveInfo));
}

void mu::ui::window::CMoveCommandWindow::SetFillPlacementSize(float width, float height)
{
    if (m_FillWidth == width && m_FillHeight == height)
        return;
    m_FillWidth = width;
    m_FillHeight = height;
    ApplyFillSize();
}

bool mu::ui::window::CMoveCommandWindow::GetFillMinimumSize(float& width, float& height) const
{
    // Its content width, and the chrome with three rows: the list takes the height it is given.
    width = static_cast<float>(UI::MoveCommand::kWindowWidth);
    if (Rml::Element* panel = m_RmlView.Document() != nullptr ? m_RmlView.Document()->GetElementById("panel") : nullptr)
    {
        if (panel->GetBox().GetSize(Rml::BoxArea::Border).x > 0.f)
            width = panel->GetBox().GetSize(Rml::BoxArea::Border).x;
    }
    height = kFillMinimumHeight;
    return true;
}

void mu::ui::window::CMoveCommandWindow::ApplyFillSize()
{
    Rml::Element* panel = m_RmlView.Document() != nullptr ? m_RmlView.Document()->GetElementById("panel") : nullptr;
    if (panel == nullptr)
        return;
    const bool fill = m_FillWidth > 0.f && m_FillHeight > 0.f;
    panel->SetClass("fill-placement", fill);
    if (fill)
    {
        panel->SetProperty(Rml::PropertyId::Width, Rml::Property(m_FillWidth, Rml::Unit::PX));
        panel->SetProperty(Rml::PropertyId::Height, Rml::Property(m_FillHeight, Rml::Unit::PX));
    }
    else
    {
        panel->RemoveProperty(Rml::PropertyId::Width);
        panel->RemoveProperty(Rml::PropertyId::Height);
    }
    m_RmlView.Document()->UpdateDocument();
}

bool mu::ui::window::CMoveCommandWindow::IsLuckySealBuff()
{
    if (g_isCharacterBuff((&Hero->Object), eBuff_Seal1)
        || g_isCharacterBuff((&Hero->Object), eBuff_Seal2)
        || g_isCharacterBuff((&Hero->Object), eBuff_Seal3)
        || g_isCharacterBuff((&Hero->Object), eBuff_Seal4)
        || g_isCharacterBuff((&Hero->Object), eBuff_Seal_HpRecovery)
        || g_isCharacterBuff((&Hero->Object), eBuff_Seal_MpRecovery)
        || g_isCharacterBuff((&Hero->Object), eBuff_AscensionSealMaster)
        || g_isCharacterBuff((&Hero->Object), eBuff_WealthSealMaster)
        || g_isCharacterBuff((&Hero->Object), eBuff_NewWealthSeal)
        || g_isCharacterBuff((&Hero->Object), eBuff_PartyExpBonus)
        )
    {
        return true;
    }
    return false;
}

bool mu::ui::window::CMoveCommandWindow::IsMapMove(const std::wstring& src)
{
    if (Hero->Object.Kind == KIND_PLAYER
        && Hero->Object.Type == MODEL_PLAYER
        && Hero->Object.SubType == MODEL_GM_CHARACTER)
    {
        return true;
    }

    if (g_isCharacterBuff((&Hero->Object), eBuff_GMEffect))
    {
        return true;
    }

    if (IsLuckySealBuff() == false) {
        wchar_t lpszStr1[1024]; wchar_t* lpszStr2 = NULL;
        if (src.find(I18N::Game::Warp) != std::wstring::npos) {
            std::wstring temp = I18N::Game::Warp;
            temp += ' ';
            mu_swprintf(lpszStr1, src.c_str());
            wchar_t* context = nullptr;
            lpszStr2 = wcstok_s(lpszStr1, temp.c_str(), &context);
            if (lpszStr2 == NULL) return false;

            SettingCanMoveMap();
            for (const auto* moveInfo : m_listMoveInfoData) {
                if (!wcscmp(lpszStr2, moveInfo->_ReqInfo.szMainMapName)) {
                    if (moveInfo->_bCanMove == true) {
                        return IsLuckySeal(moveInfo->_ReqInfo.szSubMapName);
                    }
                }
            }
            return false;
        }
        else if (src.find(L"/move") != std::wstring::npos) {
            std::wstring temp = L"/move";
            temp += ' ';
            mu_swprintf(lpszStr1, src.c_str());
            wchar_t* context = nullptr;
            lpszStr2 = wcstok_s(lpszStr1, temp.c_str(), &context);
            if (lpszStr2 == NULL) return false;

            SettingCanMoveMap();
            for (const auto* moveInfo : m_listMoveInfoData) {
                if (!wcsicmp(lpszStr2, moveInfo->_ReqInfo.szMainMapName)) {
                    if (moveInfo->_bCanMove == true) {
                        return IsLuckySeal(moveInfo->_ReqInfo.szSubMapName);
                    }
                }
            }
            return false;
        }
        else {
            return IsLuckySeal(src);
        }
    }
    return true;
}

void mu::ui::window::CMoveCommandWindow::SetMoveCommandKey(DWORD dwKey)
{
    m_dwMoveCommandKey = dwKey;
}

DWORD mu::ui::window::CMoveCommandWindow::GetMoveCommandKey()
{
    m_dwMoveCommandKey = g_KeyGenerator.GenerateKeyValue(m_dwMoveCommandKey);

    return m_dwMoveCommandKey;
}

void mu::ui::window::CMoveCommandWindow::SetStrifeMap()
{
    std::list<CMoveCommandData::MOVEINFODATA*>::iterator li;

    if (!g_ServerListManager->IsNonPvP())
    {
        int anStrifeIndex[1] = { 42 };
        int i;
        for (li = m_listMoveInfoData.begin(); li != m_listMoveInfoData.end(); advance(li, 1))
        {
            (*li)->_bStrife = false;
            for (i = 0; i < 1; ++i)
            {
                if ((*li)->_ReqInfo.index == anStrifeIndex[i])
                {
                    (*li)->_bStrife = true;
                    break;
                }
            }
        }
    }
    else
    {
        for (li = m_listMoveInfoData.begin(); li != m_listMoveInfoData.end(); advance(li, 1))
            (*li)->_bStrife = false;
    }
}

namespace
{
// Why the hero can't take a warp list entry, in the order the checks run; None when it can.
enum class MoveBlock
{
    None,
    Level,
    Zen,
    Killer,
    IcarusFlight,
    AtlansMount,
    ServerRule,
    Gens,
};

// The level-400 maps ask the same of every class.
// The level a warp list entry asks of the hero: the Dark classes need two thirds of the listed level.
int RequiredMoveLevel(int listedLevel)
{
    const auto baseClass = gCharacterManager.GetBaseClass(CharacterAttribute->Class);
    const bool reducedClass = baseClass == CLASS_DARK || baseClass == CLASS_DARK_LORD || baseClass == CLASS_RAGEFIGHTER;
    return UI::Chat::WarpUnlocks::RequiredLevel(listedLevel, reducedClass);
}

// The map's own rule: wings for Icarus, no Uniria or Dinorant in Atlans, no Vulcanus on a non-PvP server.
MoveBlock EvaluateMapRule(const wchar_t* mapName)
{
    const ITEM* rightRing = &CharacterMachine->Equipment[EQUIPMENT_RING_RIGHT];
    const ITEM* leftRing = &CharacterMachine->Equipment[EQUIPMENT_RING_LEFT];
    ITEM* helper = &CharacterMachine->Equipment[EQUIPMENT_HELPER];
    ITEM* wing = &CharacterMachine->Equipment[EQUIPMENT_WING];

    if (SEASON3B::CMoveCommandData::IsMapNamed(mapName, I18N::Game::Icarus, L"Icarus"))
    {
        const bool canFly = GameLogic::Items::HasFlightEquipment(helper, wing)
            && helper->Type != ITEM_HORN_OF_UNIRIA
            && !g_ChangeRingMgr->CheckBanMoveIcarusMap(rightRing->Type, leftRing->Type);
        return canFly ? MoveBlock::None : MoveBlock::IcarusFlight;
    }
    if (SEASON3B::CMoveCommandData::IsMapNamed(mapName, I18N::Game::Atlans, L"Atlans", true))
    {
        const bool onLandMount = helper->Type == ITEM_HORN_OF_UNIRIA || helper->Type == ITEM_HORN_OF_DINORANT;
        return onLandMount ? MoveBlock::AtlansMount : MoveBlock::None;
    }
    if (g_ServerListManager->IsNonPvP() && SEASON3B::CMoveCommandData::IsMapNamed(mapName, I18N::Game::Vulcanus, L"Vulcanus"))
        return MoveBlock::ServerRule;
    return MoveBlock::None;
}

MoveBlock EvaluateMove(const SEASON3B::CMoveCommandData::MOVEINFODATA& moveInfo)
{
    if (CharacterAttribute->Level < RequiredMoveLevel(moveInfo._ReqInfo.iReqLevel))
        return MoveBlock::Level;
    if (static_cast<int>(CharacterMachine->Gold) < moveInfo._ReqInfo.iReqZen)
        return MoveBlock::Zen;
    if (static_cast<int>(Hero->PK) >= PVP_MURDERER1)
        return MoveBlock::Killer;

    const MoveBlock mapRule = EvaluateMapRule(moveInfo._ReqInfo.szMainMapName);
    if (mapRule != MoveBlock::None)
        return mapRule;
    if (moveInfo._bStrife && Hero->m_byGensInfluence == 0)
        return MoveBlock::Gens;
    return MoveBlock::None;
}

// One line saying why a red row doesn't warp; the window stays open.
void PostMoveBlock(const SEASON3B::CMoveCommandData::MOVEINFODATA& moveInfo, MoveBlock block)
{
    wchar_t text[128] = {};
    switch (block)
    {
    case MoveBlock::Level:
        mu_swprintf(text, I18N::Game::MoveNeedsLevel, RequiredMoveLevel(moveInfo._ReqInfo.iReqLevel));
        break;
    case MoveBlock::Zen:
        mu_swprintf(text, L"%ls", I18N::Game::YouAreShortOfZen);
        break;
    case MoveBlock::Killer:
        mu_swprintf(text, L"%ls", I18N::Game::MoveBlockedKiller);
        break;
    case MoveBlock::IcarusFlight:
        mu_swprintf(text, L"%ls", I18N::Game::YouCanEnterIcarusOnlyWithWingsDinorantFenrirr);
        break;
    case MoveBlock::AtlansMount:
        mu_swprintf(text, L"%ls", I18N::Game::MoveBlockedAtlansMount);
        break;
    case MoveBlock::ServerRule:
        mu_swprintf(text, L"%ls", I18N::Game::MoveBlockedHere);
        break;
    case MoveBlock::Gens:
        mu_swprintf(text, L"%ls", I18N::Game::MoveNeedsGens);
        break;
    case MoveBlock::None:
        return;
    }
    g_pSystemLogBox->AddText(text, mu::ui::window::TYPE_ERROR_MESSAGE);
}

// How many frames an opening waits for the rows before it gives up bringing the "back" row into view.
constexpr int kScrollToBackFrames = 10;

// Deaths that send the hero nowhere he would want to go back to: the event maps, the Endless Tower (the
// Doppelganger maps) and duels.
bool DiedInEvent()
{
    const int world = gMapManager.WorldActive;
    const bool towerMap = world >= WD_65DOPPLEGANGER1 && world <= WD_68DOPPLEGANGER4;
    return g_DuelMgr.IsDuelEnabled() || world == WD_64DUELARENA || towerMap || gMapManager.InBloodCastle()
        || gMapManager.InDevilSquare() || gMapManager.InChaosCastle() || gMapManager.InHellas()
        || gMapManager.InHiddenHellas();
}
} // namespace

void UI::MoveCommand::NoteHeroDeath()
{
    if (Hero == nullptr || DiedInEvent())
        return;
    Marks::HeroDeathSpot().NoteDeath(gMapManager.WorldActive, Hero->PositionX, Hero->PositionY, GetTickCount());
}

void UI::MoveCommand::NoteHeroRevival()
{
    Marks::HeroDeathSpot().NoteRevival();
}

void UI::MoveCommand::NoteHeroWarp()
{
    Marks::HeroDeathSpot().NoteWarp();
}

void UI::MoveCommand::ForgetHeroDeath()
{
    Marks::HeroDeathSpot().Clear();
}

void mu::ui::window::CMoveCommandWindow::SettingCanMoveMap()
{
    for (auto* moveInfo : m_listMoveInfoData)
    {
        moveInfo->_bSelected = false;
        moveInfo->_bCanMove = EvaluateMove(*moveInfo) == MoveBlock::None;
    }
}

void mu::ui::window::CMoveCommandWindow::RmlWheelList(Rml::Event& event)
{
    // One row per notch: a row's height in the list's own (text-sized) layout.
    Rml::Element* list = event.GetCurrentElement();
    Rml::Element* row = list != nullptr && list->GetNumChildren() > 0 ? list->GetChild(0) : nullptr;
    const float rowStep = row != nullptr ? row->GetBox().GetSize(Rml::BoxArea::Border).y : 0.f;
    if (rowStep <= 0.f)
        return;

    // Stopping the event keeps RmlUi from scrolling the list itself.
    event.StopPropagation();
    const float notches = event.GetParameter("wheel_delta_y", 0.f);
    const float rowIndex = std::round(list->GetScrollTop() / rowStep) + notches;
    list->SetScrollTop(rowIndex * rowStep);
}

void mu::ui::window::CMoveCommandWindow::RmlClickWarp(int row)
{
    // The requirements are re-evaluated on the click itself, not trusted from the row model: the
    // list is rebuilt every frame, but level/zen/equipment can still change between the frame that
    // built the row and this one.
    SettingCanMoveMap();

    auto li = m_listMoveInfoData.begin();
    for (int i = 0; i < row && li != m_listMoveInfoData.end(); ++i)
        ++li;

    if (li == m_listMoveInfoData.end())
        return;
    if (!(*li)->_bCanMove)
    {
        PostMoveBlock(**li, EvaluateMove(**li));
        return;
    }

    if (IsTheMapInDifferentServer(gMapManager.WorldActive, (*li)->_ReqInfo.index))
    {
        SaveOptions();
    }

    SocketClient->ToGameServer()->SendWarpCommandRequest(GetMoveCommandKey(), (*li)->_ReqInfo.index);

    g_pNewUISystem->Hide(mu::ui::window::INTERFACE_MOVEMAP);
}

bool mu::ui::window::CMoveCommandWindow::UpdateMouseEvent()
{
    // Row hover, row clicks, the close bar and the scrollbar are all RmlUi's now. What is left is
    // the two things it has no part in: the picked-item backup, and claiming the panel so a click
    // on it doesn't fall through to the world.
    if (!UI::RmlBridge::IsPointerOver(m_RmlView.Document()))
        return true;

    if (IsPress(VK_LBUTTON))
    {
        mu::ui::window::CInventoryCtrl::BackupPickedItem();
    }

    // RmlUi scrolls the list off its own wheel event; this only stops the same notch reaching the
    // camera underneath, exactly as native did.
    MouseWheel = 0;

    return false;
}

bool mu::ui::window::CMoveCommandWindow::UpdateKeyEvent()
{
    if (IsVisible())
    {
        if (mu::ui::window::IsPress(VK_ESCAPE) == true)
        {
            Show(false);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }
    return true;
}

bool mu::ui::window::CMoveCommandWindow::Update()
{
    SyncRmlModel();
    return true;
}

bool mu::ui::window::CMoveCommandWindow::Render()
{
    // Nothing native left: the fill, the title, the column headers, every row, the close bar and
    // the scrollbar are all RmlUi. Kept because CObject requires the override.
    return true;
}

void mu::ui::window::CMoveCommandWindow::BindRmlModel(Rml::DataModelConstructor& c, MoveCommandRmlModel& model)
{
    c.Bind("text_px", &model.textPx);

    auto row = c.RegisterStruct<MoveCommandRowEntry>();
    row.RegisterMember("strife_text", &MoveCommandRowEntry::strifeText);
    row.RegisterMember("map_name", &MoveCommandRowEntry::mapName);
    row.RegisterMember("req_level", &MoveCommandRowEntry::reqLevel);
    row.RegisterMember("req_zen", &MoveCommandRowEntry::reqZen);
    row.RegisterMember("can_move", &MoveCommandRowEntry::canMove);
    row.RegisterMember("level_unmet", &MoveCommandRowEntry::levelUnmet);
    row.RegisterMember("zen_unmet", &MoveCommandRowEntry::zenUnmet);
    row.RegisterMember("index", &MoveCommandRowEntry::index);
    row.RegisterMember("here", &MoveCommandRowEntry::here);
    row.RegisterMember("back", &MoveCommandRowEntry::back);
    row.RegisterMember("mark_hint", &MoveCommandRowEntry::markHint);
    c.RegisterArray<std::vector<MoveCommandRowEntry>>();
    c.Bind("rows", &model.rows);

    c.Bind("title_text", &model.titleText);
    c.Bind("head_strife", &model.headStrife);
    c.Bind("head_map", &model.headMap);
    c.Bind("head_level", &model.headLevel);
    c.Bind("head_zen", &model.headZen);
    c.Bind("close_text", &model.closeText);

    c.BindEventCallback("movecommand_warp",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
        {
            if (arguments.size() == 1)
                RmlClickWarp(arguments[0].Get<int>(-1));
        });
    c.BindEventCallback("movecommand_wheel", [this](Rml::DataModelHandle, Rml::Event& event,
                                                    const Rml::VariantList&) { RmlWheelList(event); });
    c.BindEventCallback("movecommand_close",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_MOVEMAP);
        });

    model.titleText = StringUtils::WideToNarrow(I18N::Game::WarpCommandWindow);
    model.headStrife = StringUtils::WideToNarrow(I18N::Game::BattleZone);
    model.headMap = StringUtils::WideToNarrow(I18N::Game::Map);
    model.headLevel = StringUtils::WideToNarrow(I18N::Game::MinLevel);
    model.headZen = StringUtils::WideToNarrow(I18N::Game::Cost);
    model.closeText = StringUtils::WideToNarrow(I18N::Game::Close388);
    m_HereHint = StringUtils::WideToNarrow(I18N::Game::WarpListYouAreHere);
    m_BackHint = StringUtils::WideToNarrow(I18N::Game::WarpListYouDiedHere);
}

void mu::ui::window::CMoveCommandWindow::OnRmlBuilt()
{
    ApplyFillSize();
}

void mu::ui::window::CMoveCommandWindow::BuildRmlUi()
{
    m_RmlView.Ensure();
}

void mu::ui::window::CMoveCommandWindow::SyncRmlModel()
{
    BuildRmlUi();
    if (!m_RmlView.Document())
        return;

    UI::RmlBridge::SyncDocumentVisibility(m_RmlView.Document(), IsVisible());
    if (!IsVisible())
        return;

    UI::RmlBridge::SyncNativeTextSize(m_RmlView.Binder());

    SettingCanMoveMap();
    RebuildRowModel();

    // With every row in view the original drew its thumb disabled.
    if (Rml::Element* list = m_RmlView.Document()->GetElementById("list"))
        list->SetClass("no-scroll", list->GetScrollHeight() <= list->GetClientHeight() + 0.5f);

    ApplyOpeningScroll();
}

void mu::ui::window::CMoveCommandWindow::ApplyOpeningScroll()
{
    if (m_bRewindPending)
    {
        m_bRewindPending = false;
        if (Rml::Element* list = m_RmlView.Document()->GetElementById("list"))
            list->SetScrollTop(0.f);
        int hereRow = -1;
        FindMarkedRows(hereRow, m_ScrollToBackRow);
        m_ScrollToBackFrames = m_ScrollToBackRow >= 0 ? kScrollToBackFrames : 0;
        // The rows still show the previous opening until RmlUi applies this frame's model: look from the next one.
        return;
    }
    if (m_ScrollToBackFrames <= 0)
        return;

    --m_ScrollToBackFrames;
    m_RmlView.Document()->UpdateDocument();
    Rml::Element* list = m_RmlView.Document()->GetElementById("list");
    Rml::Element* backRow = list != nullptr ? list->QuerySelector(".mc-row.back") : nullptr;
    // Only the row this opening marked: an element left from an earlier opening has another index.
    if (backRow == nullptr || backRow->GetAttribute<int>("data-row", -1) != m_ScrollToBackRow)
        return;
    m_ScrollToBackFrames = 0;
    backRow->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Center, Rml::ScrollAlignment::Nearest,
                                                       Rml::ScrollBehavior::Instant, Rml::ScrollParentage::Closest));
}

void mu::ui::window::CMoveCommandWindow::FindMarkedRows(int& hereRow, int& backRow) const
{
    hereRow = -1;
    backRow = -1;
    if (Hero == nullptr)
        return;

    const auto& spots = m_RowSpots;
    hereRow = UI::MoveCommand::Marks::NearestRow(spots, gMapManager.WorldActive, Hero->PositionX, Hero->PositionY);

    int world = -1;
    int x = 0;
    int y = 0;
    if (!UI::MoveCommand::Marks::HeroDeathSpot().Get(GetTickCount(), world, x, y))
        return;
    backRow = UI::MoveCommand::Marks::NearestRow(spots, world, x, y);
    // Still where he died: the "here" mark says it all.
    if (backRow == hereRow)
        backRow = -1;
}

void mu::ui::window::CMoveCommandWindow::RebuildRowModel()
{
    MoveCommandRmlModel& model = m_RmlView.GetModel();

    std::vector<MoveCommandRowEntry> rows;
    rows.reserve(m_listMoveInfoData.size());

    const int iLevel = CharacterAttribute->Level;
    const DWORD iZen = CharacterMachine->Gold;
    const Rml::String strifeMark = StringUtils::WideToNarrow(I18N::Game::Battle2987);

    int hereRow = -1;
    int backRow = -1;
    FindMarkedRows(hereRow, backRow);
    const Rml::String& hereHint = m_HereHint;
    const Rml::String& backHint = m_BackHint;

    wchar_t szText[24];
    int index = 0;
    for (const auto* moveInfo : m_listMoveInfoData)
    {
        // The three-quarter requirement for the Dark classes, exactly as Render() computed it --
        // the displayed number is the adjusted one, not _ReqInfo.iReqLevel.
        int iReqLevel = moveInfo->_ReqInfo.iReqLevel;
        if ((gCharacterManager.GetBaseClass(CharacterAttribute->Class) == CLASS_DARK
             || gCharacterManager.GetBaseClass(CharacterAttribute->Class) == CLASS_DARK_LORD
             || gCharacterManager.GetBaseClass(CharacterAttribute->Class) == CLASS_RAGEFIGHTER)
            && (iReqLevel != 400))
        {
            iReqLevel = int(float(iReqLevel) * 2.f / 3.f);
        }

        MoveCommandRowEntry entry;
        entry.strifeText = moveInfo->_bStrife ? strifeMark : Rml::String();
        entry.mapName = StringUtils::WideToNarrow(moveInfo->_ReqInfo.szMainMapName);
        _itow(iReqLevel, szText, 10);
        entry.reqLevel = StringUtils::WideToNarrow(szText);
        entry.reqZen = StringUtils::WideToNarrow(UI::Text::GroupThousands(moveInfo->_ReqInfo.iReqZen).c_str());
        entry.canMove = moveInfo->_bCanMove;
        entry.levelUnmet = iReqLevel > iLevel;
        entry.zenUnmet = moveInfo->_ReqInfo.iReqZen > (int)iZen;
        entry.here = index == hereRow;
        entry.back = index == backRow;
        entry.markHint = entry.back ? backHint : entry.here ? hereHint : Rml::String();
        entry.index = index++;

        rows.push_back(std::move(entry));
    }

    // Only publish a genuine change. Marking the array dirty re-runs every row's text binding, and
    // this list is static for minutes at a time -- only a level-up, a zen change or an equipment
    // swap moves any of it.
    const bool changed = rows.size() != model.rows.size()
        || !std::equal(rows.begin(), rows.end(), model.rows.begin(),
                       [](const MoveCommandRowEntry& a, const MoveCommandRowEntry& b)
                       {
                           return a.canMove == b.canMove && a.levelUnmet == b.levelUnmet
                               && a.zenUnmet == b.zenUnmet && a.strifeText == b.strifeText
                               && a.reqLevel == b.reqLevel && a.reqZen == b.reqZen
                               && a.mapName == b.mapName && a.here == b.here && a.back == b.back;
                       });
    if (!changed)
        return;

    model.rows = std::move(rows);
    m_RmlView.MarkDirty("rows");
}

void mu::ui::window::CMoveCommandWindow::OpenningProcess()
{
    RefreshData();
    SetStrifeMap();
    SettingCanMoveMap();

    // Native reset its own scroll offset here; RmlUi owns the scroll position now, so the list
    // element is what has to be rewound -- and only once the document is actually visible, which
    // it is not yet: CSystem::Show() runs this before ShowInterface().
    m_bRewindPending = true;
}

void mu::ui::window::CMoveCommandWindow::ClosingProcess()
{
}

float mu::ui::window::CMoveCommandWindow::GetLayerDepth()
{
    return 8.3f;
}

BOOL CMoveCommandWindow::IsTheMapInDifferentServer(const int iFromMapIndex, const int iToMapIndex) const
{
    BOOL bInOtherServer = FALSE;

    switch (iFromMapIndex)
    {
    case WD_30BATTLECASTLE:
    case WD_79UNITEDMARKETPLACE:
        bInOtherServer = TRUE;
        break;
    default:
        break;
    }

    switch (iToMapIndex)
    {
    case 24:
    case 44:
        bInOtherServer = TRUE;
        break;
    default:
        break;
    }

    return bInOtherServer;
}

CMoveCommandData::MOVEINFODATA* CMoveCommandWindow::FindMoveInfo(const wchar_t* pszMapName)
{
    if (pszMapName == NULL)
        return NULL;

    for (auto* moveInfo : m_listMoveInfoData)
    {
        if (wcsicmp(moveInfo->_ReqInfo.szMainMapName, pszMapName) == 0 ||
            wcsicmp(moveInfo->_ReqInfo.szSubMapName, pszMapName) == 0)
        {
            return moveInfo;
        }
    }

    return NULL;
}

bool CMoveCommandWindow::CanMoveToMap(const wchar_t* pszMapName)
{
    // The requirements are recomputed first, exactly as the click path does before reading the
    // flag. This used to save and restore _bSelected around that call, because the same sweep
    // clears it and it was the hovered row's highlight -- the hover is an RCSS :hover rule now,
    // so nothing reads _bSelected any more and there is nothing to preserve.
    // The strife flags are otherwise only set when the window opens, and a query may come first.
    SetStrifeMap();
    SettingCanMoveMap();

    const CMoveCommandData::MOVEINFODATA* moveInfo = FindMoveInfo(pszMapName);
    return moveInfo != NULL && moveInfo->_bCanMove;
}

int CMoveCommandWindow::GetMapIndexFromMovereq(const wchar_t* pszMapName)
{
    const CMoveCommandData::MOVEINFODATA* moveInfo = FindMoveInfo(pszMapName);
    return moveInfo != NULL ? moveInfo->_ReqInfo.index : -1;
}
