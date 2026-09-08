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

# Asset compiler and runtime package core have no dependency on the game.
add_library(PaladinAssetCore STATIC src/assets/AssetBinary.cpp src/assets/AssetPackage.cpp src/assets/AssetManager.cpp)
target_include_directories(PaladinAssetCore PUBLIC src)
add_library(PaladinAssetImport STATIC tools/asset_compiler/SourceImporter.cpp tools/asset_compiler/AssetCompiler.cpp)
target_include_directories(PaladinAssetImport PUBLIC src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PaladinAssetImport PUBLIC PaladinAssetCore SDL3::SDL3 SDL3_image::SDL3_image)
set(PALADIN_IMPORT_SIGNATURE "")
foreach(file tools/asset_compiler/SourceImporter.cpp tools/asset_compiler/SpriteProcessor.h src/rendering/SunlitMaterials.h src/assets/AssetBinary.cpp)
 set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
 file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${file}" part)
 string(APPEND PALADIN_IMPORT_SIGNATURE "${part}")
endforeach()
target_compile_definitions(PaladinAssetImport PRIVATE PALADIN_IMPORT_SIGNATURE="${PALADIN_IMPORT_SIGNATURE}")
add_executable(PaladinAssetCompiler tools/asset_compiler/main.cpp)
target_link_libraries(PaladinAssetCompiler PRIVATE PaladinAssetImport)
# The compiler owns SDL_image deployment. Debug Paladin also uses this DLL
# for F6 import; Release Paladin does not depend on it. The tools currently
# share the build output directory with the game.
add_custom_command(TARGET PaladinAssetCompiler POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:PaladinAssetCompiler>
 COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3_image::SDL3_image> $<TARGET_FILE_DIR:PaladinAssetCompiler>)
add_custom_target(PaladinCompileAssets
 COMMAND $<TARGET_FILE:PaladinAssetCompiler> compile ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/assets/packages
 DEPENDS PaladinAssetCompiler VERBATIM)

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
    src/rendering/BuildingView.cpp
    src/rendering/ScenePresentation.cpp
    src/rendering/SettlementEnvironmentDetails.cpp
    src/rendering/TerrainMaterialField.cpp
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

target_link_libraries(Paladin PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf PaladinAssetCore $<$<CONFIG:Debug>:PaladinAssetImport>)
target_compile_definitions(Paladin PRIVATE $<$<CONFIG:Debug>:PALADIN_SOURCE_ART>)
target_include_directories(Paladin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(Paladin PaladinCompileAssets)

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

# Authoring PNGs/catalogs remain in the source tree; only packages are runtime inputs.
enable_testing()

add_executable(PaladinTests
    tests/TestMain.cpp
    tests/EntityAttributeTests.cpp
    tests/WorldReliefTests.cpp
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

# Keep source/art inputs trackable without publishing local build output.
# Git is optional for building/running the C++ simulation tests.
find_package(Git QUIET)
if(Git_FOUND)
    add_test(NAME PaladinGitIgnoreTests
        COMMAND ${CMAKE_COMMAND}
            "-DPALADIN_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DPALADIN_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/gitignore-policy-test"
            "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/GitIgnoreTests.cmake")
endif()

# Opt-in SDL integration check for application routing. The ordinary simulation
# tests remain independent of a window/renderer and do not rebuild the UI.
option(PALADIN_BUILD_APPLICATION_SMOKE_TESTS "Build application routing and mandatory art consistency tests" ON)
if(PALADIN_BUILD_APPLICATION_SMOKE_TESTS)
    get_target_property(PALADIN_APPLICATION_SOURCES Paladin SOURCES)
    list(REMOVE_ITEM PALADIN_APPLICATION_SOURCES src/main.cpp)
    add_executable(PaladinApplicationSmokeTests
        tests/ApplicationSmokeTests.cpp
        ${PALADIN_APPLICATION_SOURCES}
    )
    target_include_directories(PaladinApplicationSmokeTests PRIVATE src tests)
    target_link_libraries(PaladinApplicationSmokeTests PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf PaladinAssetImport)
    target_include_directories(PaladinApplicationSmokeTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(PaladinApplicationSmokeTests PRIVATE PALADIN_SOURCE_ART PALADIN_TEST_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    # Reuse the game's runtime DLL/font packaging even when building only this target.
    add_dependencies(PaladinApplicationSmokeTests Paladin)
    add_test(NAME PaladinApplicationSmokeTests COMMAND PaladinApplicationSmokeTests)
    add_custom_target(PaladinArtCheck
        COMMAND ${CMAKE_COMMAND} -E env SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software
            $<TARGET_FILE:PaladinApplicationSmokeTests>
        DEPENDS PaladinApplicationSmokeTests
        COMMENT "Verify final world pixel grid, daylight/night rendering and sprite integration"
        VERBATIM
    )
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
# Keep the interactive raster/mesh path optimized in the Visual Studio Debug
# executable too. Simulation and application code remain fully debuggable.
# These files generate the identical pixels; /Od's per-pixel STL overhead made
# moving into cold terrain/forest chunks exceed the frame budget.
if(MSVC)
    # Rendering and simulation exchange STL containers: use one ABI consistently.
    # Debug iterator proxy bookkeeping serializes the large per-frame geometry
    # batches. Keep symbols and simulation /RTC1, without that container tax.
    add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    file(GLOB_RECURSE PALADIN_DEBUG_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "tests/*.cpp")
    foreach(PALADIN_DEBUG_SOURCE IN LISTS PALADIN_DEBUG_SOURCES)
        if(NOT PALADIN_DEBUG_SOURCE MATCHES "/src/(rendering|assets)/")
            set_property(SOURCE ${PALADIN_DEBUG_SOURCE} APPEND PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:/RTC1>")
        endif()
    endforeach()
    file(GLOB PALADIN_RENDER_SOURCES CONFIGURE_DEPENDS "src/rendering/*.cpp" "src/assets/*.cpp")
    set_property(SOURCE ${PALADIN_RENDER_SOURCES} APPEND PROPERTY COMPILE_OPTIONS
        "$<$<CONFIG:Debug>:/O2>" "$<$<CONFIG:Debug>:/Ob2>")
endif()

add_executable(PaladinAssetTests tests/AssetCompilerTests.cpp)
target_link_libraries(PaladinAssetTests PRIVATE PaladinAssetImport)
add_dependencies(PaladinAssetTests PaladinAssetCompiler)
add_test(NAME PaladinAssetTests COMMAND PaladinAssetTests)

if(MSVC)
 target_compile_options(PaladinAssetImport PRIVATE /O2 /Ob2)
endif()
