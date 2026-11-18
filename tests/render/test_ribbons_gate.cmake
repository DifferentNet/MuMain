# The smooth trails (Render/Effects/SkillFx.h, Feature::Ribbons) must leave the classic draw alone
# when they are off. RenderJoints reaches JointRibbon only in a statement that first checks the
# frame's ribbon pieces (0 when the feature is off), read from SkillFx::ActiveRibbonPieces();
# CreateWeaponBlur takes the swing's head slice only when headSlice is set, from the Ribbons gate.
foreach(variable MU_JOINT_SOURCE MU_CHARACTER_SOURCE)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

# Fails unless `source` has a statement with `gateSource`, and every statement naming `use` also names
# `gate`. A statement is the text between two of ; { and }.
function(check_gated source use gate gateSource)
    file(READ "${source}" text)
    string(REPLACE "\r" "" text "${text}")

    string(REGEX MATCHALL "[^;{}]*${gateSource}[^;{}]*" gateStatements "${text}")
    if(NOT gateStatements)
        message(FATAL_ERROR "${source}: no statement reads the gate (${gateSource})")
    endif()

    string(REGEX MATCHALL "[^;{}]*${use}[^;{}]*" statements "${text}")
    if(NOT statements)
        message(FATAL_ERROR "${source}: ${use} is not used; update this test")
    endif()
    foreach(statement IN LISTS statements)
        string(FIND "${statement}" "${gate}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "${source}: ${use} outside its gate (${gate}):\n${statement}")
        endif()
    endforeach()
endfunction()

check_gated("${MU_JOINT_SOURCE}" "JointRibbon::" "ribbonPieces > 0 &&"
            "ribbonPieces = Render::Effects::SkillFx::ActiveRibbonPieces\\(\\)")
check_gated("${MU_CHARACTER_SOURCE}" "HeadSlice\\(" "headSlice"
            "headSlice = o->Type == MODEL_PLAYER && SkillFx::Enabled\\(SkillFx::Feature::Ribbons\\)")
