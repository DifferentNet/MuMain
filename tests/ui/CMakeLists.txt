add_test(
    NAME rml_contract_guard_tests
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_rml_contracts.py"
)

# The mistakes RmlUi only reports at runtime, or not at all, in every shipped document and stylesheet.
add_test(
    NAME rml_rcss_syntax
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/check_rml_rcss_syntax.py"
)

add_test(
    NAME rml_preview
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_rml_preview.py"
            "${CMAKE_SOURCE_DIR}"
)

mu_add_test(
    NAME rml_image_crop_tests
    SOURCES test_rml_image_crop.cpp
    LINK_LIBS RmlUi::Core
)

# The pad's button names put into the help's texts.
mu_add_test(
    NAME pad_button_names_tests
    SOURCES
        test_pad_button_names.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/PadButtonNames.cpp
)

mu_add_test(
    NAME guard_guild_lists_tests
    SOURCES
        test_guard_guild_lists.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Combat/GuardGuildLists.cpp
)

mu_add_test(
    NAME jewel_bank_names_tests
    SOURCES
        test_jewel_bank_names.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Inventory/JewelBankNames.cpp
)

mu_add_test(
    NAME chat_history_tests
    SOURCES
        test_chat_history.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/ChatHistory.cpp
)

mu_add_test(
    NAME chat_fade_tests
    SOURCES
        test_chat_fade.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/ChatFade.cpp
)

# The high-water end of the chat plate pool (UI/Chat/ChatPlateBound.h).
mu_add_test(
    NAME chat_plate_bound_tests
    SOURCES
        test_chat_plate_bound.cpp
)

mu_add_test(
    NAME zen_amount_tests
    SOURCES
        test_zen_amount.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Inventory/ZenAmount.cpp
)

mu_add_test(
    NAME party_invite_command_tests
    SOURCES
        test_party_invite_command.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/PartyInviteCommand.cpp
)

mu_add_test(
    NAME skill_resources_tests
    SOURCES
        test_skill_resources.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Skills/SkillResources.cpp
)

mu_add_test(
    NAME sale_refusal_tests
    SOURCES
        test_sale_refusal.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/SaleRefusal.cpp
)

mu_add_test(
    NAME jewel_unmix_selection_tests
    SOURCES
        test_jewel_unmix_selection.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/JewelUnmixSelection.cpp
)

mu_add_test(
    NAME buy_option_selection_tests
    SOURCES
        test_buy_option_selection.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameShop/BuyOptionSelection.cpp
)

mu_add_test(
    NAME storage_item_selection_tests
    SOURCES
        test_storage_item_selection.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameShop/StorageItemSelection.cpp
)

mu_add_test(
    NAME item_grid_geometry_tests
    SOURCES
        test_item_grid_geometry.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Inventory/ItemGridGeometry.cpp
)

mu_add_test(
    NAME tooltip_placement_tests
    SOURCES
        test_tooltip_placement.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/RmlBridge/RmlTooltipPlacement.cpp
)

mu_add_test(
    NAME socket_list_selection_tests
    SOURCES
        test_socket_list_selection.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Inventory/SocketListSelection.cpp
)

mu_add_test(
    NAME experience_text_tests
    SOURCES test_experience_text.cpp
    LINK_LIBS MuClient
)

# Numbers in chat and dialog lines grouped by thousands (round 8, D5).
mu_add_test(
    NAME grouped_number_text_tests
    SOURCES
        test_grouped_number_text.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Text/GroupedNumberText.cpp
)

mu_add_test(
    NAME help_pages_tests
    SOURCES test_help_pages.cpp
    LINK_LIBS MuClient
)

# The options window's tuning rows of the Enhanced look, and their markup in both bundled themes.
mu_add_test(
    NAME tuning_rows_tests
    SOURCES test_tuning_rows.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(tuning_rows_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
)

mu_add_test(
    NAME guild_mark_palette_tests
    SOURCES
        test_guild_mark_palette.cpp
        ${CMAKE_SOURCE_DIR}/src/source/Guild/GuildMarkPalette.cpp
)

mu_add_test(
    NAME rml_premultiply_tests
    SOURCES
        test_rml_premultiply.cpp
        ${CMAKE_SOURCE_DIR}/src/source/Render/RmlUi/RmlPremultiply.cpp
)

mu_add_test(
    NAME mini_map_layout_tests
    SOURCES
        test_mini_map_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/MiniMapLayout.cpp
)

mu_add_test(
    NAME pad_keyboard_layout_tests
    SOURCES
        test_pad_keyboard_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/PadKeyboardLayout.cpp
)

mu_add_test(
    NAME button_size_tests
    SOURCES
        test_button_size.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Mobile/ButtonSize.cpp
)

mu_add_test(
    NAME big_resize_tests
    SOURCES
        test_big_resize.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Mobile/BigResize.cpp
)

mu_add_test(
    NAME skill_arrange_tests
    SOURCES
        test_skill_arrange.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Mobile/SkillArrange.cpp
)

mu_add_test(
    NAME follow_through_tests
    SOURCES
        test_follow_through.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Mobile/FollowThrough.cpp
)

mu_add_test(
    NAME potion_kind_tests
    SOURCES
        test_potion_kind.cpp
)

# The phone hero frame's "+": stat points or master points to spend (UI/Mobile/HeroFrame.h).
mu_add_test(
    NAME hero_points_alert_tests
    SOURCES test_hero_points_alert.cpp
    LINK_LIBS RmlUi::Core
)

mu_add_test(
    NAME touch_target_tests
    SOURCES
        test_touch_target.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Mobile/TouchTarget.cpp
)

mu_add_test(
    NAME stat_points_menu_tests
    SOURCES
        test_stat_points_menu.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Character/StatPointsMenu.cpp
)

mu_add_test(
    NAME rml_stacking_order_tests
    SOURCES
        test_rml_stacking_order.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/RmlBridge/RmlStackingOrder.cpp
)
target_compile_definitions(rml_stacking_order_tests PRIVATE
    MU_CLIENT_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src/source"
)

mu_add_test(
    NAME party_list_layout_tests
    SOURCES
        test_party_list_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Party/PartyListLayout.cpp
)

mu_add_test(
    NAME text_search_tests
    SOURCES
        test_text_search.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Widgets/TextSearch.cpp
)

add_test(
    NAME no_legacy_text_measurement
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}/src/source
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_no_legacy_text_measurement.cmake
)

add_test(
    NAME inventory_window_captures_hover
    COMMAND ${CMAKE_COMMAND}
        -DMU_INVENTORY_SOURCE=${CMAKE_SOURCE_DIR}/src/source/UI/Inventory/MyInventory.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_inventory_mouse_capture.cmake
)

add_test(
    NAME npc_name_assets
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_npc_name_assets.py"
            "${CMAKE_SOURCE_DIR}"
)

mu_add_test(
    NAME npc_name_storage_tests
    SOURCES
        test_npc_name_storage.cpp
        ${CMAKE_SOURCE_DIR}/src/source/Core/Text/WideString.cpp
)

mu_add_test(
    NAME monster_health_bar_tests
    SOURCES
        test_monster_health_bar.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Combat/MonsterHealthBar.cpp
)

mu_add_test(
    NAME ui_scaling_tests
    SOURCES
        test_ui_scaling.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME mu_helper_tests
    SOURCES
        test_mu_helper.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME mu_helper_extra_items_tests
    SOURCES
        test_mu_helper_extra_items.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

add_test(
    NAME display_settings_contract
    COMMAND "${CMAKE_COMMAND}"
        "-DMU_CONFIG_CONSTANTS=${CMAKE_SOURCE_DIR}/src/source/Data/GameConfig/GameConfigConstants.h"
        "-DMU_CONFIG_HEADER=${CMAKE_SOURCE_DIR}/src/source/Data/GameConfig/GameConfig.h"
        "-DMU_CONFIG_SOURCE=${CMAKE_SOURCE_DIR}/src/source/Data/GameConfig/GameConfig.cpp"
        "-DMU_LOOK_SETTING_SOURCE=${CMAKE_SOURCE_DIR}/src/source/Render/Look/LookPresetSetting.cpp"
        "-DMU_WINMAIN_SOURCE=${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp"
        "-DMU_CONSOLE_SOURCE=${CMAKE_SOURCE_DIR}/src/source/Core/Utilities/Log/muConsoleDebug.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/test_display_settings_contract.cmake"
)

add_test(
    NAME classic_graphics_options_contract
    COMMAND "${CMAKE_COMMAND}"
        "-DMU_OPTION_WINDOW_RML=${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi/option_window.rml"
        "-DMU_MODERN_OPTION_WINDOW_RML=${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi/themes/modern/option_window.rml"
        "-DMU_OPTION_WINDOW_SOURCE=${CMAKE_SOURCE_DIR}/src/source/UI/Options/OptionWindow.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/test_classic_graphics_options_contract.cmake"
)

mu_add_test(
    NAME character_name_lifecycle_tests
    SOURCES test_character_name_lifecycle.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME window_geometry_tests
    SOURCES
        test_window_geometry.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Core/WindowGeometry.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME master_skill_tree_layout_tests
    SOURCES
        test_master_skill_tree_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/Skills/MasterSkillTreeLayout.cpp
)

mu_add_test(
    NAME skill_icon_atlas_tests
    SOURCES
        test_skill_icon_atlas.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/Skills/SkillIconAtlas.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/Skills/MasterSkillTreeLayout.cpp
)

mu_add_test(
    NAME rml_draggable_tests
    SOURCES test_rml_draggable.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_touch_scroll_tests
    SOURCES test_rml_touch_scroll.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME window_placement_layout_tests
    SOURCES test_window_placement_layout.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(window_placement_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi")

mu_add_test(
    NAME rml_model_binder_tests
    SOURCES test_rml_model_binder.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_themed_view_tests
    SOURCES
        test_rml_themed_view.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME world_label_layer_tests
    SOURCES
        test_world_label_layer.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_counter_scale_tests
    SOURCES
        test_rml_counter_scale.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_slot_placement_tests
    SOURCES
        test_rml_slot_placement.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_party_trade_layout_tests
    SOURCES
        test_rml_party_trade_layout.cpp
        RmlLayoutFixture.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(rml_party_trade_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
    MU_FONT_DIR="${CMAKE_SOURCE_DIR}/src/bin/fonts"
    MU_LOCALIZATION_DIR="${CMAKE_SOURCE_DIR}/src/Localization"
)

mu_add_test(
    NAME item_grid_px_tests
    SOURCES
        test_item_grid_px.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_scale_inputs_tests
    SOURCES
        test_rml_scale_inputs.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_runtime_scale_tests
    SOURCES
        test_rml_runtime_scale.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME theme_file_interface_tests
    SOURCES
        test_theme_file_interface.cpp
        test_theme_inheritance.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(theme_file_interface_tests PRIVATE
    MU_FONT_DIR="${CMAKE_SOURCE_DIR}/src/bin/fonts"
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
)

mu_add_test(
    NAME rml_friend_window_identity_tests
    SOURCES test_rml_friend_window_identity.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME generic_dialog_lifetime_tests
    SOURCES
        test_generic_dialog_lifetime.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_document_scene_tests
    SOURCES
        test_rml_document_scene.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

add_executable(rml_text_layout_tests
    test_rml_text_layout.cpp
    RmlTextLayoutAudit.cpp
    RmlLayoutFixture.cpp
    ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
)
target_link_libraries(rml_text_layout_tests PRIVATE mu_test_main MuClient)
target_include_directories(rml_text_layout_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/source)
if(MSVC)
    target_compile_options(rml_text_layout_tests PRIVATE /utf-8)
endif()
add_test(
    NAME rml_text_layout_baseline
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_rml_text_layout.py"
        --repo "${CMAKE_SOURCE_DIR}"
        --generated "${CMAKE_BINARY_DIR}/Generated"
        --executable "$<TARGET_FILE:rml_text_layout_tests>"
        --output "${CMAKE_BINARY_DIR}/text-layout-baseline"
)
target_compile_definitions(rml_text_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
    MU_FONT_DIR="${CMAKE_SOURCE_DIR}/src/bin/fonts"
)

mu_add_test(
    NAME rml_styling_regression_tests
    SOURCES
        test_rml_styling_regressions.cpp
        RmlLayoutFixture.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(rml_styling_regression_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
    MU_FONT_DIR="${CMAKE_SOURCE_DIR}/src/bin/fonts"
)

mu_add_test(
    NAME window_sort_memo_tests
    SOURCES test_window_sort_memo.cpp
)

mu_add_test(
    NAME compact_number_tests
    SOURCES
        test_compact_number.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Mobile/CompactNumber.cpp
)

mu_add_test(
    NAME shop_tap_to_buy_tests
    SOURCES
        test_shop_tap_to_buy.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/NPCs/ShopTapToBuy.cpp
)

mu_add_test(
    NAME phone_hud_fit_tests
    SOURCES
        test_phone_hud_fit.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Mobile/PhoneHudFit.cpp
)

mu_add_test(
    NAME desktop_tip_tests
    SOURCES
        test_desktop_tip.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Mobile/DesktopTip.cpp
)

mu_add_test(
    NAME pad_target_pick_tests
    SOURCES
        test_pad_target_pick.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/PadTargetPick.cpp
)

# The one-time pad hints' bits in config.ini, and when the small screen hint applies.
mu_add_test(
    NAME pad_hints_tests
    SOURCES
        test_pad_hints.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/PadHintBits.cpp
)

# mu_swprintf into an array cuts a long text instead of writing past the array (round 10, B1).
mu_add_test(
    NAME bounded_wide_format_tests
    SOURCES test_bounded_wide_format.cpp
)

# The old Data\Macro.txt read into the chat macros, one slot per line (round 10, Q2).
mu_add_test(
    NAME chat_macro_file_tests
    SOURCES
        test_chat_macro_file.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/ChatMacroFile.cpp
)

# The zen a sale, a repair or a death changed, summed into one line (round 11, Q3 and Q8).
mu_add_test(
    NAME zen_change_window_tests
    SOURCES
        test_zen_change_window.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/ZenChangeWindow.cpp
)

# The experience bar's pace per hour and the time to the next level (round 11, Q9).
mu_add_test(
    NAME experience_pace_tests
    SOURCES
        test_experience_pace.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/ExperiencePace.cpp
)

# The party members the big map marks and the party list refresh rhythm (round 11, Q4 and Q5).
mu_add_test(
    NAME minimap_party_dots_tests
    SOURCES
        test_minimap_party_dots.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/MiniMapPartyDots.cpp
)

# What a jewel did to the item it was used on (round 11, Q2).
mu_add_test(
    NAME jewel_upgrade_result_tests
    SOURCES
        test_jewel_upgrade_result.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/JewelUpgradeResult.cpp
)

# Round 12: chaos machine refusals, buff end / bag room / low potion lines (UI/Chat, UI/HUD, GameLogic/Items).
mu_add_test(
    NAME round12_status_lines_tests
    SOURCES
        test_round12_status_lines.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/CraftingRefusal.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/BuffEndNotices.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/BagRoomWarning.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/LowStockWarning.cpp
)

# Round 12: emote keywords in chat (case folding, whole words for short ones).
mu_add_test(
    NAME emote_keywords_tests
    SOURCES
        test_emote_keywords.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/EmoteKeywords.cpp
)

# Pad round 6: quick chat phrases, the token on the wire and the reader's own text.
mu_add_test(
    NAME chat_phrases_tests
    SOURCES
        test_chat_phrases.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/ChatPhrases.cpp
)

# Round 13: trade offer change, pet life, new skill, warp unlocks, character preselect, short of mana gate.
mu_add_test(
    NAME round13_lines_tests
    SOURCES
        test_round13_lines.cpp
        ${CMAKE_SOURCE_DIR}/src/source/Scenes/CharacterPreselect.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/PetLifeWarning.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/SeenSkills.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/WarpUnlocks.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Inventory/TradeOfferChange.cpp
)

# Round 13: the J window's client commands have descriptions in the player's language.
mu_add_test(
    NAME round13_client_commands_tests
    SOURCES test_round13_client_commands.cpp
    LINK_LIBS MuClient
)

# Round 14: duel start answers and "duel canceled" once, refused invitation lines, MU Helper run summary.
mu_add_test(
    NAME round14_logic_tests
    SOURCES
        test_round14_logic.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Combat/DuelReplies.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/DeclinedInviteLines.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/HelperRunSummary.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Text/GroupedNumberText.cpp
)

# Round 15: a chat line sent too soon waits for the cooldown, the inviter's class and guild line.
mu_add_test(
    NAME round15_logic_tests
    SOURCES
        test_round15_logic.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/ChatSendGate.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Chat/InviterDetailText.cpp
)

# Round 20: the warp list's "you are here" / "you died here" rows, a trade's waiting lines dropped on close.
mu_add_test(
    NAME round20_logic_tests
    SOURCES
        test_round20_logic.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/WarpListMarks.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Inventory/TradeOfferChange.cpp
)
