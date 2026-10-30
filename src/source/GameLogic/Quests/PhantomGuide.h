#pragma once

// The Shadow Phantom Soldier's guide (ghinzuro, 2026-10-29): in her dialog she tells about this server, a topic
// per answer, in English and Russian (src/Localization/PhantomGuide.{en,ru}.resx). Her buff answer stays as
// NPCDialogue.bmd has it. The guide's dialog states are added to the dialog map after NPCDialogue.bmd loads; its
// words are looked up here on every use, so a language switch shows the next time a page is drawn.

#include "GameLogic/Quests/QuestMng.h"

namespace PhantomGuide
{
inline constexpr DWORD kNpc = 257;

// Her greeting gets the guide's text and the answer that opens the topics; the topic list and the topic
// pages are added as states of her dialog.
void AddTo(NPCDialogueMap& dialogues);

// The guide's words for a QuestWords index, nullptr for any other index.
const wchar_t* Words(int wordsIndex);

// The line over her head: Webzen's «I'll be your strength…» and the guide's invitation in turn (ghinzuro,
// 2026-10-29: «можно сделать новую про вики и ротировать»), each for 10 s, by the clock, not per call (2026-10-30:
// «фразы у эльфа слишком быстро меняются»). Each copy of her (by character key) switches at its own moment.
const wchar_t* OverheadLine(int characterKey);
} // namespace PhantomGuide
