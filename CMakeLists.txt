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

# PNG exports from the artist; use SDL_image's maintained bundled decoder.
set(SDLIMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
foreach(format AVIF JPG JXL LBM PCX PNM QOI SVG TGA TIF WEBP XCF XPM XV)
    set(SDLIMAGE_${format} OFF CACHE BOOL "" FORCE)
endforeach()
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.2.4
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(SDL3_image)

add_executable(Paladin
    src/main.cpp
    src/core/Application.cpp
    src/core/ApplicationMainMenu.cpp
    src/core/ApplicationWorldScreen.cpp
    src/core/ApplicationCityScreen.cpp
    src/core/ApplicationFrame.cpp
    src/core/ApplicationReports.cpp
    src/core/ApplicationCityInput.cpp
    src/core/ApplicationWorldInput.cpp
    src/core/ApplicationSession.cpp
    src/core/ApplicationCamera.cpp
    src/core/ApplicationDebug.cpp
    src/core/SimulationClock.cpp
    src/interaction/CameraNavigationPolicy.cpp
    src/interaction/SettlementCommandController.cpp
    src/interaction/SettlementInspectionController.cpp
    src/interaction/SettlementObjectPlacementController.cpp
    src/interaction/SettlementPlacementController.cpp
    src/platform/Window.cpp
    src/rendering/Renderer.cpp
    src/rendering/CityRenderer.cpp
    src/rendering/SceneSpriteLibrary.cpp
    src/rendering/SettlementStructurePresentation.cpp
    src/rendering/SettlementLogisticsRenderer.cpp
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
    src/simulation/SimulationReports.cpp
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
    src/world/entities/animals/SettlementAnimals.cpp
    src/simulation/systems/SettlementAnimalWork.cpp
    src/world/settlements/SettlementEmploymentState.cpp
    src/world/settlements/SettlementNaturalFeatures.cpp
    src/simulation/systems/SettlementNavigation.cpp
    src/simulation/systems/SettlementCitizenMovement.cpp
    src/simulation/systems/SettlementActivitySystem.cpp
    src/simulation/systems/SettlementActivitySelection.cpp
    src/simulation/systems/SettlementLeisure.cpp
    src/simulation/systems/SettlementCitizenNeeds.cpp
    src/simulation/systems/SettlementFamilySystem.cpp
    src/simulation/systems/SettlementJobBoard.cpp
    src/world/settlements/SettlementLogistics.cpp
    src/world/settlements/SettlementCommerce.cpp
    src/world/settlements/objects/jobs/fishery/FisheryJob.cpp
    src/world/settlements/objects/SettlementObjectLifecycle.cpp
    src/world/settlements/SettlementEconomy.cpp
    src/world/settlements/SettlementFoundationProfile.cpp
    src/world/settlements/SettlementResourceDefinition.cpp
    src/world/settlements/SettlementPopulation.cpp
    src/world/settlements/SettlementSimulationPolicy.cpp
    src/world/settlements/SettlementSimulationState.cpp
    src/world/settlements/citizens/SettlementCitizenState.cpp
    src/world/settlements/citizens/SettlementCitizenAttributes.cpp
    src/world/settlements/SettlementImmigration.cpp
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
    src/ui/LedgerPanel.cpp
    src/ui/LedgerPanelData.cpp
    src/ui/EmploymentPanel.cpp
    src/ui/EmploymentPopulation.cpp
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

target_link_libraries(Paladin PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)

add_custom_command(
    TARGET Paladin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:Paladin>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
        $<TARGET_FILE_DIR:Paladin>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3_image::SDL3_image>
        $<TARGET_FILE_DIR:Paladin>
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:Paladin>/assets/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Arimo-Regular.ttf
        $<TARGET_FILE_DIR:Paladin>/assets/fonts/Arimo-Regular.ttf
)

# Copies only runtime exports. Art masters and the asset Git policy stay separate.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites")
    add_custom_target(PaladinSpriteAssets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites $<TARGET_FILE_DIR:Paladin>/assets/sprites)
    add_dependencies(Paladin PaladinSpriteAssets)
endif()

# ============================================================
# Tests
# Presentation recipes are source-controlled; artwork remains private.
add_custom_target(PaladinObjectPresentation ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Paladin>/assets/sprites
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/city-objects.catalog
        $<TARGET_FILE_DIR:Paladin>/assets/sprites/objects.catalog)
if(TARGET PaladinSpriteAssets)
    add_dependencies(PaladinObjectPresentation PaladinSpriteAssets)
endif()
add_dependencies(Paladin PaladinObjectPresentation)
# ============================================================

enable_testing()

add_executable(PaladinTests
    tests/TestMain.cpp
    src/simulation/SimulationReports.cpp
    tests/SettlementSimulationLoopTests.cpp
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
    src/world/entities/animals/SettlementAnimals.cpp
    src/simulation/systems/SettlementAnimalWork.cpp
    src/world/settlements/SettlementEmploymentState.cpp
    src/world/settlements/SettlementNaturalFeatures.cpp
    src/simulation/systems/SettlementNavigation.cpp
    src/simulation/systems/SettlementCitizenMovement.cpp
    src/simulation/systems/SettlementActivitySystem.cpp
    src/simulation/systems/SettlementActivitySelection.cpp
    src/simulation/systems/SettlementLeisure.cpp
    src/simulation/systems/SettlementCitizenNeeds.cpp
    src/simulation/systems/SettlementFamilySystem.cpp
    src/simulation/systems/SettlementJobBoard.cpp
    src/world/settlements/SettlementLogistics.cpp
    src/world/settlements/SettlementCommerce.cpp
    src/world/settlements/objects/jobs/fishery/FisheryJob.cpp
    src/world/settlements/objects/SettlementObjectLifecycle.cpp
    src/world/settlements/SettlementEconomy.cpp
    src/world/settlements/SettlementFoundationProfile.cpp
    src/world/settlements/SettlementResourceDefinition.cpp
    src/world/settlements/SettlementPopulation.cpp
    src/world/settlements/SettlementSimulationPolicy.cpp
    src/world/settlements/SettlementSimulationState.cpp
    src/world/settlements/citizens/SettlementCitizenState.cpp
    src/world/settlements/citizens/SettlementCitizenAttributes.cpp
    src/world/settlements/SettlementImmigration.cpp
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

# Opt-in SDL integration check for application routing. The ordinary simulation
# tests remain independent of a window/renderer and do not rebuild the UI.
option(PALADIN_BUILD_APPLICATION_SMOKE_TESTS "Build application routing smoke test" OFF)
if(PALADIN_BUILD_APPLICATION_SMOKE_TESTS)
    get_target_property(PALADIN_APPLICATION_SOURCES Paladin SOURCES)
    list(REMOVE_ITEM PALADIN_APPLICATION_SOURCES src/main.cpp)
    add_executable(PaladinApplicationSmokeTests
        tests/ApplicationSmokeTests.cpp
        ${PALADIN_APPLICATION_SOURCES}
    )
    target_include_directories(PaladinApplicationSmokeTests PRIVATE src tests)
    target_link_libraries(PaladinApplicationSmokeTests PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)
    # Reuse the game's runtime DLL/font packaging even when building only this target.
    add_dependencies(PaladinApplicationSmokeTests Paladin)
    add_test(NAME PaladinApplicationSmokeTests COMMAND PaladinApplicationSmokeTests)
    set_tests_properties(PaladinApplicationSmokeTests PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
        TIMEOUT 120
    )
    add_test(NAME PaladinRenderPerformanceTests COMMAND PaladinApplicationSmokeTests)
    set_tests_properties(PaladinRenderPerformanceTests PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;PALADIN_RENDER_BENCHMARK=1"
        TIMEOUT 60
    )
endif()

# Artist exports can be reloaded with F6 without recompiling in Debug builds.
target_compile_definitions(Paladin PRIVATE $<$<CONFIG:Debug>:PALADIN_ART_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites">)
add_custom_target(PaladinArtSettings ALL
 COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Paladin>/assets/sprites
 COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/config/art-palette.hex $<TARGET_FILE_DIR:Paladin>/assets/sprites/art-palette.hex
 COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/config/pieces.catalog $<TARGET_FILE_DIR:Paladin>/assets/sprites/pieces.catalog
 COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/config/lights.catalog $<TARGET_FILE_DIR:Paladin>/assets/sprites/lights.catalog)
add_dependencies(Paladin PaladinArtSettings)
