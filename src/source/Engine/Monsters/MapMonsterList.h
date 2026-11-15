#pragma once

#include <istream>
#include <map>
#include <vector>

namespace Engine::Monsters
{
    // The monster types the server spawns on each map, read from Data/Local/MapMonsters.txt
    // (tools/web/make_map_files.py --monsters-out). A line is "<map> <monster>"; empty lines and lines
    // starting with '#' are skipped. A malformed line (not two non-negative numbers) is counted and
    // skipped, a repeated pair is kept once, and a map's types keep the order of the file.
    class MapMonsterList
    {
    public:
        static MapMonsterList Parse(std::istream& input);

        // The map's types, empty for a map the file doesn't name.
        const std::vector<int>& TypesOf(int map) const;

        int MapCount() const { return static_cast<int>(m_typesByMap.size()); }
        int PairCount() const { return m_pairCount; }
        int BadLineCount() const { return m_badLineCount; }
        int DuplicateCount() const { return m_duplicateCount; }

    private:
        void Add(int map, int type);

        std::map<int, std::vector<int>> m_typesByMap;
        int m_pairCount = 0;
        int m_badLineCount = 0;
        int m_duplicateCount = 0;
    };
}
