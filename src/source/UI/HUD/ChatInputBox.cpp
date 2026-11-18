#include "stdafx.h"
#include "UI/Chat/ChatInput.h"
#include "UI/Chat/ChatMessages.h"
#include "UI/Chat/ChatPhraseTexts.h"
#include "UI/Chat/PartyInviteCommand.h"
#include "UI/HUD/ChatInputBox.h"
#include "Core/Input/SyntheticInput.h"
#include "I18N/All.h"

#include "Audio/DSPlaySound.h"
#include "UI/HUD/ChatLogWindow.h"
#include "UI/Mobile/InterfaceMode.h"
#include "UI/Social/SocialWindowBase.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Core/WindowGeometry.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlWorkspaceParticipant.h"
#include "UI/Placement/WindowPlacement.h"
#include "UI/Scaling/UITransform.h"
#include "Engine/Object/ZzzOpenData.h"
#include "World/MapInfra/MapManager.h"
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Items/ItemCategories.h"

#ifdef _EDITOR
#include "imgui.h"
#include "../MuEditor/Core/MuEditorCore.h"
#endif

// RmlUi migration -- see ChatInputRmlModel (ChatInputBox.h).
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "Core/Utilities/StringUtils.h"
#include "Core/Input/ImeInput.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

using namespace SEASON3B;
using namespace mu::ui::window;

mu::ui::window::CChatInputBox::CChatInputBox()
{
    Init();
}

mu::ui::window::CChatInputBox::~CChatInputBox()
{
    Release();
}

void mu::ui::window::CChatInputBox::Init()
{
    m_pNewUIMng = nullptr;
    m_pNewUIChatLogWnd = nullptr;
    m_pNewUISystemLogWnd = nullptr;
    m_iCurWhisperIDHistory = 0;

    m_iInputMsgType = INPUT_CHAT_MESSAGE;
    m_bBlockWhisper = false;
    m_bShowSystemMessages = true;
    m_bShowChatLog = true;
    m_bWhisperSend = true;

    m_bShowMessageElseNormal = false;
}

bool mu::ui::window::CChatInputBox::Create(
    CManager* pNewUIMng,
    CChatLogWindow* pNewUIChatLogWnd,
    CSystemLogWindow* pNewUISystemLogWnd)
{
    Release();

    if (nullptr == pNewUIChatLogWnd || nullptr == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_CHATINPUTBOX, this);

    m_pNewUIChatLogWnd = pNewUIChatLogWnd;
    m_pNewUISystemLogWnd = pNewUISystemLogWnd;

    // A theme slot moves the box. The slot stays while the box is hidden, so the log above it does
    // not drop when typing ends.
    UI::RmlBridge::WorkspaceDocumentOptions options;
    options.placedWhileHidden = true;
    options.measure = [] { return UI::Placement::PlacementParticipant::Size{CHATBOX_WIDTH, CHATBOX_HEIGHT}; };
    UI::RmlBridge::RegisterWorkspaceDocument("chat_input", [this] { return m_RmlView.Document(); }, "panel", std::move(options));

    // Both text fields, their tab pairing, their colours/limits and every button's hit box now
    // come from chat_input.rml/.rcss -- nothing to construct here.
    SetInputMsgType(INPUT_CHAT_MESSAGE);

    Show(false);

    return true;
}

void mu::ui::window::CChatInputBox::Release()
{
    UI::Placement::UnregisterParticipant("chat_input");

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
    }

    Init();

    m_RmlView.Release();
}

void mu::ui::window::CChatInputBox::SetInputMsgType(int iInputMsgType)
{
    m_iInputMsgType = iInputMsgType;
}

int mu::ui::window::CChatInputBox::GetInputMsgType() const
{
    return m_iInputMsgType;
}

bool mu::ui::window::CChatInputBox::HaveFocus()
{
    return IsFieldFocused("chat_field") || IsFieldFocused("whisper_field");
}

void mu::ui::window::CChatInputBox::AddChatHistory(const type_string& strText)
{
    m_chatHistory.Add(strText);
}

void mu::ui::window::CChatInputBox::RemoveChatHistory(int index)
{
    if (index >= 0)
        m_chatHistory.Remove(static_cast<std::size_t>(index));
}

void mu::ui::window::CChatInputBox::RemoveAllChatHIstory()
{
    m_chatHistory.Clear();
}

void mu::ui::window::CChatInputBox::AddWhsprIDHistory(const type_string& strWhsprID)
{
    auto vi = std::find(m_vecWhsprIDHistory.begin(), m_vecWhsprIDHistory.end(), strWhsprID);
    if (vi != m_vecWhsprIDHistory.end())
        m_vecWhsprIDHistory.erase(vi);
    else if (m_vecWhsprIDHistory.size() > 5)
        m_vecWhsprIDHistory.erase(m_vecWhsprIDHistory.begin());
    m_vecWhsprIDHistory.push_back(strWhsprID);
}

void mu::ui::window::CChatInputBox::RemoveWhsprIDHistory(int index)
{
    if (index >= 0 && index < (int)m_vecWhsprIDHistory.size())
    {
        m_vecWhsprIDHistory.erase(m_vecWhsprIDHistory.begin() + index);
    }
}

void mu::ui::window::CChatInputBox::RemoveAllWhsprIDHIstory()
{
    m_vecWhsprIDHistory.clear();
}

void mu::ui::window::CChatInputBox::SetLastWhisperSender(const type_string& sender)
{
    m_lastWhisperSender = sender;
}

bool mu::ui::window::CChatInputBox::IsBlockWhisper()
{
    return m_bBlockWhisper;
}

void mu::ui::window::CChatInputBox::SetBlockWhisper(bool bBlockWhisper)
{
    m_bBlockWhisper = bBlockWhisper;
}

bool mu::ui::window::CChatInputBox::UpdateMouseEvent()
{
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_CHATINPUTBOX) == false)
    {
        return true;
    }

    // The buttons, the fields and the bar itself are RmlUi's (the bar takes the pointer, so a click
    // on it does not fall through to the world). What is left is retargeting a whisper from a
    // right-click elsewhere on screen.
    UpdateWhisperTargetFromRightClick();

    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

void mu::ui::window::CChatInputBox::SendChatLine()
{
    wchar_t	szChatText[MAX_CHAT_SIZE + 1] = { '\0' };
    wchar_t	szWhisperID[MAX_USERNAME_SIZE + 1] = { '\0' };

    {
        type_string strChat, strWhisper;
        GetChatText(strChat);
        GetWhsprID(strWhisper);
        wcsncpy(szChatText, strChat.c_str(), MAX_CHAT_SIZE);
        szChatText[MAX_CHAT_SIZE] = L'\0';
        wcsncpy(szWhisperID, strWhisper.c_str(), MAX_USERNAME_SIZE);
        szWhisperID[MAX_USERNAME_SIZE] = L'\0';
    }

    //for (int i = 0; i < MAX_CHAT_SIZE; i++)
    //    szReceivedChat[i] = g_pMultiLanguage->ConvertFulltoHalfWidthChar(szReceivedChat[i]);

    std::wstring wstrText = L"";

    if (szChatText[0] != 0x002F)
    {
        switch (m_iInputMsgType) {
        case INPUT_PARTY_MESSAGE:
            wstrText = L"~";
            break;
        case INPUT_GUILD_MESSAGE:
            wstrText = L"@";
            break;
        case INPUT_GENS_MESSAGE:
            wstrText = L"$";
            break;
        default:
            break;
        }
    }
    wstrText.append(szChatText);

    // "/r text" goes back to the last player who whispered, before any other command.
    if (wcslen(szChatText) != 0 && TrySendReply(szChatText))
    {
        AddChatHistory(szChatText);
    }
    else if (wcslen(szChatText) != 0)
    {
        // Typed commands (/trade, /party, ...) go to the history too, so Up repeats them.
        const std::wstring typedLine = szChatText;
        if (CheckCommand(szChatText))
        {
            AddChatHistory(typedLine);
        }
        else
        {
            {
                //if (CheckAbuseFilter(szChatText))
                //{
                //    wstrText = I18N::Game::PwnedByTheFilter;
                //}

                if (m_bWhisperSend && wcslen(szChatText) && wcslen(szWhisperID) > 0)
                {
                    // A whisper goes out as typed: the party/guild/gens prefix belongs to public chat only.
                    SocketClient->ToGameServer()->SendWhisperMessage(MU_C16(szWhisperID), MU_C16(szChatText));
                    EchoSentWhisper(szWhisperID, szChatText);
                    AddWhsprIDHistory(szWhisperID);
                    AddChatHistory(szChatText);
                }
                else if (wcsncmp(szChatText, I18N::Game::Warp, wcslen(I18N::Game::Warp)) == 0)
                {
                    wchar_t* pszMapName = szChatText + wcslen(I18N::Game::Warp) + 1;
                    int iMapIndex = g_pMoveCommandWindow->GetMapIndexFromMovereq(pszMapName);

                    if (g_pMoveCommandWindow->IsTheMapInDifferentServer(gMapManager.WorldActive, iMapIndex))
                    {
                        SaveOptions();
                    }

                    SocketClient->ToGameServer()->SendWarpCommandRequest(g_pMoveCommandWindow->GetMoveCommandKey(), iMapIndex);
                    AddChatHistory(szChatText);
                }
                else
                {
                    if (Hero->SafeZone || !GameLogic::Items::IsRideableMountModel(Hero->Helper.Type))
                    {
                        UI::Chat::CheckChatText(szChatText);
                    }

                    SocketClient->ToGameServer()->SendPublicChatMessage(MU_C16(Hero->ID), MU_C16(wstrText.c_str()));
                    AddChatHistory(wstrText);
                }
            }
        }
    }
    SetFieldText("chat_field", L"");
    m_chatHistory.ResetBrowsing();
    m_iCurWhisperIDHistory = 0;

    SaveIMEStatus();

    g_pNewUISystem->Hide(mu::ui::window::INTERFACE_CHATINPUTBOX);
}

bool mu::ui::window::CChatInputBox::TrySendReply(const wchar_t* chatText)
{
    const auto replyText = UI::Chat::ReplyCommandArgument(chatText);
    if (!replyText)
    {
        return false;
    }

    if (m_lastWhisperSender.empty())
    {
        UI::Chat::PostSystem(I18N::Game::WhisperReplyNobody, mu::ui::window::TYPE_SYSTEM_MESSAGE);
        return true;
    }

    if (replyText->empty())
    {
        return true;
    }

    // Copied: the history below may hold the same name, and the whisper text must end in a zero.
    const type_string sender = m_lastWhisperSender;
    const type_string text(*replyText);
    SocketClient->ToGameServer()->SendWhisperMessage(MU_C16(sender.c_str()), MU_C16(text.c_str()));
    EchoSentWhisper(sender.c_str(), text.c_str());
    AddWhsprIDHistory(sender);
    return true;
}

bool mu::ui::window::CChatInputBox::UpdateKeyEvent()
{
    if (mu::ui::window::IsPress(VK_F2))
    {
        m_bShowMessageElseNormal = !m_bShowMessageElseNormal;

        if (m_bShowMessageElseNormal)
        {
            m_pNewUIChatLogWnd->ChangeMessage(mu::ui::window::TYPE_WHISPER_MESSAGE);
        }
        else
        {
            m_pNewUIChatLogWnd->ChangeMessage(mu::ui::window::TYPE_ALL_MESSAGE);
        }

        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    if (mu::ui::window::IsPress(VK_F3))
    {
        if (m_bWhisperSend == false)
        {
            m_bWhisperSend = true;
        }
        else
        {
            m_bWhisperSend = false;
            // RmlUi disables the recipient field through its binding. Keep typing in the
            // message field when whispering is switched off.
            if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_CHATINPUTBOX))
            {
                FocusField("chat_field");
            }
        }

        return false;
    }

    if (m_pNewUIChatLogWnd->IsShowFrame())
    {
        if (mu::ui::window::IsPress(VK_F4))
        {
            m_pNewUIChatLogWnd->SetSizeAuto();
            m_pNewUIChatLogWnd->UpdateWndSize();
            m_pNewUIChatLogWnd->UpdateScrollPos();
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }

    if (mu::ui::window::IsPress(VK_F5))
    {
        if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_CHATINPUTBOX))
        {
            if (m_pNewUIChatLogWnd->IsShowFrame())
            {
                m_pNewUIChatLogWnd->HideFrame();
            }
            else
            {
                m_pNewUIChatLogWnd->ShowFrame();
            }
        }

        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    if (m_pNewUIChatLogWnd->IsShowFrame())
    {
        if (IsPress(VK_PRIOR))
        {
            m_pNewUIChatLogWnd->Scrolling(m_pNewUIChatLogWnd->GetCurrentRenderEndLine() - m_pNewUIChatLogWnd->GetNumberOfShowingLines());
            return false;
        }
        if (IsPress(VK_NEXT))
        {
            m_pNewUIChatLogWnd->Scrolling(m_pNewUIChatLogWnd->GetCurrentRenderEndLine() + m_pNewUIChatLogWnd->GetNumberOfShowingLines());
            return false;
        }
    }

    if (false == IsVisible() && mu::ui::window::IsPress(VK_RETURN))
    {
#ifdef _EDITOR
        // Don't open chat if editor has keyboard focus. An Enter the control socket injects never
        // reached the editor, whatever the real pointer hovers.
        if (g_MuEditorCore.IsEnabled() && !Core::Input::Synthetic::IsInjecting())
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.WantCaptureKeyboard || io.WantCaptureMouse)
            {
                return false;
            }
        }
#endif // _EDITOR

        if (gMapManager.InChaosCastle() == true && g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_CHAOSCASTLE_TIME) == false)
        {
            g_pNewUISystem->Show(mu::ui::window::INTERFACE_CHATINPUTBOX);
        }
        else if (gMapManager.InChaosCastle() == false)
        {
            g_pNewUISystem->Show(mu::ui::window::INTERFACE_CHATINPUTBOX);
        }

        RestoreIMEStatus();
        return false;
    }

    // The chat line open but let go of (a finger's tap on the world, a click on another window):
    // Enter, and the phone's Chat button with it, types in it again; not from another text field.
    if (IsVisible() && !HaveFocus() && !RmlUiRuntime::Instance().IsTextInputActive()
        && mu::ui::window::IsPress(VK_RETURN))
    {
        FocusField("chat_field");
        RestoreIMEStatus();
        return false;
    }

    const uint64_t currentTickCount = GetTickCount64();
    if (IsVisible() && HaveFocus() && mu::ui::window::IsPress(VK_RETURN)
        && m_lastChatTime < currentTickCount - ChatCooldownMs)
    {
        m_lastChatTime = currentTickCount;
        SendChatLine();
        return false;
    }
    if (IsVisible() && IsFieldFocused("chat_field"))
    {
        if (mu::ui::window::IsPress(VK_UP))
        {
            if (RecallOlder("chat_field"))
                return false;
        }
        else if (mu::ui::window::IsPress(VK_DOWN))
        {
            if (const auto line = m_chatHistory.Newer())
            {
                SetFieldText("chat_field", *line);
                return false;
            }
        }
    }

    if (IsVisible() && IsFieldFocused("whisper_field") && m_bWhisperSend)
    {
        if (mu::ui::window::IsPress(VK_UP) && RecallOlder("whisper_field"))
        {
            return false;
        }
        else if (mu::ui::window::IsPress(VK_DOWN) && false == m_vecWhsprIDHistory.empty())
        {
            m_iCurWhisperIDHistory++;

            if (m_iCurWhisperIDHistory >= (int)m_vecWhsprIDHistory.size())
                m_iCurWhisperIDHistory = 0;

            SetFieldText("whisper_field", m_vecWhsprIDHistory[m_iCurWhisperIDHistory]);

            return false;
        }
    }
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_CHATINPUTBOX) == true)
    {
        if (mu::ui::window::IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_CHATINPUTBOX);

            PlayBuffer(SOUND_CLICK01);

            return false;
        }
    }

    return true;
}

bool mu::ui::window::CChatInputBox::Update()
{
    BuildRmlUi();
    SyncRmlModel();
    return true;
}

bool mu::ui::window::CChatInputBox::Render()
{
    // Nothing native left: the bar art, the buttons, the tooltip and both fields are all RmlUi.
    // Kept because CObject requires the override.
    return true;
}

void mu::ui::window::CChatInputBox::BindRmlModel(Rml::DataModelConstructor& c, ChatInputRmlModel& model)
{
    c.Bind("chat_text", &model.chatText);
    c.Bind("whisper_id", &model.whisperId);
    c.Bind("input_msg_type", &model.inputMsgType);
    c.Bind("block_whisper", &model.blockWhisper);
    c.Bind("show_system", &model.showSystem);
    c.Bind("show_chat_log", &model.showChatLog);
    c.Bind("show_frame", &model.showFrame);
    c.Bind("whisper_send", &model.whisperSend);
    for (size_t i = 0; i < model.buttonHints.size(); ++i)
        c.Bind("hint_" + std::to_string(i), &model.buttonHints[i]);
    for (size_t i = 0; i < model.quickPhrases.size(); ++i)
        c.Bind("quick_" + std::to_string(i), &model.quickPhrases[i]);
    for (size_t i = 0; i < model.quickIsMacro.size(); ++i)
        c.Bind("quick_macro_" + std::to_string(i), &model.quickIsMacro[i]);
    for (size_t i = 0; i < model.chatPhrases.size(); ++i)
        c.Bind("phrase_" + std::to_string(i), &model.chatPhrases[i]);

    // A phone's quick button: its saved macro, sent as Alt+N sends it.
    c.BindEventCallback("chat_quick",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
        {
            if (args.empty()) return;
            const int slot = args[0].Get<int>(-1);
            const ChatInputRmlModel& model = m_RmlView.GetModel();
            if (slot < 0 || slot >= static_cast<int>(model.quickPhrases.size()) || !IsVisible() || !HasMacro(slot))
                return;
            SendQuickMacro(slot);
        });

    // A phone's quick chat phrase chip (1 and up).
    c.BindEventCallback("chat_phrase",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
        {
            if (args.empty()) return;
            SendPhrase(args[0].Get<int>(0));
        });

    // A phone's history button (a phone keyboard has no arrows): Up in the field that had the focus, which gets it back
    // so its keyboard stays.
    c.BindEventCallback("chat_history_up",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (!IsVisible())
                return;
            const char* field = RecallFieldId();
            RecallOlder(field);
            FocusField(field);
        });

    c.BindEventCallback("chat_set_type",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
        {
            if (args.empty()) return;
            SetInputMsgType(INPUT_CHAT_MESSAGE + args[0].Get<int>(0));
            PlayBuffer(SOUND_CLICK01);
        });

    c.BindEventCallback("chat_toggle_whisper_block",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            m_bBlockWhisper = !m_bBlockWhisper;
            PlayBuffer(SOUND_CLICK01);
        });

    c.BindEventCallback("chat_toggle_system",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            m_bShowSystemMessages = !m_bShowSystemMessages;
            if (m_bShowSystemMessages)
                m_pNewUISystemLogWnd->ShowMessages();
            else
                m_pNewUISystemLogWnd->HideMessages();
            PlayBuffer(SOUND_CLICK01);
        });

    c.BindEventCallback("chat_toggle_chatlog",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            m_bShowChatLog = !m_bShowChatLog;
            if (m_bShowChatLog)
                m_pNewUIChatLogWnd->ShowChatLog();
            else
                m_pNewUIChatLogWnd->HideChatLog();
            PlayBuffer(SOUND_CLICK01);
        });

    c.BindEventCallback("chat_toggle_frame",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            if (m_pNewUIChatLogWnd->IsShowFrame())
                m_pNewUIChatLogWnd->HideFrame();
            else
                m_pNewUIChatLogWnd->ShowFrame();
            PlayBuffer(SOUND_CLICK01);
        });

    c.BindEventCallback("chat_size_step",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            m_pNewUIChatLogWnd->SetSizeAuto();
            m_pNewUIChatLogWnd->UpdateWndSize();
            m_pNewUIChatLogWnd->UpdateScrollPos();
            PlayBuffer(SOUND_CLICK01);
        });

    c.BindEventCallback("chat_alpha_step",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
        {
            m_pNewUIChatLogWnd->SetBackAlphaAuto();
            PlayBuffer(SOUND_CLICK01);
        });
}

void mu::ui::window::CChatInputBox::OnRmlReloaded()
{
    UI::Placement::Invalidate();
}

void mu::ui::window::CChatInputBox::BuildRmlUi()
{
    m_RmlView.Ensure();
}

// The party, guild and gens prefix (~ @ $) is added on top when the line is sent, as before.
void mu::ui::window::CChatInputBox::ApplyFieldLimits()
{
    if (Rml::Element* field = GetField("chat_field"))
        field->SetAttribute("maxlength", static_cast<int>(MAX_CHAT_SIZE));
    if (Rml::Element* field = GetField("whisper_field"))
        field->SetAttribute("maxlength", static_cast<int>(MAX_USERNAME_SIZE));
}

void mu::ui::window::CChatInputBox::SyncRmlModel()
{
    if (!m_RmlView.Document()) return;

    UI::RmlBridge::SyncDocumentVisibility(m_RmlView.Document(), IsVisible());

    // Deliberately after the line above: the field can only take focus once its document is
    // showing, and SyncDocumentVisibility()'s Show() would blur it if the order were reversed.
    if (m_bFocusPending && m_RmlView.Document()->IsVisible())
    {
        m_bFocusPending = false;
        FocusField("chat_field");
    }
    if (IsFieldFocused("chat_field"))
        m_lastFocusedField = "chat_field";
    else if (IsFieldFocused("whisper_field"))
        m_lastFocusedField = "whisper_field";

    ChatInputRmlModel& model = m_RmlView.GetModel();

    auto syncBool = [&](bool ChatInputRmlModel::* field, const char* name, bool value)
    {
        if (model.*field != value) { model.*field = value; m_RmlView.MarkDirty(name); }
    };

    syncBool(&ChatInputRmlModel::blockWhisper, "block_whisper", m_bBlockWhisper);
    syncBool(&ChatInputRmlModel::showSystem, "show_system", m_bShowSystemMessages);
    syncBool(&ChatInputRmlModel::showChatLog, "show_chat_log", m_bShowChatLog);
    syncBool(&ChatInputRmlModel::whisperSend, "whisper_send", m_bWhisperSend);
    syncBool(&ChatInputRmlModel::showFrame, "show_frame",
             m_pNewUIChatLogWnd && m_pNewUIChatLogWnd->IsShowFrame());

    const int typeIndex = GetInputMsgType() - INPUT_CHAT_MESSAGE;
    if (model.inputMsgType != typeIndex)
    {
        model.inputMsgType = typeIndex;
        m_RmlView.MarkDirty("input_msg_type");
    }

    SyncQuickButtons(model);

    // Native's string table for the buttons' hints (RenderTooltip()).
    static constexpr int kHintText[10] = {1681, 1682, 1683, 3321, 1684, 1685, 750, 1686, 751, 752};
    for (size_t i = 0; i < model.buttonHints.size(); ++i)
    {
        Rml::String hint = StringUtils::WideToNarrow(I18N::Game::Lookup(kHintText[i]));
        if (model.buttonHints[i] != hint)
        {
            model.buttonHints[i] = std::move(hint);
            m_RmlView.MarkDirty("hint_" + std::to_string(i));
        }
    }
}

void mu::ui::window::CChatInputBox::SyncQuickButtons(ChatInputRmlModel& model)
{
    for (size_t i = 0; i < model.quickPhrases.size(); ++i)
    {
        // A saved macro of keys 1 to 5 (Alt+1..5 on a PC) takes the button of its number.
        const bool isMacro = HasMacro(static_cast<int>(i));
        if (model.quickIsMacro[i] != isMacro)
        {
            model.quickIsMacro[i] = isMacro;
            m_RmlView.MarkDirty("quick_macro_" + std::to_string(i));
        }
        Rml::String macro = isMacro ? StringUtils::WideToNarrow(MacroText[i]) : Rml::String();
        if (model.quickPhrases[i] != macro)
        {
            model.quickPhrases[i] = std::move(macro);
            m_RmlView.MarkDirty("quick_" + std::to_string(i));
        }
    }
    for (size_t i = 0; i < model.chatPhrases.size(); ++i)
    {
        const wchar_t* own = UI::Chat::PhraseTexts::OwnText(static_cast<int>(i) + 1);
        Rml::String phrase = own != nullptr ? StringUtils::WideToNarrow(own) : Rml::String();
        if (model.chatPhrases[i] != phrase)
        {
            model.chatPhrases[i] = std::move(phrase);
            m_RmlView.MarkDirty("phrase_" + std::to_string(i));
        }
    }
}

const char* mu::ui::window::CChatInputBox::RecallFieldId() const
{
    return std::strcmp(m_lastFocusedField, "whisper_field") == 0 && m_bWhisperSend ? "whisper_field" : "chat_field";
}

Rml::Element* mu::ui::window::CChatInputBox::RecallFieldAt(float x, float y) const
{
    Rml::ElementDocument* document = m_RmlView.Document();
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (document == nullptr || context == nullptr || !document->IsVisible() || !IsVisible())
        return nullptr;
    for (Rml::Element* element = context->GetElementAtPoint({x, y}); element != nullptr;
         element = element->GetParentNode())
    {
        if (element->GetId() == "quick_history")
            return element->GetOwnerDocument() == document ? GetField(RecallFieldId()) : nullptr;
    }
    return nullptr;
}

bool mu::ui::window::CChatInputBox::RecallOlder(const char* id)
{
    if (std::strcmp(id, "whisper_field") == 0)
    {
        if (m_vecWhsprIDHistory.empty())
            return false;
        m_iCurWhisperIDHistory--;
        if (m_iCurWhisperIDHistory < 0)
            m_iCurWhisperIDHistory = m_vecWhsprIDHistory.size() - 1;
        SetFieldText("whisper_field", m_vecWhsprIDHistory[m_iCurWhisperIDHistory]);
        return true;
    }
    type_string draft;
    GetChatText(draft);
    const auto line = m_chatHistory.Older(draft);
    if (!line)
        return false;
    SetFieldText("chat_field", *line);
    return true;
}

float mu::ui::window::CChatInputBox::QuickRowLiftPx() const
{
    Rml::ElementDocument* document = m_RmlView.Document();
    if (document == nullptr || !document->IsVisible() || !IsVisible())
        return 0.f;
    if (!m_quickRow || !m_quickPanel || m_quickRow->GetOwnerDocument() != document)
    {
        Rml::Element* found = document->GetElementById("quick_row");
        m_quickRow = found != nullptr ? found->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>();
        found = document->GetElementById("panel");
        m_quickPanel = found != nullptr ? found->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>();
    }
    Rml::Element* row = m_quickRow.get();
    Rml::Element* panel = m_quickPanel.get();
    if (row == nullptr || panel == nullptr || !row->IsVisible() || row->GetNumChildren() == 0)
        return 0.f;
    // In the panel's layout px: from the row's top down to the bar's top edge.
    const float reach = panel->GetAbsoluteOffset(Rml::BoxArea::Border).y - row->GetAbsoluteOffset(Rml::BoxArea::Border).y;
    if (reach <= 0.f)
        return 0.f;
    // The panel is drawn at its slot's scale: two window points projected onto it give window px per layout px.
    Rml::Vector2f top{0.f, 0.f};
    Rml::Vector2f lower{0.f, 100.f};
    if (!panel->Project(top) || !panel->Project(lower) || lower.y <= top.y)
        return 0.f;
    return reach * 100.f / (lower.y - top.y);
}

Rml::Element* mu::ui::window::CChatInputBox::GetField(const char* id) const
{
    return m_RmlView.Document() ? m_RmlView.Document()->GetElementById(id) : nullptr;
}

bool mu::ui::window::CChatInputBox::IsFieldFocused(const char* id) const
{
    Rml::Element* field = GetField(id);
    return field != nullptr && field->IsPseudoClassSet("focus");
}

void mu::ui::window::CChatInputBox::SetFieldText(const char* id, const type_string& text)
{
    ChatInputRmlModel& model = m_RmlView.GetModel();
    const Rml::String narrow = StringUtils::WideToNarrow(text.c_str());

    if (strcmp(id, "chat_field") == 0)
    {
        if (model.chatText == narrow) return;
        model.chatText = narrow;
        m_RmlView.MarkDirty("chat_text");
    }
    else
    {
        if (model.whisperId == narrow) return;
        model.whisperId = narrow;
        m_RmlView.MarkDirty("whisper_id");
    }
}

void mu::ui::window::CChatInputBox::FocusField(const char* id)
{
    if (Rml::Element* field = GetField(id))
        field->Focus();
}

float mu::ui::window::CChatInputBox::GetLayerDepth()
{
    return 6.2f;
}

float mu::ui::window::CChatInputBox::GetKeyEventOrder()
{
    return 9.0f;
}

void mu::ui::window::CChatInputBox::OpenningProcess()
{
    // Only clears the field and arms the focus latch -- see m_bFocusPending. The document is not
    // visible yet at this point, so focusing here would be silently dropped.
    BuildRmlUi();
    SetFieldText("chat_field", L"");
    m_chatHistory.ResetBrowsing();
    m_bFocusPending = true;
    // A phone opens the chat on its history: the frame is what lets a finger scroll the lines and tap one to whisper
    // (CChatLogWindow::OnLinesTapped). ClosingProcess() hides it again, as on a PC.
    if (UI::Mobile::ReplacesPcHud() && m_pNewUIChatLogWnd != nullptr)
        m_pNewUIChatLogWnd->ShowFrame();
}

void mu::ui::window::CChatInputBox::ClosingProcess()
{
    m_pNewUIChatLogWnd->HideFrame();

    m_bFocusPending = false;
    if (Rml::Element* field = GetField("chat_field")) field->Blur();
    if (Rml::Element* field = GetField("whisper_field")) field->Blur();

    SetFocus(g_hWnd);
}

void mu::ui::window::CChatInputBox::GetChatText(type_string& strText)
{
    strText = StringUtils::NarrowToWide(m_RmlView.GetModel().chatText);
}

void mu::ui::window::CChatInputBox::GetWhsprID(type_string& strWhsprID)
{
    strWhsprID = StringUtils::NarrowToWide(m_RmlView.GetModel().whisperId);
}

void mu::ui::window::CChatInputBox::SetWhsprID(const wchar_t* strWhsprID)
{
    SetFieldText("whisper_field", strWhsprID ? strWhsprID : L"");
}

bool mu::ui::window::CChatInputBox::HasMacro(int slot)
{
    return slot >= 0 && slot < static_cast<int>(std::size(MacroText)) && MacroText[slot][0] != L'\0';
}

// A macro on a quick button goes as Alt+N sends it (its pause, its checks); the bar then closes as after a
// quick phrase, unless a line is half typed.
void mu::ui::window::CChatInputBox::SendQuickMacro(int slot)
{
    if (!SendMacroSlot(slot))
        return;
    PlayBuffer(SOUND_CLICK01);
    if (m_RmlView.GetModel().chatText.empty())
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_CHATINPUTBOX);
}

bool mu::ui::window::CChatInputBox::SendPhrase(int id)
{
    const type_string line = UI::Chat::Phrases::Compose(id);
    if (line.empty() || !IsVisible())
        return false;
    if (!TakeQuickPhrasePause())
    {
        // Too soon after the last phrase or macro, or in Chaos Castle: the refusal sound, as for a failed action.
        PlayBuffer(SOUND_ERROR01);
        return false;
    }
    PlayBuffer(SOUND_CLICK01);
    SendLineKeepingDraft(line);
    return true;
}

void mu::ui::window::CChatInputBox::SendLineKeepingDraft(const type_string& line)
{
    const Rml::String draft = m_RmlView.GetModel().chatText;
    SetFieldText("chat_field", line);
    SendChatLine();
    if (draft.empty())
        return;
    g_pNewUISystem->Show(mu::ui::window::INTERFACE_CHATINPUTBOX);
    SetFieldText("chat_field", StringUtils::NarrowToWide(draft));
}

void mu::ui::window::CChatInputBox::StartWhisperTo(const wchar_t* strWhsprID)
{
    SetWhsprID(strWhsprID);
    FocusField("chat_field");
}

// The own whisper shows as "[to Name] text" with no sender ID, so a right-click on the line does not
// retarget the whisper to the player's own name.
void mu::ui::window::CChatInputBox::EchoSentWhisper(const wchar_t* whisperId, const wchar_t* text)
{
    // A quick chat phrase shows in the player's own language, as the one it goes to reads it.
    wchar_t shown[MAX_CHAT_SIZE + 1] = { L'\0' };
    wcsncpy(shown, text, MAX_CHAT_SIZE);
    shown[MAX_CHAT_SIZE] = L'\0';
    UI::Chat::PhraseTexts::ShowInOwnLanguage(shown, std::size(shown));

    wchar_t echo[MAX_USERNAME_SIZE + MAX_CHAT_SIZE + 32] = { L'\0' };
    mu_swprintf_s(echo, I18N::Game::WhisperEchoTo, whisperId, shown);
    g_pChatListBox->AddText(L"", echo, mu::ui::window::TYPE_WHISPER_MESSAGE);
}

void mu::ui::window::CChatInputBox::UpdateWhisperTargetFromRightClick()
{
    if (SelectedCharacter < 0 || !mu::ui::window::IsRelease(VK_RBUTTON))
    {
        return;
    }

    auto const character = &CharactersClient[SelectedCharacter];
    if (character->Object.Kind != KIND_PLAYER)
    {
        return;
    }

    if (gMapManager.InChaosCastle())
    {
        return;
    }

    auto const blockedByGensRivalry = ::IsStrifeMap(gMapManager.WorldActive)
        && Hero->m_byGensInfluence != character->m_byGensInfluence;
    if (blockedByGensRivalry)
    {
        return;
    }

    SetWhsprID(character->ID);
}
