#include "stdafx.h"

#include "Engine/Monsters/MonsterPreload.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Engine/Monsters/MapMonsterList.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzObject.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Sprites/GlobalBitmap.h"
#include "World/MapInfra/MapManager.h"

#ifdef __EMSCRIPTEN__
#include "UI/Mobile/InterfaceMode.h"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace Engine::Monsters
{
    namespace
    {
        // The dummy's key. CHARACTER::Key is a short and the server sends 0..0x7FFF; -1 means "no one" in
        // places, so -2. It must never be a live character's key: CreateCharacter reuses the character that
        // has the key it is given.
        constexpr int DryRunKey = -2;
        static_assert(static_cast<decltype(CHARACTER::Key)>(DryRunKey) == DryRunKey,
                      "the dry run's key must survive the store into CHARACTER::Key");
        static_assert(DryRunKey < 0 && DryRunKey != -1, "the dry run's key must be one the server never sends");

#ifdef __EMSCRIPTEN__
        // The browser client preloads on a computer. A phone or tablet (UI::Mobile::IsHandheld) doesn't:
        // creating the models up front would cost it memory and a longer freeze. There the map's pack
        // (tools/web/make_map_files.py) still brings the map's monster files, so a type's first sight
        // reads them from memory, not the network.
        bool PreloadOnThisDevice()
        {
            return !UI::Mobile::IsHandheld();
        }
#else
        constexpr bool PreloadOnThisDevice()
        {
            return true;
        }
#endif

        // LoadingWorld after a map entry (WSclient.cpp); a gate's 9999999 only waits for the server's
        // teleport, after which LoadWorld releases every monster model.
        constexpr int MapEntryCountdown = 30;

        constexpr double SyncBudgetMs = 1500.0;
        constexpr int MaxTypesPerMap = 40;
        constexpr std::uint64_t BytesPerMiB = 1024ull * 1024ull;
        constexpr std::uint64_t MaxTextureBytes = 256ull * BytesPerMiB;

        // Creators that do more than build the character: Erohim's plays his entrance sound.
        constexpr std::array<EMonsterType, 1> NeverWarmedTypes = {MONSTER_EROHIM};

        const std::filesystem::path SpawnListFile = std::filesystem::path("Data/Local/MapMonsters.txt");

        bool g_warmingUp = false;
        int g_queuedMap = -1;
        // The world LoadWorld actually loaded: Devil Square's 32 becomes 9 there, the list is by the server's 32.
        int g_queuedWorld = -1;

        double NowMs()
        {
            using Clock = std::chrono::steady_clock;
            return std::chrono::duration<double, std::milli>(Clock::now().time_since_epoch()).count();
        }

        double ToMiB(std::uint64_t bytes)
        {
            return static_cast<double>(bytes) / static_cast<double>(BytesPerMiB);
        }

        const MapMonsterList& SpawnList()
        {
            static const MapMonsterList list = [] {
                std::ifstream file(SpawnListFile);
                if (!file)
                {
                    MU_LOG_WARN(mu::log::Get("data"), "Monster preload: {} not found, nothing is preloaded",
                                SpawnListFile.generic_string());
                    return MapMonsterList{};
                }
                MapMonsterList parsed = MapMonsterList::Parse(file);
                MU_LOG_INFO(mu::log::Get("data"), "Monster preload: {} pairs for {} maps ({} bad lines, {} repeated)",
                            parsed.PairCount(), parsed.MapCount(), parsed.BadLineCount(), parsed.DuplicateCount());
                return parsed;
            }();
            return list;
        }

        MonsterPreloadQueue& Queue()
        {
            static MonsterPreloadQueue queue(
                [](int type) { return WarmUp(static_cast<EMonsterType>(type)); }, NowMs,
                [] { return static_cast<std::uint64_t>(Bitmaps.GetUsedTextureMemory()); },
                MonsterPreloadLimits{SyncBudgetMs, MaxTypesPerMap, MaxTextureBytes});
            return queue;
        }

        template <typename T>
        std::vector<bool> LiveFlags(const T* items, int count)
        {
            std::vector<bool> live(count);
            for (int i = 0; i < count; ++i)
            {
                live[i] = items[i].Live;
            }
            return live;
        }

        template <typename T>
        void KillNewlyLive(T* items, const std::vector<bool>& before)
        {
            for (std::size_t i = 0; i < before.size(); ++i)
            {
                if (items[i].Live && !before[i])
                {
                    items[i].Live = false;
                }
            }
        }

        // What the world held before a dry run. Nothing else runs during one, so whatever came alive in the
        // effect pools or was appended to an object block meanwhile came from the dummy's creation (the
        // creators start energy joints, flares, auras and, for the siege suppliers, a box object).
        class WorldBeforeDryRun
        {
        public:
            WorldBeforeDryRun()
                : m_effects(LiveFlags(Effects, MAX_EFFECTS)), m_sprites(LiveFlags(Sprites, MAX_SPRITES)),
                  m_joints(LiveFlags(Joints, MAX_JOINTS)), m_particles(LiveFlags(Particles, MAX_PARTICLES)),
                  m_points(LiveFlags(Points, MAX_POINTS)), m_pointers(LiveFlags(Pointers, MAX_POINTERS))
            {
                for (std::size_t i = 0; i < m_blockTails.size(); ++i)
                {
                    m_blockTails[i] = ObjectBlock[i].Tail;
                }
            }

            void RemoveWhatAppeared() const
            {
                KillNewlyLive(Effects, m_effects);
                KillNewlyLive(Sprites, m_sprites);
                KillNewlyLive(Joints, m_joints);
                KillNewlyLive(Particles, m_particles);
                NoteSpriteFreed(0);
                NoteJointFreed(0);
                NoteParticleFreed(0);
                KillNewlyLive(Points, m_points);
                KillNewlyLive(Pointers, m_pointers);
                for (std::size_t i = 0; i < m_blockTails.size(); ++i)
                {
                    OBJECT_BLOCK* block = &ObjectBlock[i];
                    while (block->Tail != nullptr && block->Tail != m_blockTails[i])
                    {
                        DeleteObject(block->Tail, block);
                    }
                }
            }

        private:
            std::vector<bool> m_effects;
            std::vector<bool> m_sprites;
            std::vector<bool> m_joints;
            std::vector<bool> m_particles;
            std::vector<bool> m_points;
            std::vector<bool> m_pointers;
            std::array<OBJECT*, std::size(ObjectBlock)> m_blockTails{};
        };

        bool IsNeverWarmed(EMonsterType type)
        {
            return std::find(NeverWarmedTypes.begin(), NeverWarmedTypes.end(), type) != NeverWarmedTypes.end();
        }

        bool HasFreeCharacterSlot()
        {
            for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
            {
                if (!CharactersClient[i].Object.Live)
                {
                    return true;
                }
            }
            return false;
        }

        bool CanDryRun(EMonsterType type)
        {
            if (Hero == nullptr || !Hero->Object.Live || IsNeverWarmed(type))
            {
                return false;
            }
            // The key in use (a dry run that didn't end) or the type already in view, so already loaded.
            if (FindCharacterIndex(DryRunKey) != MAX_CHARACTERS_CLIENT ||
                FindCharacterIndexByMonsterIndex(type) != MAX_CHARACTERS_CLIENT)
            {
                return false;
            }
            return HasFreeCharacterSlot();
        }

        void LogMapPreload(int map, const MonsterPreloadStats& stats)
        {
            std::string limits;
            if (stats.overLimit > 0)
            {
                limits += ", " + std::to_string(stats.overLimit) + " over the type limit";
            }
            if (stats.textureLimitReached)
            {
                limits += ", texture limit reached";
            }
            MU_LOG_INFO(mu::log::Get("data"),
                        "Monster preload: map {}, {} types ({} skipped), {:.0f} ms, +{:.1f} MiB textures, {} left "
                        "for later{}",
                        map, stats.warmed, stats.skipped, stats.elapsedMs, ToMiB(stats.textureBytes), stats.left,
                        limits);
        }
    }

    WarmUpResult WarmUp(EMonsterType type)
    {
        if (!CanDryRun(type))
        {
            return WarmUpResult::Skipped;
        }

        const WorldBeforeDryRun before;
        g_warmingUp = true;
        CHARACTER* dummy = CreateMonster(type, Hero->PositionX, Hero->PositionY, DryRunKey);
        g_warmingUp = false;

        // NULL for a type no creator knows, the slot past the end when all were taken.
        const bool created = dummy != nullptr && dummy != &CharactersClient[MAX_CHARACTERS_CLIENT] &&
                             dummy->Object.Live && dummy->Key == DryRunKey;
        if (created)
        {
            DeleteCharacter(dummy, &dummy->Object);
            // Some creators put a buff on the monster (the Poison Bull); the slot's next owner starts clean.
            g_CharacterClearBuff((&dummy->Object));
        }
        before.RemoveWhatAppeared();
        return created ? WarmUpResult::Warmed : WarmUpResult::Skipped;
    }

    void PreloadMap(int map, bool countdownFollows)
    {
        if (!PreloadOnThisDevice())
        {
            return;
        }

        Queue().Clear();
        g_queuedMap = map;
        g_queuedWorld = gMapManager.WorldActive;
        if (!GameConfig::GetInstance().GetPreloadMonsters())
        {
            MU_LOG_INFO(mu::log::Get("data"), "Monster preload: off ([Render] PreloadMonsters=0), map {}", map);
            return;
        }

        Queue().Begin(SpawnList().TypesOf(map));
        LogMapPreload(map, Queue().RunFor(SyncBudgetMs));
        if (!countdownFollows && Queue().HasPending())
        {
            MU_LOG_INFO(mu::log::Get("data"),
                        "Monster preload: map {}, no loading countdown follows: {} types load when they first show",
                        map, Queue().Stats().left);
            Queue().Clear();
        }
    }

    void RunPendingStep(int loadingWorld)
    {
        if (!PreloadOnThisDevice())
        {
            return;
        }

        MonsterPreloadQueue& queue = Queue();
        if (!queue.HasPending() || loadingWorld > MapEntryCountdown)
        {
            return;
        }
        // The map changed some other way since it was queued: its types aren't this map's.
        if (gMapManager.WorldActive != g_queuedWorld)
        {
            queue.Clear();
            return;
        }

        queue.RunOne();
        if (!queue.HasPending())
        {
            MU_LOG_INFO(mu::log::Get("data"), "Monster preload: map {} done over the loading frames:", g_queuedMap);
            LogMapPreload(g_queuedMap, queue.Stats());
        }
    }

    bool IsWarmingUp()
    {
        return g_warmingUp;
    }
}
