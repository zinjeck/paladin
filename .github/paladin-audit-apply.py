from pathlib import Path
import re
import subprocess

expected = {
    'src/interaction/SettlementCommandController.h': '2953503f9356a59e7940d900d03c24dde989028b',
    'src/interaction/SettlementCommandController.cpp': 'a1b9cc58aab7c3d2093fec11695f1f00684ac678',
    'src/world/settlements/commands/SettlementCommandState.h': 'a0179b3ee501e11217b00a9687a93ab0e01e7f3c',
    'src/world/settlements/commands/SettlementCommandState.cpp': 'bb82386cf179112f6f7fff9ed07838a4203dcf2e',
    'src/core/ApplicationCityInput.cpp': 'b89a5f6cfe664214cfff47651fe5ace69623ce04',
    'src/simulation/systems/SettlementActivitySystem.cpp': '18197bed43e514403df21f1c9ef01d1d6f97850b',
    'src/simulation/systems/SettlementCitizenMovement.cpp': '57615bed32211d461a6dd23a24fa4c1574d0e964',
    'src/rendering/SettlementNaturalFeatureRenderer.cpp': '9411ab3b87cfb5fbaf156906ad26164c9027551d',
    'src/rendering/WorldGridRenderer.cpp': '1945dceeb86e466506e8c3fb1b8d8b7175824d11',
    'tests/SettlementSimulationLoopTests.cpp': 'bae89c145acd589d2c79da5b0de9e2d2d197f393',
}
for filename, sha in expected.items():
    actual = subprocess.check_output(['git', 'hash-object', filename], text=True).strip()
    if actual != sha:
        raise RuntimeError(f'Unexpected baseline for {filename}: {actual}')

def replace(filename, old, new, count=1):
    path = Path(filename)
    text = path.read_text()
    if text.count(old) != count:
        raise RuntimeError(f'{filename}: expected {count} matches, got {text.count(old)} for {old!r}')
    path.write_text(text.replace(old, new))

# Require an explicit simulation timestamp at both public cancellation entry points.
replace('src/interaction/SettlementCommandController.h',
        '            SettlementCitizenState& citizens\n        );',
        '            SettlementCitizenState& citizens,\n            double minute\n        );')
replace('src/interaction/SettlementCommandController.cpp',
        '        SettlementCitizenState& citizens\n    )',
        '        SettlementCitizenState& citizens,\n        double minute\n    )')
replace('src/interaction/SettlementCommandController.cpp',
        '                       *footprint,\n                       citizens\n',
        '                       *footprint,\n                       citizens,\n                       minute\n')
replace('src/world/settlements/commands/SettlementCommandState.h',
        '        std::size_t cancelIntersecting(\n            SettlementMap& map,\n            const SettlementObjectFootprint& footprint,\n            SettlementCitizenState& citizens\n        );',
        '        std::size_t cancelIntersecting(\n            SettlementMap& map,\n            const SettlementObjectFootprint& footprint,\n            SettlementCitizenState& citizens,\n            double minute\n        );')
replace('src/world/settlements/commands/SettlementCommandState.cpp',
        '    std::size_t SettlementCommandState::cancelIntersecting(\n        SettlementMap& map,\n        const SettlementObjectFootprint& area,\n        SettlementCitizenState& citizens\n    )',
        '    std::size_t SettlementCommandState::cancelIntersecting(\n        SettlementMap& map,\n        const SettlementObjectFootprint& area,\n        SettlementCitizenState& citizens,\n        double minute\n    )')
replace('src/world/settlements/commands/SettlementCommandState.cpp',
        '            map.logistics.synchronize(map.objectState(), 0);',
        '            map.logistics.synchronize(map.objectState(), minute);')
replace('src/core/ApplicationCityInput.cpp',
        '                            settlement->simulationState().citizens()\n                        )',
        '                            settlement->simulationState().citizens(),\n                            static_cast<double>(\n                                simulation_->world().time().totalGameMinutes()\n                            )\n                        )')

# Give the old zero-origin fixtures explicit timestamps. Nested braces/parentheses
# are counted so footprint commas are not mistaken for function arguments.
def add_fixture_times(text, method, expected_args):
    insertions = []
    pattern = r'(?:\.|->)\s*' + method + r'\s*\('
    for match in re.finditer(pattern, text):
        depth = 1
        braces = brackets = 0
        commas = 0
        quote = None
        escaped = False
        for i in range(match.end(), len(text)):
            c = text[i]
            if quote:
                if escaped:
                    escaped = False
                elif c == '\\':
                    escaped = True
                elif c == quote:
                    quote = None
                continue
            if c in ('"', "'"):
                quote = c
            elif c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0:
                    if commas + 1 == expected_args:
                        last = i
                        while last > match.end() and text[last - 1].isspace():
                            last -= 1
                        if '\n' in text[last:i]:
                            indent_start = text.rfind('\n', match.end(), last) + 1
                            indent = re.match(r' *', text[indent_start:last]).group()
                            insertions.append((last, ',\n' + indent + '0'))
                        else:
                            insertions.append((last, ', 0'))
                    break
            elif c == '{':
                braces += 1
            elif c == '}':
                braces -= 1
            elif c == '[':
                brackets += 1
            elif c == ']':
                brackets -= 1
            elif c == ',' and depth == 1 and braces == 0 and brackets == 0:
                commas += 1
        else:
            raise RuntimeError('Unterminated test call')
    for index, value in reversed(insertions):
        text = text[:index] + value + text[index:]
    return text

for path in Path('tests').glob('*.cpp'):
    original = path.read_text()
    updated = add_fixture_times(original, 'cancelIntersecting', 3)
    updated = add_fixture_times(updated, 'pointerReleased', 3)
    if updated != original:
        path.write_text(updated)

loop_tests = 'tests/SettlementSimulationLoopTests.cpp'
replace(loop_tests,
        '.cancelIntersecting(map, {{15, 14}, 3, 3}, citizens, 0)',
        '.cancelIntersecting(map, {{15, 14}, 3, 3}, citizens, minute)')

activity = 'src/simulation/systems/SettlementActivitySystem.cpp'
replace(activity,
        '        bool citizenDied = false;\n        for (auto& c : citizens.citizens_)',
        '''        bool citizenDied = false;
        // Both pre-existing deaths and deaths caused by needs take the same
        // cleanup path, before the dense citizen array is compacted.
        const auto handleDeath = [&](SettlementCitizen& deceased)
        {
            citizenDied = true;
            for (auto& survivor : citizens.citizens_)
            {
                survivor.familiarities.erase(deceased.id);
            }
            map.commerce.citizenDeparted(deceased.id);
            map.employment().citizenDeparted(deceased.workplaceId);
            deceased.workplaceId = {};
            finish(map, deceased, minute);
        };
        for (auto& c : citizens.citizens_)''')
replace(activity,
        '''                citizenDied = true;
                for (auto& survivor : citizens.citizens_)
                {
                    survivor.familiarities.erase(c.id);
                }
                map.commerce.citizenDeparted(c.id);
                map.employment().citizenDeparted(c.workplaceId);
                c.workplaceId = {};
                finish(map, c, minute);
                continue;''',
        '''                handleDeath(c);
                continue;''')
replace(activity,
        '''                citizenDied = true;
                map.commerce.citizenDeparted(c.id);
                map.employment().citizenDeparted(c.workplaceId);
                c.workplaceId = {};
                finish(map, c, minute);
                continue;''',
        '''                handleDeath(c);
                continue;''')

def initialize_edge(indent, target):
    return (indent + 'c.stepDuration = citizens.navigation_.stepCost(\n' +
            indent + '    map,\n' + indent + '    c.tilePosition,\n' +
            indent + '    ' + target + ',\n' + indent + '    citizens.movementPolicy\n' +
            indent + ');\n')
for indent, target in [('                ', 'c.path.front()'),
                       ('                        ', 'next'),
                       ('                    ', 'target')]:
    destination = 'c.task.target' if target == 'c.path.front()' else target
    old = indent + 'c.stepProgress = 0;\n' + indent + 'c.destination = ' + destination + ';'
    new = indent + 'c.stepProgress = 0;\n' + initialize_edge(indent, target) + indent + 'c.destination = ' + destination + ';'
    replace(activity, old, new)

movement = 'src/simulation/systems/SettlementCitizenMovement.cpp'
replace(movement, 'namespace Paladin\n{\n    double SettlementCitizen::visualX()',
        '''namespace Paladin
{
    namespace
    {
        double movementFraction(double progress, double duration) noexcept
        {
            // Invalid timing must not leak NaN/Inf into captured positions or
            // rendering. Keep the citizen at the current tile until repaired.
            if (!std::isfinite(progress) || !std::isfinite(duration) ||
                duration <= 0)
            {
                return 0;
            }
            return std::clamp(progress / duration, 0.0, 1.0);
        }
    } // namespace
    double SettlementCitizen::visualX()''')
replace(movement,
        'std::clamp(stepProgress / stepDuration, 0.0, 1.0)',
        'movementFraction(stepProgress, stepDuration)', 2)

# Condition cleanups only; cache budgets and draw behavior stay intact.
replace('src/rendering/SettlementNaturalFeatureRenderer.cpp',
        '                 chunk.navigation != map.objectState().navigationVersion()) &&\n                true)',
        '                 chunk.navigation != map.objectState().navigationVersion()))')
replace('src/rendering/WorldGridRenderer.cpp',
        '''            if (found == terrainChunks_.end())
            {
                continue;
            }
            found->second.lastUsed = terrainFrame_;''',
        '            found->second.lastUsed = terrainFrame_;')

replace(loop_tests, '#include "TestFramework.h"\n',
        '#include "TestFramework.h"\n#include "interaction/SettlementCommandController.h"\n#include "world/generation/GenerationNoise.h"\n#include <limits>\n')
replace(loop_tests, '    struct SettlementActivityTestFixture\n    {\n',
        '''    struct SettlementActivityTestFixture
    {
        static void executeWithoutPathSearch(
            SettlementMap& map,
            SettlementCitizenState& citizens,
            SettlementCitizen& citizen,
            double minute
        )
        {
            map.activities.pathsRemaining_ = 0;
            map.activities.execute(map, citizens, citizen, minute, 1);
        }
''')
regressions = Path('.github/paladin-audit-regressions.txt').read_text()
replace(loop_tests, 'void runSettlementSimulationLoopTests()\n{\n',
        'void runSettlementSimulationLoopTests()\n{\n' + regressions)
subprocess.run(['git', 'diff', '--check'], check=True)
print('Applied scoped bug fixes and regressions.', flush=True)
