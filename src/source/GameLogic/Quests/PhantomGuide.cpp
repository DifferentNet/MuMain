#include "stdafx.h"
#include "PhantomGuide.h"

#include "I18N/All.h"

#include <chrono>

namespace PhantomGuide
{
namespace
{
// Her dialog's states: 0 is the greeting (NPCDialogue.bmd's, with the buff), the rest are the guide's: the topic
// lists 700, 710, ... and the topic pages 801, 802, ... An answer's result up to 900 is the state it leads to
// (CNPCDialogue::ProcessSelTextResult).
constexpr DWORD kGreetingState = 0;
constexpr DWORD kFirstListState = 700;
constexpr DWORD kListStateStep = 10;
constexpr DWORD kFirstTopicState = 801;

// The greeting's buff answer (CNPCDialogue::ProcessSelTextResult sends the buff request for it).
constexpr int kBuffResult = 902;

// How long each of her two overhead lines stays before the other one comes.
constexpr long long kOverheadLineMs = 10000;

// The guide's QuestWords indexes start far above Webzen's (QuestWords_*.bmd ends at 20999).
constexpr int kWordsBase = 900000;

struct Topic
{
    const wchar_t* const* label;
    const wchar_t* const* text;
};

// The resx strings by address: I18N points them at the chosen language's text on every locale switch. The
// first list shows the first kTopicsPerList, the next list the rest.
const Topic kTopics[] = {
    {&I18N::PhantomGuide::ResetsLabel, &I18N::PhantomGuide::ResetsText},
    {&I18N::PhantomGuide::TiersLabel, &I18N::PhantomGuide::TiersText},
    {&I18N::PhantomGuide::TowerLabel, &I18N::PhantomGuide::TowerText},
    {&I18N::PhantomGuide::EventsLabel, &I18N::PhantomGuide::EventsText},
    {&I18N::PhantomGuide::PvpLabel, &I18N::PhantomGuide::PvpText},
    {&I18N::PhantomGuide::PassLabel, &I18N::PhantomGuide::PassText},
    {&I18N::PhantomGuide::DailyLabel, &I18N::PhantomGuide::DailyText},
    {&I18N::PhantomGuide::TradeLabel, &I18N::PhantomGuide::TradeText},
    {&I18N::PhantomGuide::GearLabel, &I18N::PhantomGuide::GearText},
    {&I18N::PhantomGuide::HelperLabel, &I18N::PhantomGuide::HelperText},
    {&I18N::PhantomGuide::KeysLabel, &I18N::PhantomGuide::KeysText},
};
constexpr int kTopicCount = static_cast<int>(std::size(kTopics));

// A state holds QM_MAX_ND_ANSWER answers: a list shows as many topics as leave room for "More topics" (or "First
// topics" on the last list) and "Back".
constexpr int kTopicsPerList = QM_MAX_ND_ANSWER - 2;
constexpr int kListCount = (kTopicCount + kTopicsPerList - 1) / kTopicsPerList;
static_assert(kFirstListState + kListCount * kListStateStep <= kFirstTopicState, "guide lists overlap the topics");
static_assert(kFirstTopicState + kTopicCount <= 900, "guide states must stay below the dialog's results");

enum Word : int
{
    kWordGreeting = kWordsBase + 1,
    kWordGuide,
    kWordTopicsIntro,
    kWordOtherTopics,
    kWordBack,
    kWordMoreTopics,
    kWordFirstTopics,
    kWordBuff,
    kWordFirstTopic, // then a label and a text per topic
};

constexpr int TopicLabelWord(int topic)
{
    return kWordFirstTopic + topic * 2;
}

constexpr int TopicTextWord(int topic)
{
    return kWordFirstTopic + topic * 2 + 1;
}

constexpr DWORD ListState(int list)
{
    return kFirstListState + list * kListStateStep;
}

constexpr DWORD DialogIndex(DWORD state)
{
    return kNpc * 0x10000 + state;
}

SNPCDialogue NewState(int npcWords)
{
    SNPCDialogue state{};
    state.m_nNPCWords = npcWords;
    return state;
}

// Puts an answer into the first free slot of a state; false when all slots are taken.
bool AddAnswer(SNPCDialogue& state, int words, int result)
{
    for (int i = 0; i < QM_MAX_ND_ANSWER; ++i)
    {
        if (state.m_anAnswer[i * 2] == 0)
        {
            state.m_anAnswer[i * 2] = words;
            state.m_anAnswer[i * 2 + 1] = result;
            return true;
        }
    }
    return false;
}
} // namespace

void AddTo(NPCDialogueMap& dialogues)
{
    // The greeting keeps its answers (the buff) and gets the guide after them.
    SNPCDialogue& greeting = dialogues.try_emplace(DialogIndex(kGreetingState), NewState(0)).first->second;
    greeting.m_nNPCWords = kWordGreeting;
    // Webzen's buff answer has only an English text (QuestWords): the guide's own string speaks both languages.
    for (int i = 0; i < QM_MAX_ND_ANSWER; ++i)
    {
        if (greeting.m_anAnswer[i * 2] != 0 && greeting.m_anAnswer[i * 2 + 1] == kBuffResult)
            greeting.m_anAnswer[i * 2] = kWordBuff;
    }
    AddAnswer(greeting, kWordGuide, ListState(0));

    for (int list = 0; list < kListCount; ++list)
    {
        SNPCDialogue listState = NewState(kWordTopicsIntro);
        const int first = list * kTopicsPerList;
        const int end = (std::min)(first + kTopicsPerList, kTopicCount);
        for (int topic = first; topic < end; ++topic)
        {
            AddAnswer(listState, TopicLabelWord(topic), kFirstTopicState + topic);

            // A topic's page leads back to the list it is on.
            SNPCDialogue page = NewState(TopicTextWord(topic));
            AddAnswer(page, kWordOtherTopics, ListState(list));
            dialogues[DialogIndex(kFirstTopicState + topic)] = page;
        }
        if (kListCount > 1)
        {
            const bool last = list + 1 == kListCount;
            AddAnswer(listState, last ? kWordFirstTopics : kWordMoreTopics, last ? ListState(0) : ListState(list + 1));
        }
        AddAnswer(listState, kWordBack, kGreetingState);
        dialogues[DialogIndex(ListState(list))] = listState;
    }
}

const wchar_t* Words(int wordsIndex)
{
    switch (wordsIndex)
    {
    case kWordGreeting:
        return I18N::PhantomGuide::Greeting;
    case kWordGuide:
        return I18N::PhantomGuide::Guide;
    case kWordTopicsIntro:
        return I18N::PhantomGuide::TopicsIntro;
    case kWordOtherTopics:
        return I18N::PhantomGuide::OtherTopics;
    case kWordBack:
        return I18N::PhantomGuide::Back;
    case kWordMoreTopics:
        return I18N::PhantomGuide::MoreTopics;
    case kWordFirstTopics:
        return I18N::PhantomGuide::FirstTopics;
    case kWordBuff:
        return I18N::PhantomGuide::Buff;
    default:
        break;
    }

    const int topicWord = wordsIndex - kWordFirstTopic;
    if (topicWord < 0 || topicWord >= kTopicCount * 2)
        return nullptr;

    const Topic& topic = kTopics[topicWord / 2];
    return topicWord % 2 == 0 ? *topic.label : *topic.text;
}

const wchar_t* OverheadLine(int characterKey)
{
    // The game asks many times a second (OnlyNpcChatProcess), so the line follows the clock: each one stays
    // kOverheadLineMs, and each copy switches at its own moment (an offset from its key), not all of them at once.
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto offset = static_cast<long long>((static_cast<unsigned>(characterKey) * 2654435761u) % kOverheadLineMs);
    const bool invitation = ((now + offset) / kOverheadLineMs) % 2 == 0;
    return invitation ? I18N::PhantomGuide::Overhead : I18N::Game::ILlBeYourStrengthForTheJourneyToBecomeAWarrior;
}
} // namespace PhantomGuide
