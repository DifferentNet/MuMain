
#include "stdafx.h"
#include "Core/Input/KeyBindingsRuntime.h"
#include "UI/HUD/KeyHint.h"
#include <algorithm>
#include "I18N/All.h"

#include "UI/HUD/MainFrameWindow.h"	// self
#include "UI/Options/OptionWindow.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Widgets/UIBaseDef.h"
#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Render/Models/ZzzBMD.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzInventory.h"

#include "GameLogic/Items/CSItemOption.h"
#include "GameLogic/Events/CSChaosCastle.h"
#include "World/MapInfra/MapManager.h"
#include "Character/CharacterManager.h"
#include "UI/HUD/HeroGauges.h"
#include "UI/Mobile/InterfaceMode.h"
#include "GameLogic/Skills/SkillManager.h"
#include "UI/HUD/Skills/SkillIconAtlas.h"
#include "UI/HUD/Skills/SkillTooltip.h"
#include "UI/Scaling/UITransform.h"
#include "Core/Time/CTimCheck.h"
#include "GameLogic/Social/MonkSystem.h"

#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
#include "GameShop/InGameShopSystem.h"
#endif //PBG_ADD_INGAMESHOP_UI_MAINFRAME

// RmlUi migration -- see this class's header comment.
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Renderer/MuRenderer.h"
#include "Camera/CameraProjection.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlWorkspaceParticipant.h"
#include "UI/Placement/WindowPlacement.h"
#include "UI/RmlBridge/RmlTooltip.h"
#include "Core/Utilities/StringUtils.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include "GameLogic/Quests/QuestMng.h"
#include "UI/Social/FriendWindow.h"

mu::ui::window::CMainFrameWindow::CMainFrameWindow()
{
    m_bExpEffect = false;
    m_dwExpEffectTime = 0;
    m_dwPreExp = 0;
    m_dwGetExp = 0;
    m_bButtonBlink = false;
}

mu::ui::window::CMainFrameWindow::~CMainFrameWindow()
{
    Release();
}

bool mu::ui::window::CMainFrameWindow::Create(CManager* pNewUIMng)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_MAINFRAME, this);

    UI::RmlBridge::RegisterWorkspaceDocument("main_hud", [this] { return m_RmlView.Document(); }, "hud_layout");
    UI::RmlBridge::RegisterWorkspaceDocument("top_bar", [this] { return m_RmlView.Document(1); }, "buttons_top");

    // Builds once; Create() re-runs on resolution change.
    BuildRmlUi();

    Show(true);

    return true;
}

namespace
{
constexpr float kSkillIconWidth = 20.f;
constexpr float kSkillIconHeight = 28.f;

struct SlotBox
{
    float left = 0.f, top = 0.f, width = 0.f, height = 0.f;
};

// The skill tooltips' frame: the screen in the original's reference units, i.e. scaled by the dp
// ratio main_frame.rcss is sized in, so the original's offsets (10 above a box, ...) keep their size.
UI::Scaling::Transform HudReferenceTransform(Rml::ElementDocument* document)
{
    const Rml::Context* context = document ? document->GetContext() : nullptr;
    const float ratio = context ? context->GetDensityIndependentPixelRatio() : 1.f;
    return {ratio, ratio, 0.f, 0.f, ratio};
}

// A hovered slot or cell's box in HudReferenceTransform()'s units, wherever the theme put it.
SlotBox SlotBoxInReference(Rml::Element* element)
{
    SlotBox box;
    if (element == nullptr)
        return box;
    const UI::Scaling::Transform bars = HudReferenceTransform(element->GetOwnerDocument());
    const Rml::Vector2f offset = element->GetAbsoluteOffset(Rml::BoxArea::Border);
    const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
    box.left = UI::Scaling::LogicalX(bars, offset.x);
    box.top = UI::Scaling::LogicalY(bars, offset.y);
    box.width = size.x / bars.scaleX;
    box.height = size.y / bars.scaleY;
    return box;
}
} // namespace

void mu::ui::window::CMainFrameWindow::BindRmlModel(Rml::DataModelConstructor& c, MainFrameRmlModel& model)
{

    c.Bind("hp_fraction", &model.hpFraction);
    c.Bind("mp_fraction", &model.mpFraction);
    c.Bind("ag_fraction", &model.agFraction);
    c.Bind("sd_fraction", &model.sdFraction);
    c.Bind("hp_text", &model.hpText);
    c.Bind("mp_text", &model.mpText);
    c.Bind("ag_text", &model.agText);
    c.Bind("sd_text", &model.sdText);
    c.Bind("hp_current_text", &model.hpCurrentText);
    c.Bind("mp_current_text", &model.mpCurrentText);
    c.Bind("ag_current_text", &model.agCurrentText);
    c.Bind("sd_current_text", &model.sdCurrentText);
    c.Bind("hp_tooltip", &model.hpTooltip);
    c.Bind("mp_tooltip", &model.mpTooltip);
    c.Bind("ag_tooltip", &model.agTooltip);
    c.Bind("sd_tooltip", &model.sdTooltip);
    c.Bind("poisoned", &model.poisoned);

    c.Bind("exp_fraction", &model.expFraction);
    c.Bind("exp_digit", &model.expDigit);
    c.Bind("exp_tooltip", &model.expTooltip);

    // rml-contract-unused: cshop_open: no MU Item Shop button since 2026-10-24 (the server has no cash shop); its slot is the battle pass button's
    c.Bind("cshop_open", &model.cShopOpen);
    c.Bind("battle_pass_open", &model.battlePassOpen);
    c.Bind("chainfo_open", &model.chaInfoOpen);
    c.Bind("myinven_open", &model.myInvenOpen);
    c.Bind("friend_open", &model.friendOpen);
    c.Bind("window_open", &model.windowOpen);
    // rml-contract-unused: cshop_tooltip: the MU Item Shop button's tooltip, see cshop_open
    c.Bind("cshop_tooltip", &model.cShopTooltip);
    c.Bind("battle_pass_tooltip", &model.battlePassTooltip);
    c.Bind("chainfo_tooltip", &model.chaInfoTooltip);
    c.Bind("myinven_tooltip", &model.myInvenTooltip);
    c.Bind("friend_tooltip", &model.friendTooltip);
    c.Bind("window_tooltip", &model.windowTooltip);

    c.Bind("chainfo_alert", &model.chaInfoAlert);
    c.Bind("friend_alert", &model.friendAlert);

    c.Bind("skill_slot_0_selected", &model.skillSlot0Selected);
    c.Bind("skill_slot_1_selected", &model.skillSlot1Selected);
    c.Bind("skill_slot_2_selected", &model.skillSlot2Selected);
    c.Bind("skill_slot_3_selected", &model.skillSlot3Selected);
    c.Bind("skill_slot_4_selected", &model.skillSlot4Selected);

    c.Bind("skill_slot_0_hotkey", &model.skillSlot0Hotkey);
    c.Bind("skill_slot_1_hotkey", &model.skillSlot1Hotkey);
    c.Bind("skill_slot_2_hotkey", &model.skillSlot2Hotkey);
    c.Bind("skill_slot_3_hotkey", &model.skillSlot3Hotkey);
    c.Bind("skill_slot_4_hotkey", &model.skillSlot4Hotkey);

    c.Bind("skill_slot_0_icon", &model.skillSlot0Icon);
    c.Bind("skill_slot_1_icon", &model.skillSlot1Icon);
    c.Bind("skill_slot_2_icon", &model.skillSlot2Icon);
    c.Bind("skill_slot_3_icon", &model.skillSlot3Icon);
    c.Bind("skill_slot_4_icon", &model.skillSlot4Icon);
    c.Bind("current_skill_icon", &model.currentSkillIcon);
    c.Bind("current_skill_hotkey", &model.currentSkillHotkey);

    // Skill list cooldown bindings -- see MainFrameRmlModel::skillGridOpen's own
    // header comment.
    c.Bind("skill_slot_0_cooldown", &model.skillSlot0Cooldown);
    c.Bind("skill_slot_1_cooldown", &model.skillSlot1Cooldown);
    c.Bind("skill_slot_2_cooldown", &model.skillSlot2Cooldown);
    c.Bind("skill_slot_3_cooldown", &model.skillSlot3Cooldown);
    c.Bind("skill_slot_4_cooldown", &model.skillSlot4Cooldown);
    c.Bind("current_skill_cooldown", &model.currentSkillCooldown);

    // See CCharMakeWin::BindRmlModel()'s comment on why this must re-run in full every
    // call, including on a theme switch -- no guard here.
    auto skillCell = c.RegisterStruct<SkillCellEntry>();
    skillCell.RegisterMember("skill_index", &SkillCellEntry::skillIndex);
    skillCell.RegisterMember("is_pet", &SkillCellEntry::isPet);
    skillCell.RegisterMember("is_current", &SkillCellEntry::isCurrent);
    skillCell.RegisterMember("cooldown_fraction", &SkillCellEntry::cooldownFraction);
    skillCell.RegisterMember("icon", &SkillCellEntry::icon);
    skillCell.RegisterMember("hotkey", &SkillCellEntry::hotkey);
    c.RegisterArray<std::vector<SkillCellEntry>>();

    c.Bind("skill_grid_open", &model.skillGridOpen);
    c.Bind("skill_list_up", &model.skillListUp);
    c.Bind("skill_grid_cells", &model.skillGridCells);
    c.Bind("pet_skill_cells", &model.petSkillCells);

    // Skill list click/hover bindings route into CSkillList (g_pSkillList has no RmlUi
    // doc of its own). Args are literal ints in RML (e.g. skill_hotkey_click(0)) or the
    // cell's skill_index for data-for'd lists; Variant::Get<int>() resolves either.
    c.BindEventCallback("skill_hotkey_click",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) { g_pSkillList->OnHotkeySlotClick(args.empty() ? 0 : args[0].Get<int>()); });
    c.BindEventCallback("skill_hotkey_hover",
                        [](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& args)
                        {
                            const SlotBox slot = SlotBoxInReference(event.GetCurrentElement());
                            g_pSkillList->OnHotkeySlotHover(args.empty() ? 0 : args[0].Get<int>(),
                                                            slot.left, slot.top);
                        });
    c.BindEventCallback("skill_current_click",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { g_pSkillList->OnCurrentSkillClick(); });
    c.BindEventCallback("skill_current_hover",
                        [](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&)
                        {
                            // The slot box is the icon plus the theme's even inset.
                            const SlotBox slot = SlotBoxInReference(event.GetCurrentElement());
                            g_pSkillList->OnCurrentSkillHover(
                                slot.left + (slot.width - kSkillIconWidth) / 2.f,
                                slot.top + (slot.height - kSkillIconHeight) / 2.f);
                        });
    c.BindEventCallback("skill_grid_click",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) { g_pSkillList->OnGridCellClick(args.empty() ? -1 : args[0].Get<int>()); });
    c.BindEventCallback("skill_grid_hover",
                        [](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& args)
                        {
                            const SlotBox cell = SlotBoxInReference(event.GetCurrentElement());
                            g_pSkillList->OnGridCellHover(args.empty() ? -1 : args[0].Get<int>(),
                                                          cell.left, cell.top);
                        });
    c.BindEventCallback("skill_pet_click",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) { g_pSkillList->OnPetCellClick(args.empty() ? -1 : args[0].Get<int>()); });
    c.BindEventCallback("skill_pet_hover",
                        [](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& args)
                        {
                            const SlotBox cell = SlotBoxInReference(event.GetCurrentElement());
                            g_pSkillList->OnPetCellHover(args.empty() ? -1 : args[0].Get<int>(),
                                                         cell.left, cell.top);
                        });
    c.BindEventCallback("skill_unhover",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { g_pSkillList->OnUnhover(); });

    // Item hotkey chrome (#item_slots hover/stack-count/right-click). m_ItemHotKey is
    // a member, so these lambdas capture [this] directly.
    c.Bind("item_slot_0_hovered", &model.itemSlot0Hovered);
    c.Bind("item_slot_1_hovered", &model.itemSlot1Hovered);
    c.Bind("item_slot_2_hovered", &model.itemSlot2Hovered);
    c.Bind("item_slot_3_hovered", &model.itemSlot3Hovered);
    c.Bind("item_slot_0_count", &model.itemSlot0Count);
    c.Bind("item_slot_1_count", &model.itemSlot1Count);
    c.Bind("item_slot_2_count", &model.itemSlot2Count);
    c.Bind("item_slot_3_count", &model.itemSlot3Count);
    c.Bind("item_slot_0_key", &model.itemSlot0Key);
    c.Bind("item_slot_1_key", &model.itemSlot1Key);
    c.Bind("item_slot_2_key", &model.itemSlot2Key);
    c.Bind("item_slot_3_key", &model.itemSlot3Key);

    c.BindEventCallback("item_hotkey_hover",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) { m_ItemHotKey.OnHotkeySlotHover(args.empty() ? 0 : args[0].Get<int>()); });
    c.BindEventCallback("item_hotkey_unhover",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_ItemHotKey.OnUnhover(); });
    // Uses data-event-mouseup, not data-event-click -- RmlUi's Context only dispatches
    // Click for the left button. Mouseup fires for any button; "button" param == 1 means right-click.
    c.BindEventCallback("item_hotkey_rightclick",
        [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& args)
        {
            if (event.GetParameter<int>("button", -1) != 1) return;
            m_ItemHotKey.OnHotkeySlotRightClick(args.empty() ? 0 : args[0].Get<int>());
        });

    c.BindEventCallback("mainframe_cshop_click",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { RmlClickCShop(); });
    c.BindEventCallback("mainframe_battle_pass_click",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { RmlClickBattlePass(); });
    c.BindEventCallback("mainframe_chainfo_click",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { RmlClickChaInfo(); });
    c.BindEventCallback("mainframe_myinven_click",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { RmlClickMyInven(); });
    c.BindEventCallback("mainframe_friend_click",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { RmlClickFriend(); });
    c.BindEventCallback("mainframe_window_click",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { RmlClickWindow(); });
}

void mu::ui::window::CMainFrameWindow::OnRmlReloaded()
{
    UI::Placement::Invalidate();
}

// Not shown here: Create() runs before SceneFlag reaches MAIN_SCENE, and an eager Show() let the
// HUD flash once before the first scene gate check. SyncDocVisibility() shows it.
void mu::ui::window::CMainFrameWindow::BuildRmlUi()
{
    m_RmlView.Ensure();
}

void mu::ui::window::CMainFrameWindow::Release()
{
    UI::Placement::UnregisterParticipant("main_hud");
    UI::Placement::UnregisterParticipant("top_bar");
    m_ItemHotKey.SetSlotIconsShown(false);

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }

    m_RmlView.Release();
}

// Everything this window shows is in main_frame.rml; SyncRmlModel() feeds it from Update().
bool mu::ui::window::CMainFrameWindow::Render()
{
    return true;
}

bool mu::ui::window::CMainFrameWindow::IsVisible() const
{
    return CObject::IsVisible();
}

bool mu::ui::window::CMainFrameWindow::IsMouseOverHud() const
{
    if (m_RmlView.Document() == nullptr || !m_RmlView.Document()->IsVisible())
        return false;
    Rml::Context* context = m_RmlView.Document()->GetContext();
    Rml::Element* hover = context ? context->GetHoverElement() : nullptr;
    return hover != nullptr && hover != m_RmlView.Document() && hover->GetOwnerDocument() == m_RmlView.Document();
}

// RenderRightFrame()/RenderExperienceBackground()/RenderLifeMana()/RenderGuageAG()/RenderGuageSD()/
// RenderExperience() removed -- moved to RmlUi (see SyncRmlModel()).

// RenderButtons()/RenderCharInfoButton()/RenderFriendButton()/RenderFriendButtonState()/BtnProcess()
// removed -- the 5 corner buttons moved to RmlUi (see Create()).
bool mu::ui::window::CMainFrameWindow::UpdateMouseEvent()
{
    // RmlUi's own context does hit-testing now; never consumes the legacy mouse event.
    return true;
}

bool mu::ui::window::CMainFrameWindow::UpdateKeyEvent()
{
    if (m_ItemHotKey.UpdateKeyEvent() == false)
    {
        return false;
    }
    return true;
}

bool mu::ui::window::CMainFrameWindow::Update()
{
    if (m_bExpEffect == true)
    {
        if (timeGetTime() - m_dwExpEffectTime > 2000)
        {
            m_bExpEffect = false;
            m_dwExpEffectTime = 0;
            m_dwGetExp = 0;
        }
    }

    // Button clicks, polled-and-cleared like other windows' RmlClickX() pattern. Logic ported
    // verbatim from the legacy BtnProcess(), minus the CButton hit-test wrapper.
    if (m_bRmlMyInvenClicked)
    {
        m_bRmlMyInvenClicked = false;
        g_pNewUISystem->Toggle(mu::ui::window::INTERFACE_INVENTORY);
        PlayBuffer(SOUND_CLICK01);
    }
    if (m_bRmlChaInfoClicked)
    {
        m_bRmlChaInfoClicked = false;
        g_pNewUISystem->Toggle(mu::ui::window::INTERFACE_CHARACTER);
        PlayBuffer(SOUND_CLICK01);
        if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_CHARACTER))
            g_QuestMng.SendQuestIndexByEtcSelection();
    }
    if (m_bRmlFriendClicked)
    {
        m_bRmlFriendClicked = false;
        if (gMapManager.InChaosCastle() == true)
        {
            PlayBuffer(SOUND_CLICK01);
        }
        else
        {
            int iLevel = CharacterAttribute->Level;
            if (iLevel < 6)
            {
                if (g_pSystemLogBox->CheckChatRedundancy(I18N::Game::YouMustBeAtLeastLevel6ToUseTheMyFriendFunction) == FALSE)
                {
                    g_pSystemLogBox->AddText(I18N::Game::YouMustBeAtLeastLevel6ToUseTheMyFriendFunction, mu::ui::window::TYPE_SYSTEM_MESSAGE);
                }
            }
            else
            {
                g_pNewUISystem->Toggle(mu::ui::window::INTERFACE_FRIEND);
            }
            PlayBuffer(SOUND_CLICK01);
        }
    }
    if (m_bRmlWindowClicked)
    {
        m_bRmlWindowClicked = false;
        g_pNewUISystem->Toggle(mu::ui::window::INTERFACE_WINDOW_MENU);
        PlayBuffer(SOUND_CLICK01);
    }
    if (m_bRmlBattlePassClicked)
    {
        // Same as key X (HotKey.cpp).
        m_bRmlBattlePassClicked = false;
        g_pNewUISystem->Toggle(mu::ui::window::INTERFACE_BATTLE_PASS);
        PlayBuffer(SOUND_CLICK01);
    }
#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
    if (m_bRmlCShopClicked)
    {
        m_bRmlCShopClicked = false;
        if (g_pInGameShop->IsInGameShopOpen() == false)
        {
            // Nothing to do until the server confirms the shop is open (matches legacy BtnProcess()).
        }
        else
        {
#ifdef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
            if (g_InGameShopSystem->IsScriptDownload() == true)
            {
                g_InGameShopSystem->ScriptDownload();
            }
            if (g_InGameShopSystem->IsBannerDownload() == true)
            {
                g_InGameShopSystem->BannerDownload();
            }
#endif // KJH_MOD_SHOP_SCRIPT_DOWNLOAD
            if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_INGAMESHOP) == false)
            {
                if (g_InGameShopSystem->GetIsRequestShopOpenning() == false)
                {
                    SocketClient->ToGameServer()->SendCashShopOpenState(0);
                    g_InGameShopSystem->SetIsRequestShopOpenning(true);
#ifdef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
                    SetBtnState(MAINFRAME_BTN_PARTCHARGE, true);
#endif // KJH_MOD_SHOP_SCRIPT_DOWNLOAD
                }
            }
            else
            {
                SocketClient->ToGameServer()->SendCashShopOpenState(1);
                g_pNewUISystem->Hide(mu::ui::window::INTERFACE_INGAMESHOP);
            }
        }
    }
#endif //defined PBG_ADD_INGAMESHOP_UI_MAINFRAME

    SyncRmlModel();

    return true;
}

namespace
{
// The gauge readouts' formats; their addresses are part of TextSource's key.
constexpr wchar_t kCurrentOfMaxFormat[] = L"%d / %d";
constexpr wchar_t kNumberFormat[] = L"%d";
} // namespace

void mu::ui::window::CMainFrameWindow::SyncFloat(float MainFrameRmlModel::* field, const char* boundName, float value)
{
    if (m_RmlView.GetModel().*field != value)
    {
        m_RmlView.GetModel().*field = value;
        m_RmlView.MarkDirty(boundName);
    }
}

void mu::ui::window::CMainFrameWindow::SyncBool(bool MainFrameRmlModel::* field, const char* boundName, bool value)
{
    if (m_RmlView.GetModel().*field != value)
    {
        m_RmlView.GetModel().*field = value;
        m_RmlView.MarkDirty(boundName);
    }
}

void mu::ui::window::CMainFrameWindow::SyncText(Rml::String MainFrameRmlModel::* field, const char* boundName,
                                                const Rml::String& value)
{
    if (m_RmlView.GetModel().*field != value)
    {
        m_RmlView.GetModel().*field = value;
        m_RmlView.MarkDirty(boundName);
    }
}

void mu::ui::window::CMainFrameWindow::SyncWide(Rml::String MainFrameRmlModel::* field, const char* boundName,
                                                const wchar_t* text)
{
    SyncText(field, boundName, StringUtils::WideToNarrow(text));
}

void mu::ui::window::CMainFrameWindow::ResetSyncCaches()
{
    m_TextSources = {};
    m_bGridSynced = false;
}

void mu::ui::window::CMainFrameWindow::SyncRmlModel()
{
    Rml::ElementDocument* document = m_RmlView.Document();
    if (!document)
    {
        m_pSyncedDocument = nullptr;
        return;
    }

    // A rebuilt document can come with its model reset to the defaults, and leaving the phone's HUD
    // shows parts not synced while it was on: build every text again then.
    const bool phoneHud = UI::Mobile::ReplacesPcHud();
    if (document != m_pSyncedDocument || phoneHud != m_bSyncedPhoneHud)
    {
        ResetSyncCaches();
        m_pSyncedDocument = document;
        m_bSyncedPhoneHud = phoneHud;
    }

    // Legacy RenderLifeMana()'s heartbeat under 20 % life, in either HUD.
    const UI::HUD::HeroGauges::Gauge life = UI::HUD::HeroGauges::Life();
    if (life.max > 0 && life.current > 0 && (life.current / (float)life.max) < 0.2f)
        PlayBuffer(SOUND_HEART);

    if (phoneHud)
    {
        // The phone's HUD hides the bar's gauges, buttons, skill row and item slots (main_frame.rcss),
        // so their texts, icons and stack counts are not built each frame (per-frame cost).
        m_ItemHotKey.StopSlotIcons();
    }
    else
    {
        m_ItemHotKey.SyncSlotIcons(document);
        SyncGauges();
        SyncExperience();
        SyncButtons();
        SyncSkillRow();
        SyncItemSlots();
    }

    SyncAlerts();
    SyncSkillList();
    SyncSkillTooltip();
}

void mu::ui::window::CMainFrameWindow::SyncGauges()
{
    // HP/MP -- legacy RenderLifeMana(). fLife/fMana there are the EMPTY fraction; store filled.
    const UI::HUD::HeroGauges::Gauge life = UI::HUD::HeroGauges::Life();
    const UI::HUD::HeroGauges::Gauge mana = UI::HUD::HeroGauges::Mana();
    const DWORD wLifeMax = life.max;
    const DWORD wLife = life.current;
    const DWORD wManaMax = mana.max;
    const DWORD wMana = mana.current;

    SyncFloat(&MainFrameRmlModel::hpFraction, "hp_fraction", wLifeMax > 0 ? wLife / (float)wLifeMax : 0.f);
    SyncFloat(&MainFrameRmlModel::mpFraction, "mp_fraction", wManaMax > 0 ? wMana / (float)wManaMax : 0.f);
    SyncBool(&MainFrameRmlModel::poisoned, "poisoned", g_isCharacterBuff((&Hero->Object), eDeBuff_Poison));

    // "X / Y" (current/max), not just "X" -- shared model field, both themes. Each text is formatted
    // only when its numbers (or the locale's format) changed (TextSource; per-frame cost).
    wchar_t szNum[32] = {};
    wchar_t szTip[256] = {};
    TextSources& sources = m_TextSources;
    if (sources.hpText.Changed(kCurrentOfMaxFormat, wLife, wLifeMax))
    {
        mu_swprintf(szNum, kCurrentOfMaxFormat, wLife, wLifeMax);
        SyncWide(&MainFrameRmlModel::hpText, "hp_text", szNum);
    }
    if (sources.mpText.Changed(kCurrentOfMaxFormat, wMana, wManaMax))
    {
        mu_swprintf(szNum, kCurrentOfMaxFormat, wMana, wManaMax);
        SyncWide(&MainFrameRmlModel::mpText, "mp_text", szNum);
    }
    if (sources.hpCurrent.Changed(kNumberFormat, wLife))
    {
        mu_swprintf(szNum, kNumberFormat, wLife);
        SyncWide(&MainFrameRmlModel::hpCurrentText, "hp_current_text", szNum);
    }
    if (sources.mpCurrent.Changed(kNumberFormat, wMana))
    {
        mu_swprintf(szNum, kNumberFormat, wMana);
        SyncWide(&MainFrameRmlModel::mpCurrentText, "mp_current_text", szNum);
    }
    if (sources.hpTooltip.Changed(I18N::Game::LifeDD, wLife, wLifeMax))
    {
        mu_swprintf(szTip, I18N::Game::LifeDD, wLife, wLifeMax);
        SyncWide(&MainFrameRmlModel::hpTooltip, "hp_tooltip", szTip);
    }
    if (sources.mpTooltip.Changed(I18N::Game::ManaDD359, wMana, wManaMax))
    {
        mu_swprintf(szTip, I18N::Game::ManaDD359, wMana, wManaMax);
        SyncWide(&MainFrameRmlModel::mpTooltip, "mp_tooltip", szTip);
    }

    // AG (stamina/skill-mana) -- legacy RenderGuageAG().
    const UI::HUD::HeroGauges::Gauge skillMana = UI::HUD::HeroGauges::SkillMana();
    const DWORD dwMaxSkillMana = skillMana.max;
    const DWORD dwSkillMana = skillMana.current;
    SyncFloat(&MainFrameRmlModel::agFraction, "ag_fraction", dwSkillMana / (float)dwMaxSkillMana);
    if (sources.agText.Changed(kCurrentOfMaxFormat, dwSkillMana, dwMaxSkillMana))
    {
        mu_swprintf(szNum, kCurrentOfMaxFormat, dwSkillMana, dwMaxSkillMana);
        SyncWide(&MainFrameRmlModel::agText, "ag_text", szNum);
    }
    if (sources.agCurrent.Changed(kNumberFormat, dwSkillMana))
    {
        mu_swprintf(szNum, kNumberFormat, dwSkillMana);
        SyncWide(&MainFrameRmlModel::agCurrentText, "ag_current_text", szNum);
    }
    if (sources.agTooltip.Changed(I18N::Game::AGDD, dwSkillMana, dwMaxSkillMana))
    {
        mu_swprintf(szTip, I18N::Game::AGDD, dwSkillMana, dwMaxSkillMana);
        SyncWide(&MainFrameRmlModel::agTooltip, "ag_tooltip", szTip);
    }

    // SD (shield) -- legacy RenderGuageSD().
    const UI::HUD::HeroGauges::Gauge shield = UI::HUD::HeroGauges::Shield();
    const DWORD wMaxShield = shield.max;
    const DWORD wShield = shield.current;
    SyncFloat(&MainFrameRmlModel::sdFraction, "sd_fraction", wShield / (float)wMaxShield);
    if (sources.sdText.Changed(kCurrentOfMaxFormat, wShield, wMaxShield))
    {
        mu_swprintf(szNum, kCurrentOfMaxFormat, wShield, wMaxShield);
        SyncWide(&MainFrameRmlModel::sdText, "sd_text", szNum);
    }
    if (sources.sdCurrent.Changed(kNumberFormat, wShield))
    {
        mu_swprintf(szNum, kNumberFormat, wShield);
        SyncWide(&MainFrameRmlModel::sdCurrentText, "sd_current_text", szNum);
    }
    if (sources.sdTooltip.Changed(I18N::Game::SDDD, wShield, wMaxShield))
    {
        mu_swprintf(szTip, I18N::Game::SDDD, wShield, wMaxShield);
        SyncWide(&MainFrameRmlModel::sdTooltip, "sd_tooltip", szTip);
    }
}

void mu::ui::window::CMainFrameWindow::SyncExperience()
{
    // EXP -- legacy RenderExperience(), flash-highlight overlay intentionally not reproduced (see
    // this class's header comment). expFraction is progress *within* the current decile, matching
    // buildExpSegment()'s original digit/fraction split.
    const UI::HUD::HeroGauges::Experience experience = UI::HUD::HeroGauges::ReadExperience();
    const __int64 dwNexExperience = experience.next;
    const __int64 dwExperience = experience.current;
    const double fRatio = experience.progress;

    const double scaled = std::clamp(fRatio, 0.0, 1.0) * 10.0;
    int iExp = std::clamp(static_cast<int>(scaled), 0, 9);
    double fProgress = std::clamp(scaled - static_cast<double>(static_cast<long long>(scaled)), 0.0, 1.0);
    if (fRatio >= 1.0) { iExp = 9; fProgress = 1.0; }

    SyncFloat(&MainFrameRmlModel::expFraction, "exp_fraction", static_cast<float>(fProgress));
    if (m_TextSources.expDigit.Changed(kNumberFormat, iExp))
    {
        wchar_t szExp[8] = {};
        mu_swprintf(szExp, kNumberFormat, iExp);
        SyncWide(&MainFrameRmlModel::expDigit, "exp_digit", szExp);
    }
    if (m_TextSources.expTooltip.Changed(I18N::Game::EXPI64dI64d, dwExperience, dwNexExperience))
    {
        wchar_t szTip[256] = {};
        mu_swprintf(szTip, I18N::Game::EXPI64dI64d, static_cast<unsigned long long>(dwExperience),
                    static_cast<unsigned long long>(dwNexExperience));
        SyncWide(&MainFrameRmlModel::expTooltip, "exp_tooltip", szTip);
    }
}

void mu::ui::window::CMainFrameWindow::SyncButtons()
{
    // Button tooltips: static strings, converted again only when the locale gives another one
    // (per-frame cost).
    // The key in each tooltip is the one the player bound (UI::KeyHint), rebuilt when a binding changes.
    using KeyAction = Core::Input::KeyBindings::Action;
    TextSources& sources = m_TextSources;
    const long long keys = Core::Input::KeyBindings::Runtime::Revision();
    if (sources.chaInfoTooltip.Changed(I18N::Game::CharacterC, keys))
        SyncWide(&MainFrameRmlModel::chaInfoTooltip, "chainfo_tooltip",
                 UI::KeyHint::WithKey(I18N::Game::CharacterC, KeyAction::Character).c_str());
    if (sources.myInvenTooltip.Changed(I18N::Game::InventoryIV, keys))
        SyncWide(&MainFrameRmlModel::myInvenTooltip, "myinven_tooltip",
                 UI::KeyHint::WithKey(I18N::Game::InventoryIV, KeyAction::Inventory, KeyAction::InventoryAlt).c_str());
    if (sources.friendTooltip.Changed(I18N::Game::FriendF, keys))
        SyncWide(&MainFrameRmlModel::friendTooltip, "friend_tooltip",
                 UI::KeyHint::WithKey(I18N::Game::FriendF, KeyAction::Friends).c_str());
    if (sources.windowTooltip.Changed(I18N::Game::MenuU, keys))
        SyncWide(&MainFrameRmlModel::windowTooltip, "window_tooltip",
                 UI::KeyHint::WithKey(I18N::Game::MenuU, KeyAction::WindowMenu).c_str());
    if (sources.battlePassTooltip.Changed(I18N::Game::BattlePassX, keys))
        SyncWide(&MainFrameRmlModel::battlePassTooltip, "battle_pass_tooltip",
                 UI::KeyHint::WithKey(I18N::Game::BattlePassX, KeyAction::BattlePass).c_str());
    // No SetBtnState() for the battle pass: the window opens from several places, so its state is read here.
    SyncBool(&MainFrameRmlModel::battlePassOpen, "battle_pass_open",
             g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_BATTLE_PASS));
#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
    if (sources.cShopTooltip.Changed(I18N::Game::MUItemShopX))
        SyncWide(&MainFrameRmlModel::cShopTooltip, "cshop_tooltip", I18N::Game::MUItemShopX);
#endif //defined PBG_ADD_INGAMESHOP_UI_MAINFRAME
}

void mu::ui::window::CMainFrameWindow::SyncAlerts()
{
    // Char-info quest-available blink -- legacy RenderCharInfoButton().
    bool chaInfoAlert = false;
    if (!g_QuestMng.IsQuestIndexByEtcListEmpty())
    {
        if (g_Time.GetTimeCheck(5, 500))
            m_bButtonBlink = !m_bButtonBlink;
        chaInfoAlert = m_bButtonBlink
            && !(g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_QUEST_PROGRESS_ETC)
                || g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_CHARACTER));
    }
    SyncBool(&MainFrameRmlModel::chaInfoAlert, "chainfo_alert", chaInfoAlert);

    // Friend mail/chat blink -- legacy RenderFriendButton()/RenderFriendButtonState().
    const int iBlinkTemp = g_pFriendMenu->GetBlinkTemp();
    const bool bIsAlertTime = (iBlinkTemp % 24 < 12);
    bool friendAlert = false;
    if (g_pFriendMenu->IsNewChatAlert() && bIsAlertTime)
    {
        friendAlert = true;
    }
    if (g_pFriendMenu->IsNewMailAlert())
    {
        if (bIsAlertTime)
        {
            friendAlert = true;
            if (iBlinkTemp % 24 == 11)
                g_pFriendMenu->IncreaseLetterBlink();
        }
    }
    else if (g_pLetterList->CheckNoReadLetter())
    {
        friendAlert = true;
    }
    g_pFriendMenu->IncreaseBlinkTemp();
    SyncBool(&MainFrameRmlModel::friendAlert, "friend_alert", friendAlert);
}

namespace
{
// The key the player bound to potion slot N (HOTKEY_Q..HOTKEY_R). None while the pad plays: its row over
// the bar names the pad's buttons, and a keyboard letter there only misleads.
Rml::String PotionKeyText(int slot)
{
    if (UI::Mobile::IsGamepadInUse())
        return Rml::String();
    namespace KeyBindings = Core::Input::KeyBindings;
    return KeyBindings::KeyName(KeyBindings::Runtime::KeyOf(KeyBindings::PotionAction(slot)));
}

// The key the player bound to skill hot key N ("7", "Q", "Home"), or an empty string for -1 (no slot)
// and while the pad plays (as PotionKeyText).
Rml::String HotkeyText(int hotkey)
{
    if (hotkey < 0 || UI::Mobile::IsGamepadInUse())
        return Rml::String();

    namespace KeyBindings = Core::Input::KeyBindings;
    return KeyBindings::KeyName(KeyBindings::Runtime::KeyOf(KeyBindings::SkillSlotAction(hotkey)));
}
} // namespace

void mu::ui::window::CMainFrameWindow::SyncSkillRow()
{
    // Skill-hotkey row selection highlight (legacy: the IMAGE_SKILLBOX_USE sprite, modern: an
    // outline; see MainFrameRmlModel::skillSlot0Selected and CSkillList::IsHotKeySlotCurrentSkill()).
    SyncBool(&MainFrameRmlModel::skillSlot0Selected, "skill_slot_0_selected", g_pSkillList->IsHotKeySlotCurrentSkill(0));
    SyncBool(&MainFrameRmlModel::skillSlot1Selected, "skill_slot_1_selected", g_pSkillList->IsHotKeySlotCurrentSkill(1));
    SyncBool(&MainFrameRmlModel::skillSlot2Selected, "skill_slot_2_selected", g_pSkillList->IsHotKeySlotCurrentSkill(2));
    SyncBool(&MainFrameRmlModel::skillSlot3Selected, "skill_slot_3_selected", g_pSkillList->IsHotKeySlotCurrentSkill(3));
    SyncBool(&MainFrameRmlModel::skillSlot4Selected, "skill_slot_4_selected", g_pSkillList->IsHotKeySlotCurrentSkill(4));

    // Skill-hotkey number labels (see MainFrameRmlModel::skillSlot0Hotkey and
    // CSkillList::GetHotKeySlotNumber()). -1 (empty slot) becomes an empty string.
    SyncText(&MainFrameRmlModel::skillSlot0Hotkey, "skill_slot_0_hotkey", HotkeyText(g_pSkillList->GetHotKeySlotNumber(0)));
    SyncText(&MainFrameRmlModel::skillSlot1Hotkey, "skill_slot_1_hotkey", HotkeyText(g_pSkillList->GetHotKeySlotNumber(1)));
    SyncText(&MainFrameRmlModel::skillSlot2Hotkey, "skill_slot_2_hotkey", HotkeyText(g_pSkillList->GetHotKeySlotNumber(2)));
    SyncText(&MainFrameRmlModel::skillSlot3Hotkey, "skill_slot_3_hotkey", HotkeyText(g_pSkillList->GetHotKeySlotNumber(3)));
    SyncText(&MainFrameRmlModel::skillSlot4Hotkey, "skill_slot_4_hotkey", HotkeyText(g_pSkillList->GetHotKeySlotNumber(4)));

    // Skill icons of the hotkey row and the current-skill slot, and the current skill's hotkey. The
    // decorators are kept strings (CSkillList::GetSkillIconDecorator()): compared, not built.
    SyncText(&MainFrameRmlModel::skillSlot0Icon, "skill_slot_0_icon", g_pSkillList->GetHotKeySlotIconDecorator(0));
    SyncText(&MainFrameRmlModel::skillSlot1Icon, "skill_slot_1_icon", g_pSkillList->GetHotKeySlotIconDecorator(1));
    SyncText(&MainFrameRmlModel::skillSlot2Icon, "skill_slot_2_icon", g_pSkillList->GetHotKeySlotIconDecorator(2));
    SyncText(&MainFrameRmlModel::skillSlot3Icon, "skill_slot_3_icon", g_pSkillList->GetHotKeySlotIconDecorator(3));
    SyncText(&MainFrameRmlModel::skillSlot4Icon, "skill_slot_4_icon", g_pSkillList->GetHotKeySlotIconDecorator(4));
    SyncText(&MainFrameRmlModel::currentSkillIcon, "current_skill_icon", g_pSkillList->GetCurrentSkillIconDecorator());
    SyncText(&MainFrameRmlModel::currentSkillHotkey, "current_skill_hotkey",
             CharacterAttribute->SkillNumber > 0 ? HotkeyText(g_pSkillList->GetSkillHotKeyNumber(Hero->CurrentSkill))
                                                 : Rml::String());

    // Skill list cooldown fractions.
    SyncFloat(&MainFrameRmlModel::skillSlot0Cooldown, "skill_slot_0_cooldown", g_pSkillList->GetHotKeySlotCooldownFraction(0));
    SyncFloat(&MainFrameRmlModel::skillSlot1Cooldown, "skill_slot_1_cooldown", g_pSkillList->GetHotKeySlotCooldownFraction(1));
    SyncFloat(&MainFrameRmlModel::skillSlot2Cooldown, "skill_slot_2_cooldown", g_pSkillList->GetHotKeySlotCooldownFraction(2));
    SyncFloat(&MainFrameRmlModel::skillSlot3Cooldown, "skill_slot_3_cooldown", g_pSkillList->GetHotKeySlotCooldownFraction(3));
    SyncFloat(&MainFrameRmlModel::skillSlot4Cooldown, "skill_slot_4_cooldown", g_pSkillList->GetHotKeySlotCooldownFraction(4));
    SyncFloat(&MainFrameRmlModel::currentSkillCooldown, "current_skill_cooldown",
              g_pSkillList->GetCurrentSkillCooldownFraction());
}

void mu::ui::window::CMainFrameWindow::SyncItemSlots()
{
    // Item hotkey chrome (#item_slots hover border + stack-count). m_ItemHotKey is a member, read
    // directly. The counts stay per frame: a potion stack changes in place, with no signal to wait on.
    auto stackCountText = [](int count) { return count > 0 ? std::to_string(count) : Rml::String(); };
    SyncBool(&MainFrameRmlModel::itemSlot0Hovered, "item_slot_0_hovered", m_ItemHotKey.GetHoveredSlot() == 0);
    SyncBool(&MainFrameRmlModel::itemSlot1Hovered, "item_slot_1_hovered", m_ItemHotKey.GetHoveredSlot() == 1);
    SyncBool(&MainFrameRmlModel::itemSlot2Hovered, "item_slot_2_hovered", m_ItemHotKey.GetHoveredSlot() == 2);
    SyncBool(&MainFrameRmlModel::itemSlot3Hovered, "item_slot_3_hovered", m_ItemHotKey.GetHoveredSlot() == 3);
    SyncText(&MainFrameRmlModel::itemSlot0Count, "item_slot_0_count", stackCountText(m_ItemHotKey.GetSlotItemCount(0)));
    SyncText(&MainFrameRmlModel::itemSlot1Count, "item_slot_1_count", stackCountText(m_ItemHotKey.GetSlotItemCount(1)));
    SyncText(&MainFrameRmlModel::itemSlot2Count, "item_slot_2_count", stackCountText(m_ItemHotKey.GetSlotItemCount(2)));
    SyncText(&MainFrameRmlModel::itemSlot3Count, "item_slot_3_count", stackCountText(m_ItemHotKey.GetSlotItemCount(3)));
    SyncText(&MainFrameRmlModel::itemSlot0Key, "item_slot_0_key", PotionKeyText(0));
    SyncText(&MainFrameRmlModel::itemSlot1Key, "item_slot_1_key", PotionKeyText(1));
    SyncText(&MainFrameRmlModel::itemSlot2Key, "item_slot_2_key", PotionKeyText(2));
    SyncText(&MainFrameRmlModel::itemSlot3Key, "item_slot_3_key", PotionKeyText(3));
}

void mu::ui::window::CMainFrameWindow::SyncSkillList()
{
    // The skill list stays under the phone's HUD too: its touch menu's Skills is this grid.
    auto& model = m_RmlView.GetModel();
    SyncBool(&MainFrameRmlModel::skillGridOpen, "skill_grid_open", g_pSkillList->IsSkillGridOpen());
    SyncBool(&MainFrameRmlModel::skillListUp, "skill_list_up", g_pSkillList->IsSkillListUp());

    // Dynamic-count lists, copied while the grid is open, and only when CSkillList's snapshot changed
    // since the last copy (its generation): a still grid copies nothing (per-frame cost).
    if (!model.skillGridOpen)
        return;
    const unsigned generation = g_pSkillList->GetGridSnapshotGeneration();
    if (m_bGridSynced && generation == m_uSyncedGridGeneration)
        return;
    model.skillGridCells = g_pSkillList->GetGridSnapshot();
    model.petSkillCells = g_pSkillList->GetPetSnapshot();
    m_RmlView.MarkDirty("skill_grid_cells");
    m_RmlView.MarkDirty("pet_skill_cells");
    m_uSyncedGridGeneration = generation;
    m_bGridSynced = true;
}

void mu::ui::window::CMainFrameWindow::SyncSkillTooltip()
{
    // Shared skill tooltip: one hover target queued at a time (QueueTooltip()/OnUnhover()).
    // BuildModelForSlot() is the same content resolution SkillTooltip.cpp's Render() uses; the
    // destination is now UI::RmlBridge::Tooltip's own shared document, not a per-window RML block
    // -- consolidated onto it so this window doesn't carry its own separate tooltip styling/z-order
    // (see docs/rmlui-ui-system/component-catalog.md's "Tooltip" entry).
    if (g_pSkillList->IsTooltipPending())
    {
        UI::Skills::Tooltip::Model tooltipModel;
        if (UI::Skills::Tooltip::BuildModelForSlot(g_pSkillList->GetTooltipSkillIndex(), tooltipModel))
        {
            UI::RmlBridge::Tooltip::Config config;
            config.lines = UI::Skills::Tooltip::ToRmlBridgeLines(tooltipModel);
            // GetTooltipAnchorX/Y() are in HudReferenceTransform()'s units.
            const auto skillTooltipTransform = HudReferenceTransform(m_RmlView.Document());
            // The native box ends below its anchor (G2); measured with the native text renderer
            // in window pixels.
            float bottomBelowAnchorPx = 0.f;
            {
                const UI::Scaling::ScopedWindowPixels pixels(static_cast<int>(WindowWidth),
                                                             static_cast<int>(WindowHeight));
                bottomBelowAnchorPx = UI::Skills::Tooltip::NativeBoxBottomBelowAnchor(tooltipModel);
            }
            config.anchorX = UI::Scaling::PositionX(skillTooltipTransform, g_pSkillList->GetTooltipAnchorX());
            config.anchorY =
                UI::Scaling::PositionY(skillTooltipTransform, g_pSkillList->GetTooltipAnchorY()) + bottomBelowAnchorPx;
            // The old #skill_tooltip's CSS (`transform: translateY(-100%)`) always grew upward,
            // unconditionally -- AboveLeft matches that; Show()'s own clamping now also covers the
            // horizontal/lower-edge cases that CSS-only transform never did.
            config.anchor = UI::RmlBridge::Tooltip::AnchorPoint::AboveLeft;
            // QueueTooltip()'s anchor is native's: the icon's centre, 10 above it; lines centred
            // (RenderTipTextList(..., RT3_SORT_CENTER, ...)).
            config.centerHorizontally = true;
            config.textAlign = UI::RmlBridge::Tooltip::Config::TextAlign::Center;
            config.transform = skillTooltipTransform;
            UI::RmlBridge::Tooltip::Show(config, g_pSkillList);
        }
        else
        {
            UI::RmlBridge::Tooltip::Hide(g_pSkillList);
        }
    }
    else
    {
        UI::RmlBridge::Tooltip::Hide(g_pSkillList);
    }
}

float mu::ui::window::CMainFrameWindow::GetLayerDepth()
{
    return 10.6f;
}

float mu::ui::window::CMainFrameWindow::GetKeyEventOrder()
{
    return 2.9f;
}

void mu::ui::window::CMainFrameWindow::SetItemHotKey(int iHotKey, int iItemType, int iItemLevel)
{
    m_ItemHotKey.SetHotKey(iHotKey, iItemType, iItemLevel);
}

int mu::ui::window::CMainFrameWindow::GetItemHotKey(int iHotKey)
{
    return m_ItemHotKey.GetHotKey(iHotKey);
}

int mu::ui::window::CMainFrameWindow::GetItemHotKeyLevel(int iHotKey)
{
    return m_ItemHotKey.GetHotKeyLevel(iHotKey);
}

int mu::ui::window::CMainFrameWindow::PutItemOnFreeHotKey(int iItemType, int iItemLevel, bool& bAlreadyThere)
{
    return m_ItemHotKey.PutOnFreeHotKey(iItemType, iItemLevel, bAlreadyThere);
}

int mu::ui::window::CMainFrameWindow::GetItemHotKeyCount(int iHotKey)
{
    return m_ItemHotKey.GetSlotItemCount(iHotKey);
}

void mu::ui::window::CMainFrameWindow::UpdateItemHotKey()
{
    m_ItemHotKey.UpdateKeyEvent();
}

void mu::ui::window::CMainFrameWindow::ResetSkillHotKey()
{
    g_pSkillList->Reset();
}

void mu::ui::window::CMainFrameWindow::SetSkillHotKey(int iHotKey, int iSkillType)
{
    g_pSkillList->SetHotKey(iHotKey, iSkillType);
}

int mu::ui::window::CMainFrameWindow::GetSkillHotKey(int iHotKey)
{
    return g_pSkillList->GetHotKey(iHotKey);
}

int mu::ui::window::CMainFrameWindow::GetSkillHotKeyIndex(int iSkillType)
{
    return g_pSkillList->GetSkillIndex(iSkillType);
}

void mu::ui::window::CMainFrameWindow::SetPreExp_Wide(__int64 dwPreExp)
{
    m_loPreExp = dwPreExp;
}

void mu::ui::window::CMainFrameWindow::SetGetExp_Wide(__int64 dwGetExp)
{
    m_loGetExp = dwGetExp;

    if (m_loGetExp > 0)
    {
        m_bExpEffect = true;
        m_dwExpEffectTime = timeGetTime();
    }
}

void mu::ui::window::CMainFrameWindow::SetPreExp(__int64 dwPreExp)
{
    m_dwPreExp = dwPreExp;
}

void mu::ui::window::CMainFrameWindow::SetGetExp(__int64 dwGetExp)
{
    m_dwGetExp = dwGetExp;

    if (m_dwGetExp > 0)
    {
        m_bExpEffect = true;
        m_dwExpEffectTime = timeGetTime();
    }
}


void mu::ui::window::CMainFrameWindow::SetBtnState(int iBtnType, bool bStateDown)
{
    // Sets a bound "open" model boolean; main_frame.rcss selects the panel-open sprite rect when true.
    if (!m_RmlView.Document()) return;

    switch (iBtnType)
    {
#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
    case MAINFRAME_BTN_PARTCHARGE:
        if (m_RmlView.GetModel().cShopOpen != bStateDown)
        {
            m_RmlView.GetModel().cShopOpen = bStateDown;
            m_RmlView.MarkDirty("cshop_open");
        }
        break;
#endif //defined PBG_ADD_INGAMESHOP_UI_MAINFRAME
    case MAINFRAME_BTN_CHAINFO:
        if (m_RmlView.GetModel().chaInfoOpen != bStateDown)
        {
            m_RmlView.GetModel().chaInfoOpen = bStateDown;
            m_RmlView.MarkDirty("chainfo_open");
        }
        break;
    case MAINFRAME_BTN_MYINVEN:
        if (m_RmlView.GetModel().myInvenOpen != bStateDown)
        {
            m_RmlView.GetModel().myInvenOpen = bStateDown;
            m_RmlView.MarkDirty("myinven_open");
        }
        break;
    case MAINFRAME_BTN_FRIEND:
        if (m_RmlView.GetModel().friendOpen != bStateDown)
        {
            m_RmlView.GetModel().friendOpen = bStateDown;
            m_RmlView.MarkDirty("friend_open");
        }
        break;
    case MAINFRAME_BTN_WINDOW:
        if (m_RmlView.GetModel().windowOpen != bStateDown)
        {
            m_RmlView.GetModel().windowOpen = bStateDown;
            m_RmlView.MarkDirty("window_open");
        }
        break;
    }
}

void mu::ui::window::CMainFrameWindow::SyncDocVisibility(bool sceneAllowsShow)
{
    const bool show = IsVisible() && sceneAllowsShow;

    UI::RmlBridge::SyncDocumentVisibility(m_RmlView.Document(), show);
    m_ItemHotKey.SetSlotIconsShown(show);
    UI::RmlBridge::SyncDocumentVisibility(m_RmlView.Document(1), show);
}
