# The Classic client shows the graphics preset, FXAA and sharpening rows as the Remaster does (ghinzuro,
# 2026-11-17): no options window document hides them behind the removed remaster_graphics binding, and the
# options window binds no such value.
foreach(required_var IN ITEMS MU_OPTION_WINDOW_RML MU_MODERN_OPTION_WINDOW_RML MU_OPTION_WINDOW_SOURCE)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "${required_var} is required")
    endif()
endforeach()

foreach(path IN ITEMS "${MU_OPTION_WINDOW_RML}" "${MU_MODERN_OPTION_WINDOW_RML}" "${MU_OPTION_WINDOW_SOURCE}")
    file(READ "${path}" text)
    string(FIND "${text}" "remaster_graphics" found_at)
    if(NOT found_at EQUAL -1)
        message(FATAL_ERROR "The Classic client's graphics rows are hidden again by remaster_graphics in ${path}")
    endif()
endforeach()
