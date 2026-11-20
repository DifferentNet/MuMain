#ifndef _NEWUICHATINPUTBOX_H_
#define _NEWUICHATINPUTBOX_H_

#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "UI/Chat/ChatHistory.h"
#include "UI/Chat/ChatPhrases.h"
#include "UI/Chat/ChatSendGate.h"
#include "UI/Chat/ItemLinkDraft.h"

#pragma warning(disable : 4786)
#include <array>
#include <cstddef>
#include <optional>
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
        // A phone's quick buttons over the bar (theme mobile): the saved macros of keys 1 to 5, each shown
        // only when set; a tap sends it as Alt+N does.
        std::array<Rml::String, 5> quickPhrases;
        std::array<bool, 5> quickIsMacro{};
        // Then the quick chat phrases (UI/Chat/ChatPhrases) in the player's language: a tap sends one at once,
        // each reader sees it in theirs.
        std::array<Rml::String, UI::Chat::Phrases::Count> chatPhrases;
        // The phrase chips are dimmed while the pause after a phrase or macro runs (TakeQuickPhrasePause).
        bool quickPaused = false;
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

        static constexpr uint64_t ChatCooldownMs = 1000; // 1 Second
        // Enter within the cooldown keeps the line and Update() sends it once the second passed.
        UI::Chat::ChatSendGate m_sendGate{ChatCooldownMs};
        // What the waiting line was when Enter kept it: a line changed since (text, whisper target or channel)
        // is not sent by itself.
        struct PendingLine
        {
            type_string text;
            type_string whisperId;
            int inputMsgType = 0;
            bool whisperSend = false;
            bool operator==(const PendingLine&) const = default;
        };
        PendingLine m_pendingLine;
        PendingLine CurrentLine();

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
        // Up in a field: the chat field's line sent before, the whisper field's name before. False when nothing came.
        bool RecallOlder(const char* id);
        // That field for the history button: the whisper field when it had the focus last and takes a name.
        const char* RecallFieldId() const;
        // The field that had the focus last, for the phone's history button (a tap on it takes the focus away).
        const char* m_lastFocusedField = "chat_field";
        // The quick phrase row and the bar, found once per document (QuickRowLiftPx() reads them every frame).
        mutable Rml::ObserverPtr<Rml::Element> m_quickRow;
        mutable Rml::ObserverPtr<Rml::Element> m_quickPanel;

        // Sends the chat field's line as Enter does (to the channel chosen, or the whisper target),
        // clears the field and closes the bar.
        void SendChatLine();
        // Clears the field and closes the bar after a line went.
        void CloseSentLine();

        // MU Moldova, items in the chat line (UI/Chat/ItemLinks.h): Shift+click puts an item's "[name]" into the
        // field, a token counted as long as it shows within the channel's line (89 characters in the party and the
        // guild, 90 elsewhere); the line then goes as "/il …".
        UI::Chat::ItemLinkDraft m_linkDraft;
        // The field's text the tokens were last followed in (FollowChatField()).
        Rml::String m_linkSeenText;
        // The caret (in characters) to put after an item once the bar that just opened takes the focus.
        std::optional<std::size_t> m_linkCaretPending;
        // Follows the player's edits of a line holding items: a token at the caret's edge goes out whole.
        void FollowChatField();
        // Shows `text` in the chat field with the caret at `caret` (an index into it), keeping the items.
        void PutChatField(const type_string& text, std::size_t caret);
        void SetChatCaret(std::size_t characters);
        // The chat field's caret as an index into `text` (the field's text).
        std::size_t ChatCaret(const type_string& text) const;
        // Opens the bar for an item where Enter opens it; false in Chaos Castle while its timer shows.
        bool OpenForItemLink();
        enum class LinkedSend
        {
            NotLinked,
            Sent,
            Refused,
        };
        // A line holding items goes as "/il …"; NotLinked when it holds none or cannot carry them (the gens
        // channel, a command): it then goes as typed. Refused: longer than its channel's line takes
        // (UI::Chat::ItemLinkWire::MaxLineLength, names counted as shown), too long for the packet, or sooner than
        // the server's cooldown after the last one (UI::Chat::ItemLinks::CooldownWait()); the line stays with its
        // items.
        LinkedSend SendLineWithItems();
        // The whisper target a line with items goes to; empty when the bar sends to the channel.
        type_string ItemLinkWhisperTarget();
        // The own whisper with its items as links, as EchoSentWhisper() shows a plain one.
        void EchoWhisperWithItems(const type_string& whisperId, const type_string& text);
        // The line Enter kept during the cooldown, once the cooldown passed.
        void SendPendingLine();

        // A phone's quick chat buttons 1 to 5 carry the saved macros of keys 1 to 5 (MacroText) when set.
        static bool HasMacro(int slot);
        void SendQuickMacro(int slot);
        // Sends a line as Enter does; a line half typed comes back in the bar after it.
        void SendLineKeepingDraft(const type_string& line);
        void SyncQuickButtons(ChatInputRmlModel& model);

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
        // A phone's quick phrase row stands over the bar, where the chat log's lowest lines are: how far it
        // reaches above the bar in window px, 0 while the bar or the row is not shown. The phone's HUD layout
        // lifts the log by it (PhoneHudLayout.cpp).
        float QuickRowLiftPx() const;
        // The field the phone's history button gives the focus back to when the window point x, y is on that
        // button; nullptr elsewhere. The page's keyboard treats a tap there as one on the field.
        Rml::Element* RecallFieldAt(float x, float y) const;

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
        // A quick chat phrase (UI/Chat/ChatPhrases, 1 and up) to the channel chosen or the whisper target, as Enter
        // sends a line; the pad keyboard's phrase page and a phone's chips. The bar must be open. False when nothing
        // went: an unknown id, or the pause shared with the macros (TakeQuickPhrasePause).
        bool SendPhrase(int id);

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
        // MU Moldova: puts an own item's "[name]" into the chat line at the caret, opening the bar as Enter does
        // (UI/Chat/ItemLinks.h ToChat). An item refused (too many, the line too long) says why in the chat. False
        // when the bar cannot open here.
        bool InsertItemLink(UI::Chat::ItemLinkDraft::Item item);

    protected:
        void GetChatText(type_string& strText);
        void GetWhsprID(type_string& strWhsprID);

        void UpdateWhisperTargetFromRightClick();
        static void EchoSentWhisper(const wchar_t* whisperId, const wchar_t* text);
        // "[to Name] text" as the own whisper shows.
        static type_string WhisperEchoText(const wchar_t* whisperId, const wchar_t* text);
    };
}

#endif // _NEWUICHATINPUTBOX_H_