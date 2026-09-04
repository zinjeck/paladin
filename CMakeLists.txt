cmake_minimum_required(VERSION 3.24)

project(Paladin LANGUAGES C CXX)

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

set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG release-3.2.2
)
FetchContent_MakeAvailable(SDL3_ttf)

add_executable(Paladin
    src/main.cpp
    src/core/Application.cpp
    src/core/SimulationClock.cpp
    src/interaction/CameraNavigationPolicy.cpp
    src/interaction/SettlementCommandController.cpp
    src/interaction/SettlementInspectionController.cpp
    src/interaction/SettlementObjectPlacementController.cpp
    src/interaction/SettlementPlacementController.cpp
    src/platform/Window.cpp
    src/rendering/Renderer.cpp
    src/rendering/CityRenderer.cpp
    src/rendering/SettlementNaturalFeatureRenderer.cpp
    src/rendering/Texture.cpp
    src/rendering/SpriteRenderer.cpp
    src/rendering/OverlayRenderer.cpp
    src/rendering/WorldRenderer.cpp
    src/rendering/SettlementMarkerRenderer.cpp
    src/rendering/SettlementCitizenRenderer.cpp
    src/rendering/SettlementCommandRenderer.cpp
    src/rendering/SettlementObjectRenderer.cpp
    src/rendering/TerritoryPresentationPolicy.cpp
    src/rendering/TerritoryRenderer.cpp
    src/simulation/Simulation.cpp
    src/simulation/WorldSimulationPipeline.cpp
    src/simulation/systems/SettlementEconomySystem.cpp
    src/simulation/systems/SettlementPopulationSystem.cpp
    src/world/World.cpp
    src/world/WorldTime.cpp
    src/world/generation/GenerationNoise.cpp
    src/world/generation/SettlementMapGenerator.cpp
    src/world/generation/LandmassGenerationTemplate.cpp
    src/world/generation/LandmassGenerator.cpp
    src/world/generation/ClimateGenerator.cpp
    src/world/generation/TerrainBiomeClassifier.cpp
    src/world/generation/WorldGenerator.cpp
    src/world/generation/WorldGenerationSeed.cpp
    src/world/settlements/ResourceStockpile.cpp
    src/world/settlements/SettlementMap.cpp
    src/world/settlements/SettlementEmploymentState.cpp
    src/world/settlements/SettlementNaturalFeatures.cpp
    src/simulation/systems/SettlementNavigation.cpp
    src/simulation/systems/SettlementCitizenMovement.cpp
    src/world/settlements/SettlementEconomy.cpp
    src/world/settlements/SettlementFoundationProfile.cpp
    src/world/settlements/SettlementResourceDefinition.cpp
    src/world/settlements/SettlementPopulation.cpp
    src/world/settlements/SettlementSimulationPolicy.cpp
    src/world/settlements/SettlementSimulationState.cpp
    src/world/settlements/citizens/SettlementCitizenState.cpp
    src/world/settlements/commands/SettlementCommandDefinition.cpp
    src/world/settlements/commands/SettlementCommandState.cpp
    src/world/settlements/objects/SettlementObjectDefinition.cpp
    src/world/settlements/objects/SettlementObjectState.cpp
    src/world/territory/TerritoryFoundationPolicy.cpp
    src/world/territory/TerritoryFoundationSystem.cpp
    src/world/territory/TerritoryMap.cpp
    src/rendering/WorldGridRenderer.cpp
    src/ui/BitmapFontRenderer.cpp
    src/ui/CityHud.cpp
    src/ui/EmploymentPanel.cpp
    src/ui/DebugConsole.cpp
    src/ui/GrayUiRenderer.cpp
    src/ui/FoundingPanel.cpp
    src/ui/UiButton.cpp
    src/ui/UiTextField.cpp
    src/ui/MainMenu.cpp
    src/ui/NormalFontRenderer.cpp
    src/ui/SettlementInspectionPanel.cpp
    src/ui/SimulationSpeedControls.cpp
    src/ui/WorldHud.cpp
)

target_include_directories(Paladin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Paladin PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)

add_custom_command(
    TARGET Paladin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:Paladin>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
        $<TARGET_FILE_DIR:Paladin>
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:Paladin>/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Arimo-Regular.ttf
        $<TARGET_FILE_DIR:Paladin>/assets/fonts/Arimo-Regular.ttf
)

# ============================================================
# Tests
# ============================================================

enable_testing()

add_executable(PaladinTests
    tests/TestMain.cpp
    tests/SettlementActivityTests.cpp
    tests/SettlementEmploymentTests.cpp
    tests/CoreTests.cpp
    tests/WorldTests.cpp
    tests/WorldGenerationTests.cpp

    src/world/World.cpp
    src/world/WorldTime.cpp
    src/world/generation/GenerationNoise.cpp
    src/world/generation/SettlementMapGenerator.cpp
    src/world/generation/LandmassGenerationTemplate.cpp
    src/world/generation/LandmassGenerator.cpp
    src/world/generation/ClimateGenerator.cpp
    src/world/generation/TerrainBiomeClassifier.cpp
    src/world/generation/WorldGenerator.cpp
    src/world/generation/WorldGenerationSeed.cpp
    src/world/settlements/ResourceStockpile.cpp
    src/world/settlements/SettlementMap.cpp
    src/world/settlements/SettlementEmploymentState.cpp
    src/world/settlements/SettlementNaturalFeatures.cpp
    src/simulation/systems/SettlementNavigation.cpp
    src/simulation/systems/SettlementCitizenMovement.cpp
    src/world/settlements/SettlementEconomy.cpp
    src/world/settlements/SettlementFoundationProfile.cpp
    src/world/settlements/SettlementResourceDefinition.cpp
    src/world/settlements/SettlementPopulation.cpp
    src/world/settlements/SettlementSimulationPolicy.cpp
    src/world/settlements/SettlementSimulationState.cpp
    src/world/settlements/citizens/SettlementCitizenState.cpp
    src/world/settlements/commands/SettlementCommandDefinition.cpp
    src/world/settlements/commands/SettlementCommandState.cpp
    src/world/settlements/objects/SettlementObjectDefinition.cpp
    src/world/settlements/objects/SettlementObjectState.cpp
    src/world/territory/TerritoryFoundationPolicy.cpp
    src/world/territory/TerritoryFoundationSystem.cpp
    src/world/territory/TerritoryMap.cpp
    src/simulation/WorldSimulationPipeline.cpp
    src/simulation/systems/SettlementEconomySystem.cpp
    src/simulation/systems/SettlementPopulationSystem.cpp
    src/interaction/SettlementObjectPlacementController.cpp
    src/interaction/SettlementCommandController.cpp
    src/interaction/SettlementInspectionController.cpp
)

target_include_directories(PaladinTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

add_test(
    NAME PaladinTests
    COMMAND PaladinTests
)
