# Run in a disposable build-directory repository, never the user's checkout.
foreach(required PALADIN_SOURCE_DIR PALADIN_TEST_DIR GIT_EXECUTABLE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing required variable: ${required}")
    endif()
endforeach()

set(fixture "${PALADIN_TEST_DIR}/repo")
file(MAKE_DIRECTORY "${fixture}")
configure_file("${PALADIN_SOURCE_DIR}/.gitignore" "${fixture}/.gitignore" COPYONLY)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" init --quiet
    WORKING_DIRECTORY "${fixture}"
    RESULT_VARIABLE init_result
    ERROR_VARIABLE init_error
)
if(NOT init_result EQUAL 0)
    message(FATAL_ERROR "Could not initialize ignore-policy fixture: ${init_error}")
endif()

function(check_ignore expected path)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -c core.excludesFile= check-ignore --quiet --no-index -- "${path}"
        WORKING_DIRECTORY "${fixture}"
        RESULT_VARIABLE result
        ERROR_VARIABLE error
    )
    if(NOT "${result}" STREQUAL "${expected}")
        message(FATAL_ERROR "Ignore-policy mismatch for '${path}': expected exit ${expected}, got ${result}. ${error}")
    endif()
endfunction()

# Exit 1 means trackable. Do not silently drop an authored source language.
foreach(extension c h cc cpp cxx c++ hh hpp hxx h++ inl ipp tpp ixx cppm cs csx asm s S inc)
    check_ignore(1 "src/example.${extension}")
endforeach()
foreach(path
    .gitignore AGENTS.md CMakeLists.txt cmake/Rules.cmake cmake/Config.cmake.in
    tests/EntityAttributeTests.cpp tests/WorldReliefTests.cpp tests/GitIgnoreTests.cmake
    config/city-objects.catalog config/pieces.catalog config/lights.catalog
    config/art-palette.hex config/master.gpl config/master.pal
    assets/sprites/sprites.catalog assets/sprites/world-relief/hill.png
    assets/sprites/terrain/grass.png assets/sprites/atlas.json
    assets/fonts/Arimo-Regular.ttf assets/fonts/OFL.txt licenses/art-license.txt
    handoffs/art-direction/README.md
    handoffs/art-direction/world-cartography/source/hill.png
    handoffs/art-direction/world-cartography/source/hill.kra
    handoffs/art-direction/world-cartography/source/hill.aseprite
    handoffs/art-direction/world-cartography/source/hill.ora
    handoffs/art-direction/world-cartography/source/roof.psd
    handoffs/art-direction/world-cartography/tools/export.ps1
    handoffs/art-direction/world-cartography/tools/export.py
    handoffs/art-direction/world-cartography/previews/world-foothills.png
    handoffs/art-direction/world-cartography/references/style.jpg
)
    check_ignore(1 "${path}")
endforeach()

# Exit 0 means ignored. Artwork elsewhere is not automatically public.
foreach(path
    private-notes.md screenshot.png .env
    docs/private-reference.cpp sources/private-example.h handoffs/private-notes.md
    handoffs/other-work/source.cpp
    out/build/generated.cpp build/generated.h build-debug/generated.cpp
    cmake-build-debug/generated.cpp CMakeFiles/CompilerId.cpp _deps/sdl/src/vendor.c
    .vs/cache.cpp .vscode/settings.json .idea/workspace.xml .cache/atlas.png
    Testing/log.txt test-results/screenshot.png coverage/report.cpp
    coverage-debug/report.cpp node_modules/vendor.cpp .venv/include/python.h
    venv/include/python.h renderer-test-fixture/example.png
    assets/out/atlas.png assets/build/generated.cpp assets/_deps/vendor.h
    assets/sprites/hill.png.tmp assets/sprites/hill.png.bak assets/sprites/Thumbs.db
    handoffs/art-direction/world-cartography/source/hill.kra~
    handoffs/art-direction/world-cartography/source/hill.aseprite~
    handoffs/art-direction/world-cartography/previews/.DS_Store
    handoffs/art-direction/world-cartography/tools/__pycache__/export.pyc
    handoffs/art-direction/world-cartography/tools/.venv/include/python.h
    handoffs/art-direction/world-cartography/out/preview.png
)
    check_ignore(0 "${path}")
endforeach()
message(STATUS "Paladin source/art tracking and generated/private-file exclusions passed.")
