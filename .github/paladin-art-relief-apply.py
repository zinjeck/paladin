"""Temporary, guarded edits for this verification branch only."""
from pathlib import Path
import subprocess

EXPECTED = {
    "CMakeLists.txt": "d9d145ffe22b6b702a7b29f88727695d32e5606e",
    "tests/TestMain.cpp": "a62d5f98c8e3827f2c58b04e1e65227bdcb429e8",
    "src/world/generation/WorldRelief.h": "1c02fa61826b3a355239a9b6fa2de5867c2271ab",
}
for name, expected in EXPECTED.items():
    actual = subprocess.check_output(["git", "rev-parse", f"HEAD:{name}"], text=True).strip()
    if actual != expected:
        raise RuntimeError(f"Refusing to patch an unexpected baseline: {name}: {actual}")


def replace_once(name, old, new):
    path = Path(name)
    data = path.read_text(encoding="utf-8")
    if data.count(old) != 1:
        raise RuntimeError(f"Expected exactly one replacement in {name}: {old!r}")
    path.write_text(data.replace(old, new), encoding="utf-8", newline="\n")


replace_once(
    "src/world/generation/WorldRelief.h",
    """                double u = double(x) / w, v = double(y) / h;
                u += .035 * GenerationNoise::simplexFractal(
                                u * 5,
                                v * 5,
                                seed + 51,
                                2,
                                .5,
                                2
                            );
                v += .035 * GenerationNoise::simplexFractal(
                                u * 5,
                                v * 5,
                                seed + 89,
                                2,
                                .5,
                                2
                            );""",
    """                const double sourceU = double(x) / w;
                const double sourceV = double(y) / h;
                // Both offset fields sample the same unwarped coordinate.
                // Do not let the horizontal offset feed the vertical sample.
                const double u = sourceU + .035 * GenerationNoise::simplexFractal(
                    sourceU * 5, sourceV * 5, seed + 51, 2, .5, 2
                );
                const double v = sourceV + .035 * GenerationNoise::simplexFractal(
                    sourceU * 5, sourceV * 5, seed + 89, 2, .5, 2
                );""",
)
replace_once(
    "tests/TestMain.cpp",
    "void runSettlementSimulationLoopTests();",
    "void runEntityAttributeTests();\nvoid runWorldReliefTests();\nvoid runSettlementSimulationLoopTests();",
)
replace_once(
    "tests/TestMain.cpp",
    "        runSettlementSimulationLoopTests();",
    "        runEntityAttributeTests();\n        runWorldReliefTests();\n        runSettlementSimulationLoopTests();",
)
replace_once(
    "CMakeLists.txt",
    "    tests/TestMain.cpp\n",
    "    tests/TestMain.cpp\n    tests/EntityAttributeTests.cpp\n    tests/WorldReliefTests.cpp\n",
)
replace_once(
    "CMakeLists.txt",
    "# Copies only runtime exports. Art masters and the asset Git policy stay separate.",
    "# Copies only runtime exports. Source art and review material stay out of the build.",
)
replace_once(
    "CMakeLists.txt",
    "# Presentation recipes are source-controlled; artwork remains private.",
    "# Presentation recipes and runtime artwork are source-controlled inputs.",
)
replace_once(
    "CMakeLists.txt",
    """add_test(
    NAME PaladinTests
    COMMAND PaladinTests
)
""",
    """add_test(
    NAME PaladinTests
    COMMAND PaladinTests
)

# Keep source/art inputs trackable without publishing local build output.
# Git is optional for building/running the C++ simulation tests.
find_package(Git QUIET)
if(Git_FOUND)
    add_test(NAME PaladinGitIgnoreTests
        COMMAND ${CMAKE_COMMAND}
            \"-DPALADIN_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}\"
            \"-DPALADIN_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/gitignore-policy-test\"
            \"-DGIT_EXECUTABLE=${GIT_EXECUTABLE}\"
            -P \"${CMAKE_CURRENT_SOURCE_DIR}/tests/GitIgnoreTests.cmake\")
endif()
""",
)
