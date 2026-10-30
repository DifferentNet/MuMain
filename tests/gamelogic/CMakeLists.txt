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
    NAME test_walk_stop
    SOURCES
        test_walk_stop.cpp
)

mu_add_test(
    NAME test_steer_hold
    SOURCES
        test_steer_hold.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Touch/SteerHold.cpp
)
