#include "GameLogic/Combat/DuelReplies.h"

namespace GameLogic::Combat
{
namespace
{
constexpr int ResultAccepted = 0;
constexpr int ResultTooLowLevel = 12;
constexpr int ResultFailed = 14;
constexpr int ResultDeclined = 15;
constexpr int ResultArenaFull = 16;
constexpr int ResultTooLowLevelOld = 28;
constexpr int ResultNotEnoughZen = 30;
} // namespace

DuelStartReply ClassifyDuelStart(int result)
{
    switch (result)
    {
    case ResultAccepted: return DuelStartReply::Accepted;
    case ResultTooLowLevel:
    case ResultTooLowLevelOld: return DuelStartReply::TooLowLevel;
    case ResultFailed: return DuelStartReply::Failed;
    case ResultDeclined: return DuelStartReply::Declined;
    case ResultArenaFull: return DuelStartReply::ArenaFull;
    case ResultNotEnoughZen: return DuelStartReply::NotEnoughZen;
    default: return DuelStartReply::Unknown;
    }
}

void DuelEndNotice::Started()
{
    m_on = true;
    m_finished = false;
}

void DuelEndNotice::Finished()
{
    m_finished = true;
}

void DuelEndNotice::Reset()
{
    m_on = false;
    m_finished = false;
    m_hadEnd = false;
}

bool DuelEndNotice::OnEnded(bool heroInArena, std::uint32_t nowMs)
{
    // Unsigned subtraction keeps the gap right across the tick counter's wrap.
    const bool sameBurst = m_hadEnd && nowMs - m_lastEndMs < BurstMs;
    m_hadEnd = true;
    m_lastEndMs = nowMs;
    if (sameBurst)
        return false;

    const bool say = (m_on || heroInArena) && !m_finished;
    m_on = false;
    m_finished = false;
    return say;
}
} // namespace GameLogic::Combat
