cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0091 NEW)

project(PulseSrcShader LANGUAGES CXX)

if(NOT MSVC)
    message(FATAL_ERROR "PulseSrcShader builds with MSVC only.")
endif()

if(CMAKE_SIZEOF_VOID_P GREATER 4)
    message(FATAL_ERROR "L4D2 is 32-bit. Configure CMake with a Win32/x86 platform.")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Perl REQUIRED)

set(SWARM_SDK "${CMAKE_CURRENT_SOURCE_DIR}/sdk/alienswarm-sdk/src")
# Valve stdshaders HLSL headers, vendored - see sdk/valve-stdshaders/README.md.
set(STDSHADERS "${CMAKE_CURRENT_SOURCE_DIR}/sdk/valve-stdshaders")
set(SWARM_LIB_DIR "${SWARM_SDK}/lib/public")
set(DIST_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist/left4dead2/bin")
set(DIST_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist/left4dead2/shaders/fxc")
set(SHADER_INC_DIR "${CMAKE_CURRENT_BINARY_DIR}/inc")

set(FXC_HINTS)
if(DEFINED ENV{WindowsSdkDir} AND DEFINED ENV{WindowsSDKVersion})
    list(APPEND FXC_HINTS "$ENV{WindowsSdkDir}/bin/$ENV{WindowsSDKVersion}/x64")
endif()
file(GLOB WINDOWS_KIT_BINS "C:/Program Files (x86)/Windows Kits/10/bin/10.*")
list(SORT WINDOWS_KIT_BINS COMPARE NATURAL ORDER DESCENDING)
foreach(WINDOWS_KIT_BIN IN LISTS WINDOWS_KIT_BINS)
    list(APPEND FXC_HINTS "${WINDOWS_KIT_BIN}/x64")
endforeach()
list(APPEND FXC_HINTS "C:/Program Files (x86)/Windows Kits/10/bin/x64")
find_program(FXC_EXECUTABLE fxc.exe HINTS ${FXC_HINTS} REQUIRED)

# One VS/PS pair per shader variant. fxc_prep.pl writes filelistgen.txt into its
# cwd, so each pair compiles in a private work dir rather than a shared one.
set(PULSE_SHADER_OUTPUTS)

function(pulse_add_shader_pair)
    cmake_parse_arguments(ARG "" "PREFIX" "EXTRA_SOURCES" ${ARGN})

    set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")
    set(WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaderwork/${ARG_PREFIX}")

    set(COPY_FILES "${SRC_DIR}/${ARG_PREFIX}_vs30.fxc" "${SRC_DIR}/${ARG_PREFIX}_ps30.fxc")
    foreach(EXTRA IN LISTS ARG_EXTRA_SOURCES)
        list(APPEND COPY_FILES "${SRC_DIR}/${EXTRA}")
    endforeach()

    set(INPUTS ${COPY_FILES}
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/vcspack.py"
        "${SWARM_SDK}/devtools/bin/fxc_prep.pl")

    set(OUTPUTS
        "${SHADER_INC_DIR}/${ARG_PREFIX}_vs30.inc"
        "${SHADER_INC_DIR}/${ARG_PREFIX}_ps30.inc"
        "${DIST_SHADER_DIR}/${ARG_PREFIX}_vs30.vcs"
        "${DIST_SHADER_DIR}/${ARG_PREFIX}_ps30.vcs")

    set(STAGE_COMMANDS
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${WORK_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${WORK_DIR}" "${SHADER_INC_DIR}" "${DIST_SHADER_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy ${COPY_FILES} "${WORK_DIR}")

    foreach(STAGE vs30 ps30)
        list(APPEND STAGE_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E chdir "${WORK_DIR}" "${PERL_EXECUTABLE}"
                    "${SWARM_SDK}/devtools/bin/fxc_prep.pl"
                    "${ARG_PREFIX}_${STAGE}.fxc-----${ARG_PREFIX}_${STAGE}")
    endforeach()

    foreach(STAGE vs30 ps30)
        list(APPEND STAGE_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E chdir "${WORK_DIR}" "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/vcspack.py" filelistgen.txt
                    --shader "${ARG_PREFIX}_${STAGE}"
                    --workdir .
                    --out "${DIST_SHADER_DIR}/${ARG_PREFIX}_${STAGE}.vcs"
                    --fxc "${FXC_EXECUTABLE}"
                    --include "${STDSHADERS}"
            COMMAND "${CMAKE_COMMAND}" -E copy
                    "${WORK_DIR}/fxctmp9_tmp/${ARG_PREFIX}_${STAGE}.inc"
                    "${SHADER_INC_DIR}/${ARG_PREFIX}_${STAGE}.inc")
    endforeach()

    add_custom_command(
        OUTPUT ${OUTPUTS}
        ${STAGE_COMMANDS}
        DEPENDS ${INPUTS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        VERBATIM
    )

    set(PULSE_SHADER_OUTPUTS ${PULSE_SHADER_OUTPUTS} ${OUTPUTS} PARENT_SCOPE)
endfunction()

pulse_add_shader_pair(PREFIX pulse_pbr       EXTRA_SOURCES openpbr_common_ps3_x.h pulse_outline_vs30.h)
pulse_add_shader_pair(PREFIX pulse_girlsfrontline EXTRA_SOURCES openpbr_common_ps3_x.h pulse_outline_vs30.h)
pulse_add_shader_pair(PREFIX pulse_npr        EXTRA_SOURCES npr_common_ps3_x.h pulse_outline_vs30.h)
pulse_add_shader_pair(PREFIX pulse_umamusume EXTRA_SOURCES npr_common_ps3_x.h pulse_outline_vs30.h)
pulse_add_shader_pair(PREFIX pulse_tooneye   EXTRA_SOURCES npr_common_ps3_x.h)

add_custom_target(pulse_shaders DEPENDS ${PULSE_SHADER_OUTPUTS})

add_library(game_shader_generic_pulse SHARED
    src/shaders/pbr_dx9.cpp
    src/shaders/girlsfrontline_dx9.cpp
    src/shaders/pulse_shader_convars.cpp
    src/shaders/npr_dx9.cpp
    src/shaders/umamusume_dx9.cpp
    src/shaders/tooneye_dx9.cpp
    src/basevsshader_impl.cpp
    src/pulse_shaderdll.cpp
    src/pulse_shaderlib_cvar.cpp
    src/log.cpp
    "${SWARM_SDK}/materialsystem/shaderlib/BaseShader.cpp"
)

add_dependencies(game_shader_generic_pulse pulse_shaders)

target_compile_definitions(game_shader_generic_pulse PRIVATE
    WIN32
    _WIN32
    COMPILER_MSVC
    COMPILER_MSVC32
)

target_compile_options(game_shader_generic_pulse PRIVATE /EHsc /W3)

target_include_directories(game_shader_generic_pulse PRIVATE
    src/sdk_override
    "${SWARM_SDK}/public"
    "${SWARM_SDK}/public/tier0"
    "${SWARM_SDK}/public/tier1"
    "${SWARM_SDK}/common"
    "${SWARM_SDK}/materialsystem"
    "${SWARM_SDK}/public/materialsystem"
    "${SWARM_SDK}/materialsystem/stdshaders"
    "${SWARM_SDK}/materialsystem/shaderlib"
    "${SHADER_INC_DIR}"
    src/shaders
    src
)

target_link_libraries(game_shader_generic_pulse PRIVATE
    "${SWARM_LIB_DIR}/tier0.lib"
    "${SWARM_LIB_DIR}/tier1.lib"
    "${SWARM_LIB_DIR}/mathlib.lib"
    "${SWARM_LIB_DIR}/vstdlib.lib"
    "${SWARM_LIB_DIR}/interfaces.lib"
    legacy_stdio_definitions.lib
)

set_target_properties(game_shader_generic_pulse PROPERTIES
    PREFIX ""
    OUTPUT_NAME game_shader_generic_pulse
    MSVC_RUNTIME_LIBRARY "MultiThreaded"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${DIST_BIN_DIR}>"
    LIBRARY_OUTPUT_DIRECTORY "$<1:${DIST_BIN_DIR}>"
    ARCHIVE_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}>"
)

add_custom_command(TARGET game_shader_generic_pulse POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E touch "${DIST_BIN_DIR}/game_shader_generic_pulse"
    VERBATIM
)
