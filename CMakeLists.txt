cmake_minimum_required(VERSION 3.21)

project(OptiCraftHeritage
    VERSION 1.7.3
    DESCRIPTION "OptiCraft Heritage - heavily modified clean-room C++ implementation (Linux x86_64 only)"
    LANGUAGES C CXX
)

if(NOT UNIX OR APPLE)
    message(FATAL_ERROR "This trimmed CMakeLists.txt only supports desktop Linux (x86_64). "
                         "Use the upstream project if you need Windows/macOS/PS2/Wii.")
endif()

# StrictMathCompat reproduces the operation ordering of Java StrictMath/fdlibm.
# Contracting a multiply+add into FMA can change the last bit, so keep
# contraction disabled for this TU only; the rest of the renderer/world code
# keeps its normal optimization profile.
file(GLOB FDLIBM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/java/fdlibm/*.c"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/java/StrictMathCompat.cpp"
    ${FDLIBM_SOURCES}
    PROPERTIES COMPILE_OPTIONS "-ffp-contract=off"
)

enable_testing()

function(opticraft_add_optional_test target test_source)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_source}")
        message(STATUS "${target} skipped: ${test_source} is not available in this source package")
        return()
    endif()

    add_executable(${target}
        "${test_source}"
        ${ARGN}
    )
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# Only tests that exercise platform-neutral logic. Anything that pulls in
# PS2/Wii/PC_LEGACY sources or defines PS2_PLATFORM/PLATFORM_PC_LEGACY was
# dropped along with those build targets.
opticraft_add_optional_test(IntCacheTests
    tests/IntCacheTests.cpp
    src/net/minecraft/src/IntCache.cpp
)

opticraft_add_optional_test(MathHelperFloorTests
    tests/MathHelperFloorTests.cpp
)

opticraft_add_optional_test(StructureReservationTests
    tests/StructureReservationTests.cpp
    src/net/minecraft/src/StructureBoundingBox.cpp
)

opticraft_add_optional_test(JavaHashSetTests
    tests/JavaHashSetTests.cpp
    src/net/minecraft/src/ChunkCoordIntPair.cpp
)
if(TARGET JavaHashSetTests)
    target_compile_options(JavaHashSetTests PRIVATE -std=c++17)
endif()

opticraft_add_optional_test(LocalStatsFormatTests
    tests/LocalStatsFormatTests.cpp
    src/net/minecraft/src/LocalStatsFormat.cpp
)

opticraft_add_optional_test(EndDragonSpawnPolicyTests
    tests/EndDragonSpawnPolicyTests.cpp
)

# Keep third-party projects small: OptiCraft only needs their static libraries.
set(SDL_SHARED OFF CACHE BOOL "Build the SDL shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build the SDL static library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Build the SDL test library" FORCE)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Build zlib examples" FORCE)

add_subdirectory(external/glad EXCLUDE_FROM_ALL)
add_subdirectory(external/SDL2 EXCLUDE_FROM_ALL)
add_subdirectory(external/zlib EXCLUDE_FROM_ALL)

# SDL_net links against the SDL2 static target created above, so it must be
# added afterwards. Force a static build and skip its samples; CMP0077 lets the
# normal variable override SDL_net's own option() defaults.
set(BUILD_SHARED_LIBS OFF)
set(SDL2NET_SAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/SDL_net EXCLUDE_FROM_ALL)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

file(GLOB_RECURSE OPTICRAFT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

set(OPTICRAFT_MINIZIP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib/contrib/minizip/ioapi.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib/contrib/minizip/unzip.c"
)

# Unified diagnostic verbosity: 0=off, 1=info, 2=debug, 3=trace.
set(MC_LOG_LEVEL "0" CACHE STRING "Unified diagnostic verbosity: 0=off, 1=info, 2=debug, 3=trace")
set_property(CACHE MC_LOG_LEVEL PROPERTY STRINGS 0 1 2 3)

# --- Response files: keeps very long compiler/link command lines under the
# OS argument-length limit once every source file is on one command line. ---
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS ON)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS ON)
set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON)

file(GLOB_RECURSE ALL_GAME_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")

# Only the desktop Linux tree is ever compiled: win32, wii, ps2, and
# PC_LEGACY sources are excluded outright rather than discovered and then
# filtered, so an added console/legacy .cpp cannot silently join this target.
list(FILTER ALL_GAME_SOURCES EXCLUDE REGEX "src/pc/win32/.*")
list(FILTER ALL_GAME_SOURCES EXCLUDE REGEX "src/wii/.*")
list(FILTER ALL_GAME_SOURCES EXCLUDE REGEX "src/ps2/.*")
list(FILTER ALL_GAME_SOURCES EXCLUDE REGEX ".*_WII\\.cpp$")
list(FILTER ALL_GAME_SOURCES EXCLUDE REGEX ".*_PS2\\.cpp$")
list(FILTER ALL_GAME_SOURCES EXCLUDE REGEX ".*PcLegacy.*\\.cpp$")

# Exclusion of duplicate backends
list(FILTER ALL_GAME_SOURCES EXCLUDE REGEX "src/platform/RenderAPI_PC\\.cpp$")
list(FILTER ALL_GAME_SOURCES EXCLUDE REGEX "src/platform/RenderTerrainAPI_PC\\.cpp$")

add_executable(OptiCraft
    ${ALL_GAME_SOURCES}
    ${OPTICRAFT_MINIZIP_SOURCES}
    ${OPTICRAFT_HEADERS}
)

target_compile_features(OptiCraft PRIVATE cxx_std_17)
set_target_properties(OptiCraft PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# A level, not a boolean, so it is passed through as-is rather than through
# $<BOOL:>, which would collapse 2 to 1.
target_compile_definitions(OptiCraft PRIVATE
    MC_LOG_LEVEL=${MC_LOG_LEVEL}
    MC_LINUX
)

# Link-Time Optimization for Release. The port is built from hundreds of tiny
# cross-translation-unit accessors (getBlockId, getSavedLightValue, Chunk /
# NibbleArray getters) called millions of times per frame in the hot loops;
# without LTO the compiler cannot inline them across .cpp boundaries.
option(OPTICRAFT_ENABLE_LTO "Enable Link-Time Optimization for Release builds" OFF)
if(OPTICRAFT_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES CXX)
    if(ipo_supported)
        set_target_properties(OptiCraft PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else()
        message(WARNING "IPO/LTO requested but not supported by this toolchain, skipping: ${ipo_error}")
    endif()
endif()

target_include_directories(OptiCraft PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pc"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/miniaudio"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib/contrib/minizip"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/SDL2/include"
    "/usr/include/SDL2"
)

target_link_libraries(OptiCraft PRIVATE
    glad
    SDL2-static
    SDL2_net
    zlibstatic
    OpenGL::GL
    Threads::Threads
)

# Every build configuration lands in a predictable folder next to the source
# tree instead of beside CMake's generated files.
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(OptiCraft PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${config_upper}"
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/${config}"
    )
endforeach()