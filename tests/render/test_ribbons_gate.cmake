# The smooth trails (Render/Effects/SkillFx.h, Feature::Ribbons) must leave the classic draw alone
# when they are off. RenderJoints reaches JointRibbon only in a statement that first checks the
# frame's ribbon pieces (0 when the feature is off), read from SkillFx::ActiveRibbonPieces();
# CreateWeaponBlur takes the swing's head slice only when headSlice is set, from the Ribbons gate.
# RenderEffects draws Twisting Slash's arc band (WheelRibbon) only in a statement that first checks the
# pieces it read the same way and that the ghost is its cast's lead, right before the ghost weapon, and
# only outside the blend-mesh pass (water maps run RenderEffects twice), so once a frame; the
# cast's move, once, marks the lead. CreateJoint, and only it, forgets a slot's kept ribbon pieces, where
# it clears the new joint's tails.
foreach(variable MU_JOINT_SOURCE MU_CHARACTER_SOURCE MU_EFFECT_SOURCE MU_MOVE_HANDLERS_SOURCE)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

# Fails unless `source` has a statement with `gateSource`, and every statement naming `use` also names
# `gate`, except the statements matching the optional fifth argument. A statement is the text between
# two of ; { and }.
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
        if(ARGC GREATER 4 AND statement MATCHES "${ARGV4}")
            continue()
        endif()
        string(FIND "${statement}" "${gate}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "${source}: ${use} outside its gate (${gate}):\n${statement}")
        endif()
    endforeach()
endfunction()

# Fails unless `source` matches `pattern` exactly `count` times. A ; in a match would split the list of
# matches, so the text and the pattern spell it as a word first.
function(check_count source pattern count)
    file(READ "${source}" text)
    string(REPLACE "\r" "" text "${text}")
    string(REPLACE ";" "<semicolon>" text "${text}")
    string(REPLACE ";" "<semicolon>" pattern "${pattern}")
    string(REGEX MATCHALL "${pattern}" found "${text}")
    list(LENGTH found length)
    if(NOT length EQUAL count)
        message(FATAL_ERROR "${source}: ${pattern} found ${length} times, expected ${count}")
    endif()
endfunction()

check_gated("${MU_JOINT_SOURCE}" "JointRibbon::" "ribbonPieces > 0 &&"
            "ribbonPieces = Render::Effects::SkillFx::ActiveRibbonPieces\\(\\)"
            "^[ \t\n]*Render::Effects::JointRibbon::Forget\\(i\\)[ \t\n]*$")
check_count("${MU_JOINT_SOURCE}" "JointRibbon::Forget\\(" 1)
check_count("${MU_JOINT_SOURCE}"
            "std::memset\\(o->Tails, 0, sizeof\\(o->Tails\\)\\);[ \t\n]*Render::Effects::JointRibbon::Forget\\(i\\);" 1)
check_gated("${MU_CHARACTER_SOURCE}" "HeadSlice\\(" "headSlice"
            "headSlice = o->Type == MODEL_PLAYER && SkillFx::Enabled\\(SkillFx::Feature::Ribbons\\)")
check_gated("${MU_EFFECT_SOURCE}" "WheelRibbon::"
            "wheelPieces > 0 && o->Owner && Render::Effects::WheelRibbon::IsLeadGhost(o->m_bySkillSerialNum)"
            "wheelPieces = Render::Effects::SkillFx::ActiveRibbonPieces\\(\\)")
check_count("${MU_EFFECT_SOURCE}" "WheelRibbon::" 2)
check_count("${MU_EFFECT_SOURCE}"
            "case MODEL_SKILL_WHEEL2:[ \t\n]*if \\(!bRenderBlendMesh && wheelPieces > 0 && o->Owner && Render::Effects::WheelRibbon::IsLeadGhost\\(o->m_bySkillSerialNum\\)\\)[ \t\n]*Render::Effects::WheelRibbon::Draw\\(\\*o, wheelPieces\\);[ \t\n]*RenderWheelWeapon\\(o\\);" 1)
check_count("${MU_MOVE_HANDLERS_SOURCE}" "WheelRibbon::LayGhost\\(o->m_bySkillSerialNum\\)" 1)
