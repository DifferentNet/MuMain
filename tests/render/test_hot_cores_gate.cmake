# The hot cores (Render/PostProcessing/HotCores.h, SkillFx::Feature::Cores) must leave the Classic client
# and the web alone and the plain composite unchanged when they are off: the twin shader is created only
# on the desktop and has no WGSL, DrawComposite takes the twin only behind coreStrength > 0, the shader's
# thresholds are HotCores.h's and none of the new code calls rand().
foreach(variable MU_RENDERER_SOURCE MU_POST_PROCESSING_SOURCE MU_HOT_CORES_HEADER MU_SHADER_SOURCE_DIR
                 MU_ROOT_CMAKELISTS)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

function(read_source source out)
    file(READ "${source}" text)
    string(REPLACE "\r" "" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# The last position of `needle` in `text` before `limit` (-1: none).
function(find_last_before text needle limit out)
    string(SUBSTRING "${text}" 0 ${limit} head)
    string(FIND "${head}" "${needle}" at REVERSE)
    set(${out} ${at} PARENT_SCOPE)
endfunction()

# The twin's createShader sits inside #ifndef __EMSCRIPTEN__ (no #endif or #else between them).
read_source("${MU_RENDERER_SOURCE}" renderer)
string(FIND "${renderer}" "createShader(\"post_composite_cores\"" twinCreate)
if(twinCreate EQUAL -1)
    message(FATAL_ERROR "${MU_RENDERER_SOURCE}: post_composite_cores is never created")
endif()
find_last_before("${renderer}" "#ifndef __EMSCRIPTEN__" ${twinCreate} desktopOnly)
find_last_before("${renderer}" "#endif" ${twinCreate} lastEndif)
find_last_before("${renderer}" "#else" ${twinCreate} lastElse)
if(desktopOnly EQUAL -1 OR desktopOnly LESS lastEndif OR desktopOnly LESS lastElse)
    message(FATAL_ERROR
            "${MU_RENDERER_SOURCE}: post_composite_cores must be created inside #ifndef __EMSCRIPTEN__")
endif()
string(REGEX MATCHALL "createShader\\(\"post_composite_cores\"" twinCreates "${renderer}")
list(LENGTH twinCreates twinCreateCount)
if(NOT twinCreateCount EQUAL 1)
    message(FATAL_ERROR
            "${MU_RENDERER_SOURCE}: expected one post_composite_cores createShader, found ${twinCreateCount}")
endif()
# Scene, bloom source, bloom and grading (4 samplers), no storage buffer, CompositeUniforms (1 uniform).
set(gap "[ \t\n]*")
set(twinCountsPattern "createShader\\(\"post_composite_cores\",${gap}\"frag\",${gap}SDL_GPU_SHADERSTAGE_FRAGMENT,")
string(APPEND twinCountsPattern "${gap}4,${gap}0,${gap}1,")
string(REGEX MATCH "${twinCountsPattern}" twinCounts "${renderer}")
if(NOT twinCounts)
    message(FATAL_ERROR "${MU_RENDERER_SOURCE}: post_composite_cores must be created with 4 samplers, 0 storage "
                        "buffers and 1 uniform buffer")
endif()
# The frame's composite gets the live core strength from the SkillFx gate (0 on the web and in Classic).
string(FIND "${renderer}" "look.coreStrength = Render::Effects::SkillFx::ActiveCoreStrength();" liveStrength)
if(liveStrength EQUAL -1)
    message(FATAL_ERROR
            "${MU_RENDERER_SOURCE}: the composite's core strength must come from SkillFx::ActiveCoreStrength")
endif()

# The twin is desktop-only for the web build and has no WGSL.
read_source("${MU_ROOT_CMAKELISTS}" rootLists)
string(REGEX MATCH "set\\(MU_DESKTOP_ONLY_SHADER_STAGE_NAMES[^)]*\\)" desktopList "${rootLists}")
string(FIND "${desktopList}" "post_composite_cores.frag" listed)
if(listed EQUAL -1)
    message(FATAL_ERROR
            "${MU_ROOT_CMAKELISTS}: post_composite_cores.frag must be in MU_DESKTOP_ONLY_SHADER_STAGE_NAMES")
endif()
if(EXISTS "${MU_SHADER_SOURCE_DIR}/compiled/post_composite_cores.frag.wgsl")
    message(FATAL_ERROR "post_composite_cores.frag must have no WGSL: the browser never loads it")
endif()
foreach(format spv msl dxil)
    if(NOT EXISTS "${MU_SHADER_SOURCE_DIR}/compiled/post_composite_cores.frag.${format}")
        message(FATAL_ERROR "post_composite_cores.frag.${format} is missing")
    endif()
endforeach()

# DrawComposite takes the twin only when the core strength is above 0 (and the scene view has bloom),
# binds it only inside if (cores), returns before the plain bind and sends the strength only then.
read_source("${MU_POST_PROCESSING_SOURCE}" post)
string(FIND "${post}"
       "const bool cores = sceneShown && bloom && look.coreStrength > 0.0f && m_compositeCoresPipeline;" coresGate)
if(coresGate EQUAL -1)
    message(FATAL_ERROR "${MU_POST_PROCESSING_SOURCE}: DrawComposite must gate the twin behind coreStrength > 0")
endif()
string(REGEX MATCHALL "SDL_BindGPUGraphicsPipeline\\(pass, m_compositeCoresPipeline\\)" twinBinds "${post}")
list(LENGTH twinBinds twinBindCount)
string(FIND "${post}" "if (cores)" coresBranch)
string(FIND "${post}" "SDL_BindGPUGraphicsPipeline(pass, m_compositeCoresPipeline)" twinBind)
string(FIND "${post}" "SDL_BindGPUGraphicsPipeline(pass, m_compositePipeline)" plainBind)
if(NOT twinBindCount EQUAL 1 OR coresBranch EQUAL -1 OR twinBind LESS coresBranch OR plainBind LESS twinBind)
    message(FATAL_ERROR "${MU_POST_PROCESSING_SOURCE}: the twin must be bound once, inside if (cores), "
                        "before the plain composite")
endif()
math(EXPR betweenLength "${plainBind} - ${twinBind}")
string(SUBSTRING "${post}" ${twinBind} ${betweenLength} twinBranch)
string(FIND "${twinBranch}" "return;" returns)
if(returns EQUAL -1)
    message(FATAL_ERROR "${MU_POST_PROCESSING_SOURCE}: the twin branch must return before the plain composite bind")
endif()
string(FIND "${post}" "cores ? std::clamp(look.coreStrength, 0.0f, 1.0f) : 0.0f" strengthSent)
if(strengthSent EQUAL -1)
    message(FATAL_ERROR "${MU_POST_PROCESSING_SOURCE}: the plain composite must get core strength 0")
endif()

# The shader's thresholds are HotCores.h's.
read_source("${MU_HOT_CORES_HEADER}" header)
read_source("${MU_SHADER_SOURCE_DIR}/post_composite.frag.hlsl" composite)
foreach(constant kCoreMeanLow kCoreMaxLow)
    string(REGEX MATCH "${constant} = ([0-9.]+)f;" headerMatch "${header}")
    set(headerValue "${CMAKE_MATCH_1}")
    string(REGEX MATCH "static const float ${constant} = ([0-9.]+);" shaderMatch "${composite}")
    set(shaderValue "${CMAKE_MATCH_1}")
    if(headerValue STREQUAL "" OR NOT headerValue STREQUAL shaderValue)
        message(FATAL_ERROR "${constant}: HotCores.h has '${headerValue}', post_composite.frag.hlsl '${shaderValue}'")
    endif()
endforeach()

# The twin is the base shader with COMPOSITE_CORES, so the base's blobs stay as they were.
read_source("${MU_SHADER_SOURCE_DIR}/post_composite_cores.frag.hlsl" twin)
string(REGEX MATCH "#define COMPOSITE_CORES 1[ \t\n]*#include \"post_composite\\.frag\\.hlsl\"" twinInclude "${twin}")
if(NOT twinInclude)
    message(FATAL_ERROR
            "post_composite_cores.frag.hlsl must define COMPOSITE_CORES and include post_composite.frag.hlsl")
endif()

# No rand() in the new code.
foreach(text_variable header composite twin)
    string(REGEX MATCH "rand\\(" found "${${text_variable}}")
    if(found)
        message(FATAL_ERROR "${text_variable}: rand() is not allowed here")
    endif()
endforeach()
