# The skill ground marks (Render/Effects/SkillFx.h, Feature::Decals) must leave the classic frame alone
# when they are off: GroundMarks::Add reads the Decals gate before anything else, the Rageful Blow crack
# model is skipped only when Add laid a mark, the hooks pass no rand() and GroundMarks itself never
# calls rand() or touches the terrain light.
foreach(variable MU_GROUND_MARKS_SOURCE MU_GROUND_MARKS_HEADER MU_EFFECT_SOURCE MU_MOVE_HANDLERS_SOURCE)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

function(read_source source out)
    file(READ "${source}" text)
    string(REPLACE "\r" "" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Add's first statement returns unless Decals is on and the map takes marks; MapTakesMarks leaves out
# the water maps (Kalima), Atlans, Icarus and Kanturu's Maya scene, and Update and Draw read it too.
read_source("${MU_GROUND_MARKS_SOURCE}" marks)
string(REGEX MATCH
       "bool Add\\([^)]*\\)[ \t\n]*{[ \t\n]*if \\(!SkillFx::Enabled\\(SkillFx::Feature::Decals\\) \\|\\| !MapTakesMarks\\(\\)\\)"
       addGate "${marks}")
if(NOT addGate)
    message(FATAL_ERROR "${MU_GROUND_MARKS_SOURCE}: Add does not start with the Decals and map gate")
endif()
string(REGEX MATCH "bool MapTakesMarks\\(\\)[ \t\n]*{[^}]*}[^}]*}" mapGate "${marks}")
foreach(needed "IsWaterTerrain\\(\\)" "WD_7ATLANSE" "WD_10HEAVEN" "WD_39KANTURU_3RD && g_Direction\\.m_CKanturu\\.IsMayaScene\\(\\)")
    string(REGEX MATCH "${needed}" found "${mapGate}")
    if(NOT found)
        message(FATAL_ERROR "${MU_GROUND_MARKS_SOURCE}: MapTakesMarks does not check ${needed}")
    endif()
endforeach()
foreach(reader "MapTakesMarks\\(\\) \\? SkillFx::ActiveMarkBudget\\(\\) : 0" "s_pool\\.Count\\(\\) == 0 \\|\\| !MapTakesMarks\\(\\)")
    string(REGEX MATCH "${reader}" found "${marks}")
    if(NOT found)
        message(FATAL_ERROR "${MU_GROUND_MARKS_SOURCE}: Update or Draw does not read MapTakesMarks (${reader})")
    endif()
endforeach()

# No rand() and no terrain light in GroundMarks.
foreach(source "${MU_GROUND_MARKS_SOURCE}" "${MU_GROUND_MARKS_HEADER}")
    read_source("${source}" text)
    foreach(banned "rand\\(" "AddTerrainLight" "SetTerrainLight")
        string(REGEX MATCH "${banned}" found "${text}")
        if(found)
            message(FATAL_ERROR "${source}: ${banned} is not allowed here")
        endif()
    endforeach()
endforeach()

# The hooks: every statement laying a mark passes no rand().
set(hooks 0)
foreach(source "${MU_EFFECT_SOURCE}" "${MU_MOVE_HANDLERS_SOURCE}")
    read_source("${source}" text)
    string(REGEX MATCHALL "[^;{}]*GroundMarks::Add\\([^;{}]*" statements "${text}")
    foreach(statement IN LISTS statements)
        math(EXPR hooks "${hooks} + 1")
        string(FIND "${statement}" "rand" at)
        if(NOT at EQUAL -1)
            message(FATAL_ERROR "${source}: a ground mark hook calls rand:\n${statement}")
        endif()
    endforeach()
endforeach()
if(NOT hooks EQUAL 8)
    message(FATAL_ERROR "expected 8 ground mark hooks, found ${hooks}; update this test")
endif()

# Fails unless every `Add(...Source::<source>` in `source` sits right inside `if (<guard>) {`.
function(check_hook_guard source markSource guard expected)
    read_source("${source}" text)
    set(add "Render::Effects::GroundMarks::Add\\(Render::Effects::GroundMarks::Source::${markSource}")
    string(REGEX MATCHALL "${add}" all "${text}")
    string(REGEX MATCHALL "if \\(${guard}\\)[ \t\n]*{[ \t\n]*${add}" guarded "${text}")
    list(LENGTH all allCount)
    list(LENGTH guarded guardedCount)
    if(NOT allCount EQUAL ${expected} OR NOT guardedCount EQUAL ${expected})
        message(FATAL_ERROR "${source}: ${markSource} marks must be laid only inside if (${guard}) "
                            "(${guardedCount} of ${allCount}, expected ${expected})")
    endif()
endfunction()
# Flame: not under the map's ownerless fires. Ice Storm: one frost a cast, from its first shard.
check_hook_guard("${MU_EFFECT_SOURCE}" "Flame" "o->Owner != nullptr" 2)
check_hook_guard("${MU_MOVE_HANDLERS_SOURCE}" "IceStorm" "o->PKKey == 1" 1)

# The crack model: created in one place, only when Add laid no mark (Add is false while Decals is off).
read_source("${MU_MOVE_HANDLERS_SOURCE}" moves)
string(REGEX MATCHALL "MODEL_SKILL_FURY_STRIKE \\+ 3" crackModels "${moves}")
list(LENGTH crackModels crackModelCount)
if(NOT crackModelCount EQUAL 1)
    message(FATAL_ERROR "${MU_MOVE_HANDLERS_SOURCE}: expected one MODEL_SKILL_FURY_STRIKE + 3, found ${crackModelCount}")
endif()
string(REGEX MATCH
       "const bool crackMark = Render::Effects::GroundMarks::Add\\(Render::Effects::GroundMarks::Source::RagefulBlow[^;]*;[ \t\n]*if \\(!crackMark\\)[ \t\n]*{[ \t\n]*CreateEffectFpsChecked\\(MODEL_SKILL_FURY_STRIKE \\+ 3"
       crackGate "${moves}")
if(NOT crackGate)
    message(FATAL_ERROR "${MU_MOVE_HANDLERS_SOURCE}: the crack model is not behind the crack mark")
endif()
