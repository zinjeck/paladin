cmake_minimum_required(VERSION 3.24)

project(Paladin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.20
)

FetchContent_MakeAvailable(SDL3)

add_executable(Paladin
    src/main.cpp
    src/core/Application.cpp
    src/core/SimulationClock.cpp
    src/platform/Window.cpp
    src/rendering/Renderer.cpp
    src/simulation/Simulation.cpp
    src/world/World.cpp
    src/world/WorldTime.cpp
    src/world/WorldGrid.cpp
    src/rendering/WorldGridRenderer.cpp
)

target_include_directories(Paladin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Paladin PRIVATE SDL3::SDL3)

add_custom_command(
    TARGET Paladin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:Paladin>
)

# ============================================================
# Tests
# ============================================================

enable_testing()

add_executable(PaladinTests
    tests/TestMain.cpp
    tests/CoreTests.cpp
    tests/WorldTests.cpp

    src/world/World.cpp
    src/world/WorldTime.cpp
    src/world/WorldGrid.cpp
)

target_include_directories(PaladinTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

add_test(
    NAME PaladinTests
    COMMAND PaladinTests
)
