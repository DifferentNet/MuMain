#include "GameLogic/Events/EntryLevels.h"

namespace GameLogic::Events::EntryLevels
{
namespace
{
constexpr int BloodCastleGrades = 8;
constexpr int DevilSquareGrades = 7;

// Castle 8 and square 7: the third class from level 331 (MU Skynet has no master levels).
constexpr Band BloodCastleBands[2][BloodCastleGrades] = {
    {{15, 80}, {81, 130}, {131, 180}, {181, 230}, {231, 280}, {281, 330}, {331, 400}, {331, 400}},
    {{10, 60}, {61, 110}, {111, 160}, {161, 210}, {211, 260}, {261, 310}, {311, 400}, {311, 400}},
};

constexpr Band DevilSquareBands[2][DevilSquareGrades] = {
    {{15, 130}, {131, 180}, {181, 230}, {231, 280}, {281, 330}, {331, 400}, {331, 400}},
    {{15, 110}, {111, 160}, {161, 210}, {211, 260}, {261, 310}, {311, 400}, {311, 400}},
};
} // namespace

int GradeCount(Event event)
{
    return event == Event::BloodCastle ? BloodCastleGrades : DevilSquareGrades;
}

Band BandOf(Event event, bool strongClass, int grade)
{
    if (grade < 0 || grade >= GradeCount(event))
        return {};
    const int table = strongClass ? 1 : 0;
    return event == Event::BloodCastle ? BloodCastleBands[table][grade] : DevilSquareBands[table][grade];
}

int GradeFor(Event event, bool strongClass, bool thirdClass, int level)
{
    const int last = GradeCount(event) - 1;
    if (thirdClass && level >= BandOf(event, strongClass, last).minLevel)
        return last;

    for (int grade = 0; grade < last; ++grade)
    {
        const Band band = BandOf(event, strongClass, grade);
        if (level >= band.minLevel && level <= band.maxLevel)
            return grade;
    }
    return -1;
}
} // namespace GameLogic::Events::EntryLevels
