from pathlib import Path
import subprocess

def replace(filename, old, new, count=1):
    p = Path(filename)
    s = p.read_text()
    assert s.count(old) == count, (filename, s.count(old), old)
    p.write_text(s.replace(old, new))

route = 'src/simulation/systems/SettlementActivitySelection.cpp'
replace(route,
        '            double duration;\n            ~RestorePosition()',
        '''            double duration;
            const SettlementMap& map;
            const SettlementNavigation& navigation;
            const CitizenMovementPolicy& movementPolicy;
            ~RestorePosition()''')
replace(route,
        '''                    if (progress > 0)
                    {
                        citizen.stepDuration = duration;
                    }
                }
            }
        } restore{c, original, exitPath, successful, progress, duration};''',
        '''                    if (progress > 0)
                    {
                        citizen.stepDuration = duration;
                    }
                    else
                    {
                        // A prepended house exit changes the first edge even
                        // when the outdoor leg needs no path search.
                        citizen.stepDuration = navigation.stepCost(
                            map,
                            original,
                            citizen.path.front(),
                            movementPolicy
                        );
                    }
                }
            }
        } restore{c, original, exitPath, successful, progress, duration,
                  map, citizens.navigation_, citizens.movementPolicy};''')

replace('src/simulation/systems/SettlementActivitySystem.h',
        '        bool enterHome(SettlementMap&, SettlementCitizen&);',
        '''        bool enterHome(
            SettlementMap&,
            const SettlementCitizenState&,
            SettlementCitizen&
        );''')
replace('src/simulation/systems/SettlementActivitySystem.cpp',
        'enterHome(map, c)', 'enterHome(map, citizens, c)', 4)
replace('src/simulation/systems/SettlementCitizenNeeds.cpp',
        '''    bool SettlementActivitySystem::enterHome(
        SettlementMap& map,
        SettlementCitizen& c
    )''',
        '''    bool SettlementActivitySystem::enterHome(
        SettlementMap& map,
        const SettlementCitizenState& citizens,
        SettlementCitizen& c
    )''')
replace('src/simulation/systems/SettlementCitizenNeeds.cpp',
        '''            c.path = {interior};
            c.pathIndex = 0;
            c.stepProgress = 0;
            c.explicitMovement = true;''',
        '''            c.path = {interior};
            c.pathIndex = 0;
            c.stepProgress = 0;
            c.stepDuration = citizens.navigation_.stepCost(
                map, c.tilePosition, interior, citizens.movementPolicy
            );
            c.explicitMovement = true;''')

loop = 'tests/SettlementSimulationLoopTests.cpp'
replace(loop, '    struct SettlementActivityTestFixture\n    {\n',
        '''    struct SettlementActivityTestFixture
    {
        static bool routeWithoutPathSearch(
            SettlementMap& map,
            SettlementCitizenState& citizens,
            SettlementCitizen& citizen,
            SettlementTilePosition goal
        )
        {
            map.activities.pathsRemaining_ = 0;
            return map.activities.route(map, citizens, citizen, {goal, 1, 1}, true);
        }
        static bool enterHome(
            SettlementMap& map,
            const SettlementCitizenState& citizens,
            SettlementCitizen& citizen
        )
        {
            return map.activities.enterHome(map, citizens, citizen);
        }
''')
replace(loop,
        '''        // Select a nonzero interior offset deterministically. With no path
        // budget, the outdoor trip is rejected and Home uses its fallback.''',
        '''        // Select a nonzero offset deterministically. Home may use an
        // interior fallback or a zero-search exit; both must initialize timing.''')
needle = '    std::cout << "[audit] direct interior path timing passed\\n";\n'
extra = '''
    // A blocked exit forces Home's one-tile interior fallback specifically.
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        const auto homeId = completed(map, SettlementObjectTypes::House, {{12, 12}, 3, 3});
        const auto* home = map.objectState().completedObject(homeId);
        PALADIN_CHECK(home && home->door);
        map.grid().tile(outsideDoor(home->footprint, *home->door))->terrain = TerrainType::Water;
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.homeId = homeId;
        c.insideHome = true;
        c.tilePosition = {13, 13};
        c.destination = c.tilePosition;
        c.task.kind = CitizenTaskKind::Home;
        c.task.object = homeId;
        c.stepDuration = 99;
        for (std::uint64_t sequence = 0; sequence < 100; ++sequence)
        {
            const auto r = GenerationNoise::mix(c.id.value() ^ (sequence + 1));
            if (r % 3 != 1 || (r >> 8) % 3 != 1)
            {
                c.choiceSequence = sequence;
                break;
            }
        }
        SettlementActivityTestFixture::executeWithoutPathSearch(map, citizens, c, 600);
        PALADIN_CHECK(c.path.size() == 1);
        PALADIN_CHECK(home->footprint.contains(c.path.front()));
        PALADIN_CHECK(c.stepDuration == citizens.navigationDiagnostics().stepCost(
            map, c.tilePosition, c.path.front(), citizens.movementPolicy
        ));
    }
    // Door-entry and prepended-exit paths use the first physical edge, while
    // a route replacement halfway through a step preserves its progress.
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        const auto homeId = completed(map, SettlementObjectTypes::House, {{12, 12}, 3, 3});
        const auto* home = map.objectState().completedObject(homeId);
        PALADIN_CHECK(home && home->door);
        const auto outside = outsideDoor(home->footprint, *home->door);
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.homeId = homeId;
        c.tilePosition = outside;
        c.destination = outside;
        c.stepDuration = 99;
        PALADIN_CHECK(!SettlementActivityTestFixture::enterHome(map, citizens, c));
        PALADIN_CHECK(c.path.size() == 1 && c.path.front() == *home->door);
        PALADIN_CHECK(c.stepDuration == 1);
        c.tilePosition = {13, 13};
        c.destination = c.tilePosition;
        c.insideHome = true;
        c.path.clear();
        c.pathIndex = 0;
        c.stepProgress = 0;
        c.stepDuration = 99;
        PALADIN_CHECK(SettlementActivityTestFixture::routeWithoutPathSearch(
            map, citizens, c, outside
        ));
        PALADIN_CHECK(!c.path.empty() && c.path.back() == outside);
        PALADIN_CHECK(c.stepDuration == citizens.navigationDiagnostics().stepCost(
            map, c.tilePosition, c.path.front(), citizens.movementPolicy
        ));
        citizens.movementPolicy.diagonalCost = 1.75;
        c.path = {{14, 14}};
        c.pathIndex = 0;
        c.stepDuration = 1.75;
        c.stepProgress = .7;
        const auto x = c.visualX(), y = c.visualY();
        PALADIN_CHECK(SettlementActivityTestFixture::routeWithoutPathSearch(
            map, citizens, c, outside
        ));
        PALADIN_CHECK(c.path.front().x == 14 && c.path.front().y == 14);
        PALADIN_CHECK(c.stepProgress == .7 && c.stepDuration == 1.75);
        PALADIN_CHECK(std::abs(c.visualX() - x) < 1e-9);
        PALADIN_CHECK(std::abs(c.visualY() - y) < 1e-9);
    }
    std::cout << "[audit] entry, exit, fallback and mid-step rerouting passed\\n";
'''
replace(loop, needle, needle + extra)
subprocess.run(['git', 'diff', '--check'], check=True)
