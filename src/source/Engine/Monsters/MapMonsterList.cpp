#include "stdafx.h"

#include "Engine/Monsters/MapMonsterList.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace Engine::Monsters
{
    namespace
    {
        constexpr char CommentMark = '#';
        constexpr char Whitespace[] = " \t\r";

        bool IsBlankOrComment(const std::string& line)
        {
            const std::size_t first = line.find_first_not_of(Whitespace);
            return first == std::string::npos || line[first] == CommentMark;
        }

        // "<map> <monster>" with nothing after them, both non-negative.
        std::optional<std::pair<int, int>> ParsePair(const std::string& line)
        {
            std::istringstream fields(line);
            int map = -1;
            int type = -1;
            if (!(fields >> map >> type) || map < 0 || type < 0)
            {
                return std::nullopt;
            }

            std::string rest;
            if (fields >> rest)
            {
                return std::nullopt;
            }
            return std::make_pair(map, type);
        }
    }

    MapMonsterList MapMonsterList::Parse(std::istream& input)
    {
        MapMonsterList list;
        std::string line;
        while (std::getline(input, line))
        {
            if (IsBlankOrComment(line))
            {
                continue;
            }

            const std::optional<std::pair<int, int>> pair = ParsePair(line);
            if (!pair)
            {
                ++list.m_badLineCount;
                continue;
            }
            list.Add(pair->first, pair->second);
        }
        return list;
    }

    const std::vector<int>& MapMonsterList::TypesOf(int map) const
    {
        static const std::vector<int> NoTypes;
        const auto found = m_typesByMap.find(map);
        return found != m_typesByMap.end() ? found->second : NoTypes;
    }

    void MapMonsterList::Add(int map, int type)
    {
        std::vector<int>& types = m_typesByMap[map];
        if (std::find(types.begin(), types.end(), type) != types.end())
        {
            ++m_duplicateCount;
            return;
        }
        types.push_back(type);
        ++m_pairCount;
    }
}
