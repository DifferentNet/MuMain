mu_add_test(
    NAME test_boid_steering
    SOURCES
        test_boid_steering.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_head_movement
    SOURCES
        test_head_movement.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_earthquake_falloff
    SOURCES
        test_earthquake_falloff.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_reference_frame_steps
    SOURCES
        test_reference_frame_steps.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_swing_cadence
    SOURCES
        test_swing_cadence.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_direction_turning
    SOURCES
        test_direction_turning.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_personal_shop_currency
    SOURCES
        test_personal_shop_currency.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_item_combat_power
    SOURCES
        test_item_combat_power.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_path_end_nodes
    SOURCES
        test_path_end_nodes.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_helper_fence
    SOURCES
        test_helper_fence.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

mu_add_test(
    NAME test_walk_stop
    SOURCES
        test_walk_stop.cpp
)

mu_add_test(
    NAME test_walk_give_up
    SOURCES
        test_walk_give_up.cpp
)

mu_add_test(
    NAME test_pickup_set_aside
    SOURCES
        test_pickup_set_aside.cpp
)

mu_add_test(
    NAME test_target_priority
    SOURCES
        test_target_priority.cpp
)

# The pad's right stick picks the target that lies the way it leans (pad round 3, 2026-11-13).
mu_add_test(
    NAME test_direction_pick
    SOURCES
        test_direction_pick.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Touch/DirectionPick.cpp
)

# The foe the player chose passes on as it dies, the way it lay (pad round 5, 2026-11-17).
mu_add_test(
    NAME test_target_pass_on
    SOURCES
        test_target_pass_on.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Touch/TargetPassOn.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Touch/DirectionPick.cpp
)

# Where the pad's and touch's area skills aim: the pack's centre, the stick's line (pad round 6).
mu_add_test(
    NAME test_area_aim
    SOURCES
        test_area_aim.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Touch/AreaAim.cpp
)

mu_add_test(
    NAME test_steer_hold
    SOURCES
        test_steer_hold.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Touch/SteerHold.cpp
)

# The server's "#UPF|<key>|<level>" line after an enchant to +13..+15 (Render/Items/EnchantFx).
mu_add_test(
    NAME test_enchant_flash_line
    SOURCES
        test_enchant_flash_line.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/EnchantFlashLine.cpp
)

# The server's "#GRS|str|agi|vit|ene|cmd" line with the own Grand stat points (GameLogic/Items/GrandStats).
mu_add_test(
    NAME test_grand_stats_line
    SOURCES
        test_grand_stats_line.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/GrandStatsLine.cpp
)

# The character window's "Skill Damage: N %" for the Dark Knight, Magic Gladiator and Dark Lord
# (GameLogic/Skills/SkillDamagePercent.h).
mu_add_test(
    NAME test_skill_damage_percent
    SOURCES
        test_skill_damage_percent.cpp
)

# The loot highlight filter's rules (GameLogic/Items/LootFilter).
mu_add_test(
    NAME test_loot_filter
    SOURCES
        test_loot_filter.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/LootFilter.cpp
)

# The server's "#UPL|<name>|<levels>" line: other characters' exact +14/+15 (GameLogic/Items/AncientLook, EnchantFx).
mu_add_test(
    NAME test_exact_level_line
    SOURCES
        test_exact_level_line.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/ExactLevelLine.cpp
)

mu_add_test(
    NAME test_fortress_targets
    SOURCES
        test_fortress_targets.cpp
    LINK_LIBS
        MuClient
)

# Who joined and who left between two party lists, for the chat lines (round 10, Q5).
mu_add_test(
    NAME test_party_roster_diff
    SOURCES
        test_party_roster_diff.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Social/PartyRosterDiff.cpp
)

# Junk marks: which items may be junk, the fingerprint a mark keeps and the marks themselves (pad round 6, B7).
mu_add_test(
    NAME test_junk_marks
    SOURCES
        test_junk_marks.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/JunkMarks.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/JunkMarkSet.cpp
)

# The quick equip offer: which pick-ups are offered and how long (pad round 6, B3).
mu_add_test(
    NAME test_equip_offer
    SOURCES
        test_equip_offer.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/EquipOffer.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/EquipOfferTracker.cpp
)

# Buff countdowns are cleared on relog / character switch and restart with a sent time (round 15, B8).
mu_add_test(
    NAME test_buff_time_control
    SOURCES
        test_buff_time_control.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS
        MuClient
)

# Blood Castle / Devil Square level bands shared by the entry windows and the ticket tooltips (round 17, Q5).
mu_add_test(
    NAME test_entry_levels
    SOURCES
        test_entry_levels.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Events/EntryLevels.cpp
)

# MU Helper's crafting materials option: feathers, the Spirit and the Fenrir materials leave the quest option (2026-11-20).
mu_add_test(
    NAME test_helper_craft_pick
    SOURCES
        test_helper_craft_pick.cpp
    LINK_LIBS
        MuClient
)
