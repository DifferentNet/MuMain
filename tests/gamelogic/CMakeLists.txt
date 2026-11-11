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
