#ifndef _NEWUICHATINPUTBOX_H_
#define _NEWUICHATINPUTBOX_H_

#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "UI/Chat/ChatHistory.h"

#pragma warning(disable : 4786)
#include <array>
#include <string>
#include <vector>

namespace Rml
{
    class ElementDocument;
}

namespace mu::ui::window
{
    class CManager;
    class CChatLogWindow;
    class CSystemLogWindow;

    // Every piece of this window's presentation is RmlUi's now: the bar art, all ten buttons, the
    // tooltip and both text fields. C++ keeps the chat/whisper history, the send logic and the
    // keyboard handling, which runs while one of its fields is focused (TakesTypingFrom()).
    struct ChatInputRmlModel
    {
        Rml::String chatText;
        Rml::String whisperId;

        int inputMsgType = 0;   // INPUT_CHAT_MESSAGE..INPUT_GENS_MESSAGE, as a 0-based index
        bool blockWhisper = false;
        bool showSystem = true;
        bool showChatLog = true;
        bool showFrame = false;
        bool whisperSend = true;

        // The buttons' hints, in the button row's order.
        std::array<Rml::String, 10> buttonHints;
        // A phone's quick phrases over the bar (theme mobile): a tap sends one as Enter sends a line.
        std::array<Rml::String, 5> quickPhrases;
        // Which of them carry a saved macro (cut short in the rcss when long).
        std::array<bool, 5> quickIsMacro{};
    };

    class CChatInputBox : public CObject
    {
    public:
        // It's also the size of the graphics IMAGE_INPUTBOX_BACK.
        enum
        {
            CHATBOX_WIDTH = 281,
            CHATBOX_HEIGHT = 47,
        };

        enum INPUT_MESSAGE_TYPE
        {
            INPUT_NOTHING = -1,
            INPUT_CHAT_MESSAGE,
            INPUT_PARTY_MESSAGE,
            INPUT_GUILD_MESSAGE,
            INPUT_GENS_MESSAGE,
        };

    private:
        typedef std::wstring type_string;
        typedef std::vector<type_string>	type_vec_history;

        const uint64_t ChatCooldownMs = 1000; // 1 Second
        uint64_t  m_lastChatTime = 0;

        CManager* m_pNewUIMng;
        CChatLogWindow* m_pNewUIChatLogWnd;
        CSystemLogWindow* m_pNewUISystemLogWnd;

        // Lines sent from the chat field (whispers too), for Up and Down there.
        UI::Chat::ChatHistory m_chatHistory;
        type_vec_history	m_vecWhsprIDHistory;
        // The player who whispered last: "/r text" and "/reply text" answer them.
        type_string m_lastWhisperSender;

        int m_iCurWhisperIDHistory;

        int m_iInputMsgType;
        bool m_bBlockWhisper;
        bool m_bShowSystemMessages;
        bool m_bShowChatLog;
        bool m_bWhisperSend;
        bool m_bShowMessageElseNormal;

        void Init();

        void SetInputMsgType(int iInputMsgType);
        int GetInputMsgType() const;

        void BuildRmlUi();
        void SyncRmlModel();
        // Focus/value access for the two <input>s, so the key handler below never has to know they
        // are RmlUi elements.
        Rml::Element* GetField(const char* id) const;
        bool IsFieldFocused(const char* id) const;
        void SetFieldText(const char* id, const type_string& text);
        void FocusField(const char* id);

        // Sends the chat field's line as Enter does (to the channel chosen, or the whisper target),
        // clears the field and closes the bar.
        void SendChatLine();

        // A phone's quick chat buttons 1 to 5 carry the saved macros of keys 1 to 5 (MacroText) when set.
        static bool HasMacro(int slot);
        void SendQuickMacro(int slot);

        void BindRmlModel(Rml::DataModelConstructor& c, ChatInputRmlModel& model);
        void OnRmlReloaded();
        // Caps the fields at what a line or a name can carry, so nothing typed is cut off unseen on Enter.
        void ApplyFieldLimits();
        UI::RmlBridge::ThemedView<ChatInputRmlModel> m_RmlView{"chat_input",
            [this](Rml::DataModelConstructor& c, ChatInputRmlModel& model) { BindRmlModel(c, model); },
            {{"Data/Interface/RmlUi/chat_input.rml"}},
            {.afterBuild = [this] { ApplyFieldLimits(); }, .afterReload = [this] { OnRmlReloaded(); }}};
        // Set by OpenningProcess(), consumed once the document is actually visible. CSystem::Show()
        // runs OpenningProcess() BEFORE ShowInterface(), so IsVisible() is still false there and
        // focusing the field at that point lands on a hidden document and is lost. It also has to
        // happen after SyncDocumentVisibility()'s own Show(), which defaults to FocusFlag::Auto and
        // would blur the field again.
        bool m_bFocusPending = false;

    public:
        CChatInputBox();
        virtual ~CChatInputBox();

        bool Create(CManager* pNewUIMng,
            CChatLogWindow* pNewUIChatLogWnd,
            CSystemLogWindow* pNewUISystemLogWnd);
        void Release();



        bool HaveFocus();

        void AddChatHistory(const type_string& strText);
        void RemoveChatHistory(int index);
        void RemoveAllChatHIstory();

        void AddWhsprIDHistory(const type_string& strWhsprID);
        void RemoveWhsprIDHistory(int index);
        void RemoveAllWhsprIDHIstory();
        void SetLastWhisperSender(const type_string& sender);
        // "/r text" or "/reply text" (typed, from the command window or a macro): whispers the text to the
        // last player who whispered. False when the line is not the reply command. The caller keeps the
        // line in the chat history if it wants it there.
        bool TrySendReply(const wchar_t* chatText);

        bool IsBlockWhisper();
        void SetBlockWhisper(bool bBlockWhisper);

        bool UpdateMouseEvent();
        bool UpdateKeyEvent();
        bool Update();
        // Enter, Escape and the history keys while the player types a line.
        bool TakesTypingFrom(const Rml::ElementDocument* document) const override
        {
            return document == m_RmlView.Document();
        }
        bool Render();

        float GetLayerDepth();
        float GetKeyEventOrder();

        void OpenningProcess();
        void ClosingProcess();

        void SetWhsprID(const wchar_t* strWhsprID);
        // Puts the name into the whisper field and the caret into the message field (a phone's tap on a chat line).
        void StartWhisperTo(const wchar_t* strWhsprID);

    protected:
        void GetChatText(type_string& strText);
        void GetWhsprID(type_string& strWhsprID);

        void UpdateWhisperTargetFromRightClick();
        static void EchoSentWhisper(const wchar_t* whisperId, const wchar_t* text);
    };
}

#endif // _NEWUICHATINPUTBOX_H_