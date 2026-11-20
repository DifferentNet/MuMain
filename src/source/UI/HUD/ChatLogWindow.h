#ifndef _NEWCHATLONGWINDOW_H_
#define _NEWCHATLONGWINDOW_H_

#pragma once

#include "UI/Core/WindowObject.h"
#include "UI/Chat/MessageType.h"
#include "Render/Textures/ZzzTexture.h"
#include "UI/HUD/ChatInputBox.h"
#include "UI/RmlBridge/RmlThemedView.h"

#pragma warning(disable : 4786)
#include <string>
#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace Rml
{
    class ElementDocument;
    class Context;
}

namespace mu::ui::window
{
    class CManager;

    // The clock a chat line is stamped with when it comes (ChatLogWindow.cpp), in milliseconds.
    std::uint64_t ChatLogClockMs();

    template <class T>
    class TMessageText
    {
        typedef std::wstring type_string;

        type_string	m_strID, m_strText;
        MESSAGE_TYPE m_MsgType;
        DWORD m_dwIndentSize;
        // When the line came (ChatLogClockMs()); a closed chat lets it go after a while (UI/Chat/ChatFade.h).
        std::uint64_t m_addedAtMs = 0;
        // The line as the chat log shows it ("ID : text"), in UTF-8, made once when the log first shows it
        // (CChatLogWindow::RebuildLineModel()) instead of on every rebuild of the list.
        std::string m_lineUtf8;

    public:
        TMessageText() : m_MsgType(TYPE_UNKNOWN), m_dwIndentSize(0) {}
        ~TMessageText() { Release(); }

        bool Create(const type_string& strID, const type_string& strText, MESSAGE_TYPE MsgType)
        {
            if (MsgType >= NUMBER_OF_TYPES)
                return false;

            m_strID = strID;
            m_strText = strText;
            m_MsgType = MsgType;
            m_addedAtMs = ChatLogClockMs();
            m_lineUtf8.clear();

            return true;
        }
        void Release()
        {
            m_strID.resize(0);
            m_strText.resize(0);
            m_lineUtf8.clear();
            m_MsgType = TYPE_UNKNOWN;
        }

        const type_string& GetID() const { return m_strID; }
        const type_string& GetText() const { return m_strText; }
        MESSAGE_TYPE GetType() const { return m_MsgType; }
        std::uint64_t GetAddedAtMs() const { return m_addedAtMs; }
        // Empty until the chat log has made it (a shown line is never empty: it has text).
        const std::string& GetLineUtf8() const { return m_lineUtf8; }
        void SetLineUtf8(std::string line) { m_lineUtf8 = std::move(line); }
    };

    typedef TMessageText<wchar_t> CMessageText;

    // One rendered chat line. `kind` is the lowercase message-type slug the RCSS builds its
    // .chat-line--<kind> class from; `hasId` marks a line whose sender can be right-clicked to
    // target a whisper (and which therefore gets the hover highlight). `faded` marks a line a
    // closed chat has let go (UI/Chat/ChatFade.h): it stays in the list, so indices still match,
    // but takes no space; `lead` marks the first line on screen, which carries the bottom-up
    // stacking margin (chat_log.rcss).
    struct ChatLogLineEntry
    {
        Rml::String text;
        Rml::String kind;
        bool hasId = false;
        bool faded = false;
        bool lead = false;
    };

    // RmlUi owns this window's whole presentation now: the background fill, every message line,
    // and -- via base.rcss's .scroll-pane on #lines -- the scroll position, mouse wheel and
    // scrollbar drag. C++ keeps the message vectors, the filters and the 3-line-step resize, and
    // pins the view to the bottom when a new message arrives.
    struct ChatLogRmlModel
    {
        float panelHeight = 100.0f;  // dp -- 15 * showing lines + 10, native's own UpdateWndSize()
        float clientHeight = 90.0f;  // dp -- the scrolling well inside it
        // The user's transparency setting on its own, 0..1. Frame state is showFrame below and
        // the backdrop's colour is the theme's -- three separate owners, not one composed string.
        float backAlpha = 0.f;
        bool showFrame = false;
        // Index of the line under the cursor, or -1. Drives .chat-line--pointed; C++ resolves it
        // because the lines themselves are pointer-events:none (see chat_log.rcss).
        int pointedIndex = -1;
        Rml::Vector<ChatLogLineEntry> lines;

        // The native renderer's line geometry in physical px (the legacy theme's chat_log.rml):
        // the text size, each line's background height (the measured text) and the 15-unit row
        // pitch of the dp-sized well.
        float textPx = 0.f;
        float linePx = 0.f;
        float rowPx = 0.f;
    };

    class CChatLogWindow : public CObject
    {
    public:
        enum IMAGE_LIST
        {
            IMAGE_SCROLL_TOP = BITMAP_INTERFACE_NEW_CHATLOGWND_BEGIN,	//. newui_scrollbar_up.tga
            IMAGE_SCROLL_MIDDLE,	//. newui_scrollbar_m.tga
            IMAGE_SCROLL_BOTTOM,	//. newui_scrollbar_down.tga
            IMAGE_SCROLLBAR_ON,		//. newui_scroll_on.tga
            IMAGE_SCROLLBAR_OFF,	//. newui_scroll_off.tga
            IMAGE_DRAG_BTN,			//. newui_scrollbar_stretch.tga
        };

    private:
        static constexpr int MAX_CHAT_BUFFER_SIZE = 60;
        static constexpr int MAX_NUMBER_OF_LINES = 200;
        // A full list drops this many of its oldest lines at once: dropping one per message shifted every row of the
        // list on every message, while now the appends in between only add a row.
        static constexpr int LINES_DROPPED_AT_CAP = 20;
        static constexpr float WND_WIDTH = CChatInputBox::CHATBOX_WIDTH;
        static constexpr float FONT_LEADING = 4.0f;
        static constexpr float WND_TOP_BOTTOM_EDGE = 2.0f;
        static constexpr float WND_LEFT_RIGHT_EDGE = 4.0f;
        static constexpr float RESIZING_BTN_WIDTH = WND_WIDTH;
        static constexpr float RESIZING_BTN_HEIGHT = 10.0f;
        static constexpr float SCROLL_BAR_WIDTH = 7.0f;
        static constexpr float SCROLL_TOP_BOTTOM_PART_HEIGHT = 3.0f;
        static constexpr float SCROLL_MIDDLE_PART_HEIGHT = 15.0f;
        static constexpr float CLIENT_WIDTH = WND_WIDTH - SCROLL_BAR_WIDTH * 2.0f - (WND_LEFT_RIGHT_EDGE * 2.0f);
        // SplitIntoRows() without RmlUi, or in a log too narrow to split for: room kept free at the end of a row
        // measured with g_pRenderText (the chat window's font draws a little wider than it measures, most of all on /play).
        static constexpr int FIRST_ROW_SAFETY_MARGIN = 8;
        typedef std::wstring type_string;
        typedef std::vector<CMessageText*>	type_vector_msgs;
        typedef std::vector<type_string>	type_vector_filters;

        CManager* m_pNewUIMng;

        type_vector_msgs	m_vecAllMsgs;
        type_vector_msgs	m_VecChatMsgs;
        type_vector_msgs	m_vecWhisperMsgs;
        type_vector_msgs	m_vecPartyMsgs;
        type_vector_msgs	m_vecGuildMsgs;
        type_vector_msgs	m_vecUnionMsgs;
        type_vector_msgs	m_vecGensMsgs;
        type_vector_msgs	m_VecSystemMsgs;
        type_vector_msgs	m_vecErrorMsgs;
        type_vector_msgs	m_vecGMMsgs;
        type_vector_filters	m_vecFilters;

        SIZE	m_WndSize;
        int		m_nShowingLines;

        MESSAGE_TYPE		m_CurrentRenderMsgType;
        bool				m_bShowChatLog;
        bool m_bSceneAllowsShow = false;
        int		m_iCurrentRenderEndLine;
        float	m_fBackAlpha;


        bool m_bShowFrame;

        void Init();

        void BuildRmlUi();
        void SyncRmlModel();
        void SyncNativeLineGeometry();
        // Rebuilds the bound line list from the currently-selected message vector. Kept
        // index-aligned with that vector (undrawable entries become blanks rather than being
        // skipped) because chat_line_rightclick() resolves a sender by array index. Only entries
        // that differ are rewritten; returns whether any did, so an unchanged list is not marked dirty.
        bool RebuildLineModel();
        // Pins #lines to the bottom after the model's line list changes, so a new message scrolls
        // into view exactly as native's own "follow the tail" behaviour did. Skipped while the
        // user has scrolled up, same as native.
        void ScrollToBottomIfFollowing();
        bool IsScrolledToBottom() const;
        // Resolves which rendered line the cursor is over and acts on a right-click, replacing
        // native's own per-line loop in UpdateMouseEvent(). Compares raw window pixels against
        // RmlUi's own element boxes -- both are already in screen space, so no transform
        // conversion enters anywhere (that conversion is the bug class RmlPanelGeometry.h warns
        // about).
        void UpdatePointedLine();
        // The shown line with a sender under a window pixel (the whole row, gap included), or -1.
        int SenderLineAt(float px, float py) const;
        // Puts a line's sender into the whisper field; `focusMessage` also takes the caret to the message field.
        void WhisperToSenderOf(int line, bool focusMessage);
        // A phone's tap on the chat's history while its frame is shown whispers to the line's sender, as a right
        // click does on a PC (a long press there would cast the selected skill).
        void OnLinesTapped(float px, float py);
        // Pushes m_iCurrentRenderEndLine into RmlUi's scroll offset (native's own fPosRate math),
        // and, when no request is pending, reads it back the other way so the logical cursor
        // tracks wherever the user dragged or scrolled to.
        void ApplyLogicalScroll();
        void SyncLogicalScrollFromView();
        // #lines of the current document, found by its id once rather than several times a frame.
        Rml::Element* LinesElement() const;
        mutable Rml::ObserverPtr<Rml::Element> m_LinesElement;
        // The hidden row of #lines a message of this type is measured with (chat_log.rml): the GM one is bold.
        Rml::Element* RowProbe(MESSAGE_TYPE MsgType) const;
        mutable Rml::ObserverPtr<Rml::Element> m_RowProbe;
        mutable Rml::ObserverPtr<Rml::Element> m_GmRowProbe;
        // The width the hidden row of this type gives a row's text, in RmlUi's layout px; 0 until RmlUi has updated
        // the log's document once (before that the legacy line's text size is not on it yet). A hidden document,
        // which RmlUi leaves as it was, and one not laid out yet are brought up to date here, once a frame at most.
        float RowRoom(MESSAGE_TYPE MsgType);
        // Whether a message of this type can be split into rows now: on the hidden row, or with the native font
        // when there is no RmlUi to wait for.
        bool CanSplitRows(MESSAGE_TYPE MsgType);
        // The document RmlUi has updated since it was built (AfterRmlLayout()); only compared, never followed.
        const Rml::ElementDocument* m_UpdatedDocument = nullptr;
        // RowRoom() forced the document's layout this frame.
        bool m_bLayoutForced = false;
        // MU Skynet: messages that came before the log could measure their rows (the first ones at world entry come
        // before the log's document exists), in the order they came. Each is split and added as soon as it can be,
        // and the ones after it wait behind it, so the order stays.
        struct PendingMessage
        {
            type_string id;
            type_string text;
            MESSAGE_TYPE type;
            MESSAGE_TYPE errType;
        };
        std::deque<PendingMessage> m_PendingMessages;
        void AddPendingMessages();
        // A full list drops its oldest lines before a message comes in (AddText()).
        void DropOldestAtCap(MESSAGE_TYPE MsgType);

        void BindRmlModel(Rml::DataModelConstructor& c, ChatLogRmlModel& model);
        void OnRmlReloaded();
        UI::RmlBridge::ThemedView<ChatLogRmlModel> m_RmlView{"chat_log",
            [this](Rml::DataModelConstructor& c, ChatLogRmlModel& model) { BindRmlModel(c, model); },
            {{"Data/Interface/RmlUi/chat_log.rml"}},
            {.afterReload = [this] { OnRmlReloaded(); }}};
        // The log's bottom edge (screen pixels) and scale where it is placed; the resize bands
        // measure from them.
        float m_PanelBottomPx = 0.f;
        float m_PanelScale = 0.f;
        bool m_bLinesDirty = true;
        bool m_bFollowTail = true;
        // One-shot, set when the line list changes and consumed after RmlUi has
        // laid the new lines out. Never pin outside this latch -- doing so re-clamps the view
        // every frame and makes the scrollbar and wheel look broken.
        bool m_bScrollPending = false;
        // Set by Scrolling()/UpdateScrollPos() -- the logical scroll position still has external
        // drivers (CChatInputBox's PageUp/PageDown and its resize buttons), so those requests are
        // latched and applied to RmlUi's own scroll offset on the next sync rather than fighting
        // it every frame.
        bool m_bScrollRequest = false;
        // MU Moldova, lines that go after a while (UI/Chat/ChatFade.h). Whether the chat counted as
        // open at the last rebuild, and when a line on screen goes next (empty: none will). The
        // model is rebuilt only when either changes, never every frame.
        bool m_bChatOpen = false;
        std::optional<std::uint64_t> m_NextFadeMs;
        bool FadeChanged();

    public:
        // The chat input is up or the log's frame is shown; the system log asks too (CSystemLogWindow).
        bool IsChatOpen() const;

        CChatLogWindow();
        ~CChatLogWindow() override;

        bool Create(CManager* pNewUIMng, int nShowingLines = 6);
        void Release();

        void AddText(const type_string& strID, const type_string& strText, MESSAGE_TYPE MsgType, MESSAGE_TYPE ErrMsgType = TYPE_ALL_MESSAGE);
        // Drops the `count` oldest lines of the list; the view keeps showing the same messages.
        void RemoveFrontLine(MESSAGE_TYPE MsgType, int count = 1);
        void Clear(MESSAGE_TYPE MsgType);
        void ClearAll();

        size_t GetNumberOfLines(MESSAGE_TYPE MsgType);
        MESSAGE_TYPE GetCurrentMsgType() const;

        void ChangeMessage(MESSAGE_TYPE MsgType);

        void ShowChatLog();
        void HideChatLog();

        int GetCurrentRenderEndLine() const;
        void Scrolling(int nRenderEndLine);

        void SetFilterText(const type_string& strFilterText);
        void ResetFilter();

        void SetSizeAuto();
        void SetNumberOfShowingLines(int nShowingLines, OUT LPSIZE lpBoxSize = NULL);
        size_t GetNumberOfShowingLines() const;
        void SetBackAlphaAuto();
        void SetBackAlpha(float fAlpha);
        float GetBackAlpha() const;

        void ShowFrame();
        void HideFrame();
        bool IsShowFrame();

        bool UpdateMouseEvent() override;
        // Native's 3-line resize steps for the pointer's height (screen pixels) while #resize_handle
        // is dragged.
        void ResizeToPointer(float pointerYPx);
        bool UpdateKeyEvent() override;
        bool Update() override;
        void AfterRmlLayout(Rml::Context* context);
        // Hides the document outside the main scene (CSystem::SyncMainSceneHudVisibility()): the
        // window is only updated there, but its document would keep drawing in every scene.
        void SyncDocVisibility(bool sceneAllowsShow);

        bool Render() override;

        float GetLayerDepth() override;	//. 6.1f
        float GetKeyEventOrder() override;	//. 8.0f

        void UpdateWndSize();
        void UpdateScrollPos();

    protected:
        type_vector_msgs* GetMsgs(MESSAGE_TYPE MsgType);
        // Adds the message's rows, or keeps it pending while the log cannot measure them yet.
        void ProcessAddText(const type_string& strID, const type_string& strText, MESSAGE_TYPE MsgType, MESSAGE_TYPE ErrMsgType);
        void AddRows(const type_string& strID, const type_string& strText, MESSAGE_TYPE MsgType, MESSAGE_TYPE ErrMsgType);

        // The rows a message takes in the log, none wider than a row of the log (UI/Chat/ChatRowWrap.h): the
        // first is drawn with "ID : " in front of it, the rest hold the text alone.
        std::vector<type_string> SplitIntoRows(const type_string& strID, const type_string& strText,
                                               MESSAGE_TYPE MsgType);

        bool CheckFilterText(const type_string& strTestText);
        void AddFilterWord(const type_string& strWord);
    };

    // Top-left system/error overlay. Reuses ChatLogLineEntry (its `hasId` is simply never set --
    // these lines have no sender), so both log windows share one line shape.
    struct SystemLogRmlModel
    {
        // Native applies the user's transparency setting to every line's own background quad, so
        // this is a per-line colour, not a panel fill.
        float backAlpha = 0.6f; // the player's own transparency setting, 0 .. 1
        Rml::Vector<ChatLogLineEntry> lines;

        // The native renderer's geometry under the window's layout, in physical px (the legacy
        // theme's system_log.rml): the row pitch, each line's background
        // height and the text size.
        float rowPx = 0.f;
        float linePx = 0.f;
        float textPx = 0.f;
    };

    class CSystemLogWindow : public CObject
    {
    private:
        enum
        {
            MAX_MSG_BUFFER_SIZE = 6,
            MAX_NUMBER_OF_LINES = 6,
            WND_WIDTH = CChatInputBox::CHATBOX_WIDTH,
            FONT_LEADING = 4,
            WND_TOP_BOTTOM_EDGE = 2,
            WND_LEFT_RIGHT_EDGE = 4,
            CLIENT_WIDTH = WND_WIDTH - (WND_LEFT_RIGHT_EDGE * 2),
        };


        typedef std::wstring type_string;
        typedef std::vector<CMessageText*>	type_vector_msgs;

        CManager* m_pNewUIMng;

        type_vector_msgs	m_vecAllMsgs;

        SIZE	m_WndSize;
        int		m_nShowingLines;

        int		m_iCurrentRenderEndLine;
        float	m_fBackAlpha;
        bool    m_bShowMessages;
        bool m_bSceneAllowsShow = false;

        void Init();

        void BuildRmlUi();
        void SyncRmlModel();
        void SyncNativeGeometry();
        void RebuildLineModel();

        void BindRmlModel(Rml::DataModelConstructor& c, SystemLogRmlModel& model);
        void OnRmlBuilt();
        void OnRmlReloaded();
        UI::RmlBridge::ThemedView<SystemLogRmlModel> m_RmlView{"system_log",
            [this](Rml::DataModelConstructor& c, SystemLogRmlModel& model) { BindRmlModel(c, model); },
            {{"Data/Interface/RmlUi/system_log.rml"}},
            {.afterBuild = [this] { OnRmlBuilt(); }, .afterReload = [this] { OnRmlReloaded(); }}};
        bool m_bLinesDirty = true;
        float m_LastPanelHeight = -1.f;
        // #panel of the current document, found by its id once rather than every frame.
        Rml::ObserverPtr<Rml::Element> m_PanelElement;
        void TrackPanelSize();
        // The height of L"Q" SyncNativeGeometry measured last, and what it was measured with.
        const void* m_MeasuredFont = nullptr;
        unsigned int m_MeasuredWindowWidth = 0;
        unsigned int m_MeasuredWindowHeight = 0;
        float m_MeasuredNativeTextPx = -1.f;
        float m_MeasuredTextHeight = 0.f;
        // MU Moldova: the newest messages, each for a while -- 3 for 8 s on a phone, 6 for 30 s on a PC
        // unless the chat is open (ChatLogWindow.cpp, UI/Chat/ChatFade.h). When each message of
        // m_vecAllMsgs came (ChatLogClockMs()), and how many the model showed last.
        std::vector<std::uint64_t> m_Arrivals;
        // How many times each line of m_vecAllMsgs came in a row (shown as "×N", UI/Chat/ChatFade.h).
        std::vector<int> m_RepeatCounts;
        int m_ShownCount = -1;
        int ShownCount() const;
        // The rows the log keeps room for: 3 on a phone, 6 on a PC and on a phone with the chat open.
        int ReservedRows() const;

        void RemoveFrontLine();
        // The same text again within a moment: counts it on the newest line instead of adding one.
        bool FoldIntoNewestLine(const type_string& strText, MESSAGE_TYPE MsgType);
        int GetCurrentRenderEndLine() const;

    public:

        CSystemLogWindow();
        ~CSystemLogWindow() override;

        bool Create(CManager* pNewUIMng);
        void Release();

        void AddText(const type_string& strText, MESSAGE_TYPE MsgType);

        void ClearAll();
        void ShowMessages() { m_bShowMessages = true; }
        void HideMessages() { m_bShowMessages = false; }

        bool UpdateMouseEvent() override;
        bool UpdateKeyEvent() override;
        bool Update() override;
        // Hides the document outside the main scene (CSystem::SyncMainSceneHudVisibility()): the
        // window is only updated there, but its document would keep drawing in every scene.
        void SyncDocVisibility(bool sceneAllowsShow);

        bool Render() override;

        float GetLayerDepth() override;
        float GetKeyEventOrder() override;

        bool CheckChatRedundancy(const type_string& strText, int iSearchLine = 1);
    };
}

#endif // _NEWCHATLONGWINDOW_H_
