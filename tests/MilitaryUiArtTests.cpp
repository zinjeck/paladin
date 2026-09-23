#include "TestFramework.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/CityRenderer.h"
#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/SettlementCitizenRenderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldArmyPresentation.h"
#include "rendering/WorldObjectRenderer.h"
#include "rendering/WorldPixelGrid.h"
#include "simulation/DiplomacySystem.h"
#include "simulation/MilitarySystem.h"
#include "simulation/Simulation.h"
#include "ui/CityHud.h"
#include "ui/DiplomacyPanel.h"
#include "ui/GrayUiRenderer.h"
#include "ui/MilitaryPanel.h"
#include "ui/RealmFlagRenderer.h"
#include "world/RealmFlagDesigns.h"
#include "world/World.h"
#include "world/generation/SettlementMapGenerator.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/WorkplaceCompound.h"
#include "world/settlements/objects/jobs/market/MarketJob.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <vector>

namespace
{
    using namespace Paladin;
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    std::vector<std::uint32_t> capture(
        SDL_Window* window,
        const std::string& name
    )
    {
        Surface raw(
            SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr),
            SDL_DestroySurface
        );
        PALADIN_CHECK(raw);
        Surface rgba(
            SDL_ConvertSurface(raw.get(), SDL_PIXELFORMAT_RGBA32),
            SDL_DestroySurface
        );
        PALADIN_CHECK(rgba);
        if (const char* root = SDL_getenv("PALADIN_SMOKE_SCREENSHOTS"))
        {
            std::filesystem::create_directories(root);
            PALADIN_CHECK(IMG_SavePNG(
                rgba.get(),
                (std::filesystem::path(root) / name).string().c_str()
            ));
        }
        std::vector<std::uint32_t> result;
        for (int y = 0; y < rgba->h; ++y)
        {
            const auto* row = reinterpret_cast<const std::uint32_t*>(
                static_cast<const Uint8*>(rgba->pixels) + y * rgba->pitch
            );
            result.insert(result.end(), row, row + rgba->w);
        }
        return result;
    }
    void requireBlocks(
        const std::vector<std::uint32_t>& pixels,
        int width,
        int height,
        int pitch,
        bool translated = false
    )
    {
        if (translated)
        {
            // CityPixelView shifts the entire lattice by a physical-pixel
            // residual. Every layer must still share one pitch and phase.
            bool aligned = false;
            for (int py = 0; py < pitch && !aligned; ++py)
            {
                for (int px = 0; px < pitch && !aligned; ++px)
                {
                    bool matches = true;
                    for (int y = 0; y < height && matches; ++y)
                    {
                        for (int x = 0; x < width && matches; ++x)
                        {
                            const int bx =
                                std::max(0, x - (x + pitch - px) % pitch);
                            const int by =
                                std::max(0, y - (y + pitch - py) % pitch);
                            matches = pixels[y * width + x] ==
                                      pixels[by * width + bx];
                        }
                    }
                    aligned = matches;
                }
            }
            PALADIN_CHECK(aligned);
            PALADIN_CHECK(
                std::set<std::uint32_t>(pixels.begin(), pixels.end()).size() > 8
            );
            return;
        }
        std::set<std::uint32_t> colors;
        for (int y = 0; y < height; y += pitch)
        {
            for (int x = 0; x < width; x += pitch)
            {
                const auto value = pixels[y * width + x];
                colors.insert(value);
                for (int dy = 0; dy < pitch && y + dy < height; ++dy)
                {
                    for (int dx = 0; dx < pitch && x + dx < width; ++dx)
                    {
                        PALADIN_CHECK(
                            pixels[(y + dy) * width + x + dx] == value
                        );
                    }
                }
            }
        }
        PALADIN_CHECK(colors.size() > 8);
    }
    void actors(Renderer& renderer, SDL_Window* window, SceneSpriteLibrary& art)
    {
        SettlementCitizenState people;
        PALADIN_CHECK(people.initialize(1, 47));
        auto& person =
            const_cast<SettlementCitizen&>(people.citizens().front());
        person.tilePosition = {10, 10};
        person.insideHome = false;
        person.sex = CitizenSex::Male;
        person.hasVisualSnapshot = false;
        Camera2D camera(10.5, 10.5);
        camera.setZoom(16);
        TileRenderMetrics metrics;
        SettlementCitizenRenderer citizens;
        art.setTime(42);
        const auto render = [&](const std::string& name)
        {
            renderer.beginFrame();
            {
                WorldPixelScene scene(
                    renderer,
                    metrics.scaledTilePixels(camera.zoom())
                );
                renderer.fillRectangle(0, 0, 960, 640, {32, 44, 67, 255});
                citizens.render(
                    renderer,
                    people,
                    camera,
                    metrics,
                    nullptr,
                    1,
                    nullptr,
                    &art
                );
                citizens.renderAnnotations(
                    renderer,
                    metrics.scaledTilePixels(camera.zoom())
                );
            }
            const auto result = capture(window, name);
            renderer.endFrame();
            return result;
        };
        person.path = {{11, 10}};
        std::vector<std::vector<std::uint32_t>> walk;
        for (int frame = 0; frame < 4; ++frame)
        {
            person.walkDistance = frame * .25;
            walk.push_back(
                render("pr26-walk-" + std::to_string(frame) + ".png")
            );
            requireBlocks(walk.back(), 960, 640, 4);
            for (int previous = 0; previous < frame; ++previous)
            {
                PALADIN_CHECK(walk[previous] != walk.back());
            }
        }
        PALADIN_CHECK(render("pr26-walk-paused.png") == walk.back());
        person.path.clear();
        person.task.kind = CitizenTaskKind::Gather;
        std::vector<std::vector<std::uint32_t>> gather;
        for (int frame = 0; frame < 4; ++frame)
        {
            person.workAnimationMinutes = frame * 2;
            gather.push_back(
                render("pr26-gather-" + std::to_string(frame) + ".png")
            );
            requireBlocks(gather.back(), 960, 640, 4);
            for (int previous = 0; previous < frame; ++previous)
            {
                PALADIN_CHECK(gather[previous] != gather.back());
            }
        }
        person.task = {};
        person.activity = CitizenActivity::Sleeping;
        const auto sleep = render("pr26-sleep-close.png");
        requireBlocks(sleep, 960, 640, 4);
        PALADIN_CHECK(render("pr26-sleep-paused.png") == sleep);
        camera.setZoom(4);
        render("pr26-sleep-normal.png");
        person.activity = CitizenActivity::Idle;
        person.soldierId = SoldierId{1};
        camera.setZoom(16);
        const auto soldier = render("pr26-soldier-close.png");
        person.soldierId = {};
        const auto civilian = render("pr26-citizen-close.png");
        PALADIN_CHECK(soldier != civilian);
        person.militaryDeployed = true;
        const auto absent = render("pr26-deployed-hidden.png");
        PALADIN_CHECK(
            std::all_of(
                absent.begin(),
                absent.end(),
                [&](auto p) { return p == absent.front(); }
            )
        );
    }
    void citizenPicking(Renderer& renderer, SDL_Window* window)
    {
        SettlementGrid grid(32, 32);
        for (int y = 0; y < 32; ++y)
        {
            for (int x = 0; x < 32; ++x)
            {
                auto& t = *grid.tile({x, y});
                t.terrain = TerrainType::Land;
                t.biome = BiomeType::Plain;
            }
        }
        SettlementMap map(std::move(grid), {0, 0}, 1, 1, 32, 3301);
        auto house = *SettlementObjectCatalog::definition("house");
        house.bypassesConstruction = true;
        PALADIN_CHECK(map.objectState().placeCompletedObject(
            map.grid(),
            house,
            {{8, 8}, 5, 5}
        ));
        map.naturalFeatures().clear({{8, 8}, 5, 5});
        SettlementCitizenState people;
        PALADIN_CHECK(people.initialize(1, 47));
        auto& person =
            const_cast<SettlementCitizen&>(people.citizens().front());
        person.tilePosition = {10, 10};
        person.insideHome = true;
        person.hasVisualSnapshot = false;
        CityRenderer city;
        city.presentation.roofsVisible = false;
        city.presentation.shadowsVisible = false;
        city.presentation.cloudsEnabled = false;
        city.animationTimeOverride = 42;
        SettlementInspectionController inspection;
        SettlementCommandController commands;
        SettlementObjectPlacementController placement;
        Camera2D camera(10.52, 10.49);
        TileRenderMetrics metrics;
        const auto draw = [&](const std::string& name)
        {
            renderer.beginFrame();
            city.render(
                renderer,
                map,
                camera,
                metrics,
                placement,
                commands,
                people,
                inspection,
                1,
                12
            );
            auto image = capture(window, name);
            renderer.endFrame();
            return image;
        };
        for (double zoom : {4., 13.3, 16.})
        {
            camera.setZoom(zoom);
            for (bool child : {false, true})
            {
                person.child = child;
                person.militaryDeployed = true;
                const auto until = SDL_GetTicks() + 500;
                do
                {
                    renderer.beginFrame();
                    city.render(
                        renderer,
                        map,
                        camera,
                        metrics,
                        placement,
                        commands,
                        people,
                        inspection,
                        1,
                        12
                    );
                    renderer.endFrame();
                    SDL_Delay(1);
                } while (SDL_GetTicks() < until);
                const auto backdrop = draw("citizen-pick-backdrop.png");
                person.militaryDeployed = false;
                const auto shown = draw(
                    "citizen-pick-" + std::to_string(zoom) +
                    (child ? "-child.png" : "-adult.png")
                );
                int hits = 0, misses = 0;
                for (int y = 230; y < 342; ++y)
                {
                    for (int x = 400; x < 560; ++x)
                    {
                        const bool changed =
                            backdrop[y * 960 + x] != shown[y * 960 + x];
                        const auto picked = city.citizenAtScreen(
                            float(x) + .5F,
                            float(y) + .5F,
                            map,
                            people
                        );
                        if (bool(picked) != changed)
                        {
                            std::cerr << "pick mismatch zoom=" << zoom
                                      << " child=" << child << " x=" << x
                                      << " y=" << y << " hit=" << bool(picked)
                                      << " changed=" << changed << "\n";
                        }
                        PALADIN_CHECK(bool(picked) == changed);
                        if (picked)
                        {
                            ++hits;
                            PALADIN_CHECK(picked == person.id);
                        }
                        else
                        {
                            ++misses;
                        }
                    }
                }
                PALADIN_CHECK(hits > 0 && misses > 0);
                // The tile still selects its house when no sprite pixel is hit.
                PALADIN_CHECK(inspection.selectAt(
                    {10, 10},
                    map.objectState(),
                    people,
                    true,
                    nullptr,
                    false
                ));
                PALADIN_CHECK(
                    inspection.kind() ==
                    SettlementInspectionKind::CompletedObject
                );
                inspection.clear();
            }
        }
        std::cout << "Citizen alpha picking: adult/child bodies, three zooms, "
                     "camera residual and house pass-through passed\n";
    }
    void militaryAndCities(
        Renderer& renderer,
        SDL_Window* window,
        SceneSpriteLibrary& art
    )
    {
        WorldGenerationSettings settings;
        settings.width = settings.height = 64;
        settings.seed = 711;
        settings.populateAiRealms = false;
        Simulation sim(settings);
        auto& world = sim.world();
        for (int y = 0; y < 64; ++y)
        {
            for (int x = 0; x < 64; ++x)
            {
                auto& tile = *world.grid().tile({x, y});
                tile.terrain = TerrainType::Land;
                tile.biome = BiomeType::Plain;
            }
        }
        const auto city = sim.foundPlayerCapital(
            {32, 32},
            {"Art Realm", "Art Folk", "Caerwyn Haven", {}, "civic", {}}
        );
        PALADIN_CHECK(city);
        SettlementMapGenerationSettings local;
        local.localTilesPerWorldTile = 4;
        PALADIN_CHECK(sim.prepareSettlementMap(city, local));
        auto& map = *sim.settlementMap(city);
        for (int y = 0; y < map.grid().height(); ++y)
        {
            for (int x = 0; x < map.grid().width(); ++x)
            {
                map.grid().tile({x, y})->terrain = TerrainType::Land;
            }
        }
        for (const auto type : {"city_keep", "barracks"})
        {
            auto definition = *SettlementObjectCatalog::definition(type);
            definition.bypassesConstruction = true;
            const SettlementObjectFootprint bounds =
                type == std::string_view("city_keep")
                    ? SettlementObjectFootprint{{2, 2}, 5, 7}
                    : SettlementObjectFootprint{{11, 2}, 5, 5};
            PALADIN_CHECK(map.objectState().placeCompletedObject(
                map.grid(),
                definition,
                bounds
            ));
            map.naturalFeatures().clear(bounds);
        }
        map.logistics.synchronize(map.objectState(), 360);
        world.settlement(city)
            ->simulationState()
            .citizens()
            .placeUnpositionedCitizens(map);
        MilitaryPanel panel;
        GrayUiRenderer ui;
        BitmapFontRenderer font;
        PALADIN_CHECK(
            font.measureWidth("iii", 2) < font.measureWidth("MMM", 2)
        );
        panel.toggle(city);
        panel.layout(320, 360, world, sim.playerRealmId());
        PALADIN_CHECK(panel.bounds().x + panel.bounds().width <= 320);
        PALADIN_CHECK(panel.bounds().y + panel.bounds().height <= 360);
        panel.close();
        panel.toggle(city);
        panel.layout(960, 640, world, sim.playerRealmId());
        const auto click =
            [&](MilitaryPanel::Action action, ArmyId id = ArmyId{})
        {
            const auto b = panel.controlBounds(action, id);
            PALADIN_CHECK(b);
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.button = SDL_BUTTON_LEFT;
            event.button.x = b->x + b->width * .5F;
            event.button.y = b->y + b->height * .5F;
            PALADIN_CHECK(panel.handle(event, world, sim.playerRealmId()));
            event.type = SDL_EVENT_MOUSE_BUTTON_UP;
            PALADIN_CHECK(panel.handle(event, world, sim.playerRealmId()));
        };
        using A = MilitaryPanel::Action;
        click(A::New);
        PALADIN_CHECK(!panel.selection() && world.armies().empty());
        PALADIN_CHECK(
            MilitarySystem::recruit(world, sim.playerRealmId(), city, 1) ==
            MilitaryResult::Success
        );
        PALADIN_CHECK(
            MilitarySystem::recruit(world, sim.playerRealmId(), city, 1) ==
            MilitaryResult::Success
        );
        panel.layout(960, 640, world, sim.playerRealmId());
        PALADIN_CHECK(MilitarySystem::available(world, city) == 2);
        click(A::New);
        const auto unit = panel.selection();
        PALADIN_CHECK(unit);
        PALADIN_CHECK(world.army(unit)->soldierCount() == 1);
        click(A::AddOne);
        PALADIN_CHECK(world.army(unit)->soldierCount() == 2);
        renderer.beginFrame();
        renderer.fillRectangle(0, 0, 960, 640, {35, 87, 71, 255});
        panel.render(renderer, ui, world, sim.playerRealmId(), &art);
        capture(window, "pr26-military-panel.png");
        renderer.endFrame();
        click(A::RemoveOne);
        PALADIN_CHECK(world.army(unit)->soldierCount() == 1);
        click(A::New);
        const auto second = panel.selection();
        PALADIN_CHECK(second && second != unit);
        const auto firstCard = panel.controlBounds(A::Row, unit),
                   secondCard = panel.controlBounds(A::Row, second);
        PALADIN_CHECK(
            firstCard && secondCard && firstCard->height > firstCard->width
        );
        PALADIN_CHECK(
            firstCard->y == secondCard->y &&
            firstCard->x + firstCard->width < secondCard->x
        );
        renderer.beginFrame();
        renderer.fillRectangle(0, 0, 960, 640, {35, 87, 71, 255});
        panel.render(renderer, ui, world, sim.playerRealmId(), &art);
        capture(window, "pr29-compact-unit-grid.png");
        renderer.endFrame();
        click(A::Disband);
        PALADIN_CHECK(!world.army(second));
        // PR30 discharge returns an unemployed civilian, not an automatic
        // barracks reserve. Rehire explicitly for the count-only render check.
        PALADIN_CHECK(
            MilitarySystem::recruit(world, sim.playerRealmId(), city, 1) ==
            MilitaryResult::Success
        );
        click(A::Row, unit);
        const auto soldier = world.army(unit)->soldiers().front();
        const auto origin = world.soldier(soldier)->homeSettlementId();
        const auto person = world.soldier(soldier)->sourceCitizenId();
        const auto age = world.settlement(origin)
                             ->simulationState()
                             .citizens()
                             .citizen(person)
                             ->ageYears;
        click(A::ToGarrison);
        PALADIN_CHECK(world.army(unit)->garrisonSettlementId() == city);
        // Controls on the other side must not resize this selected garrison.
        click(A::AddOne);
        PALADIN_CHECK(world.army(unit)->soldierCount() == 1);
        click(A::GarrisonAddOne);
        PALADIN_CHECK(world.army(unit)->soldierCount() == 2);
        click(A::GarrisonRemoveOne);
        PALADIN_CHECK(world.army(unit)->soldierCount() == 1);
        PALADIN_CHECK(world.soldier(soldier)->homeSettlementId() == origin);
        PALADIN_CHECK(
            world.settlement(origin)
                ->simulationState()
                .citizens()
                .citizen(person)
                ->ageYears == age
        );
        renderer.beginFrame();
        renderer.fillRectangle(0, 0, 960, 640, {35, 87, 71, 255});
        panel.render(renderer, ui, world, sim.playerRealmId(), &art);
        capture(window, "military-field-garrison.png");
        renderer.endFrame();
        click(A::ToField);
        PALADIN_CHECK(!world.army(unit)->garrisoned());
        click(A::Focus);
        PALADIN_CHECK(panel.takeFocus() == unit && !panel.isOpen());
        // Universal markers do not grow or acquire sprawl with population.
        WorldObjectRenderer objects;
        Camera2D camera(32.5, 33.5);
        camera.setWorldZoom(16);
        const auto worldFrame = [&](const std::string& name,
                                    const SceneSpriteLibrary* library,
                                    ArmyId selected = ArmyId{})
        {
            renderer.beginFrame();
            renderer.fillRectangle(0, 0, 960, 640, {35, 87, 71, 255});
            objects.render(
                renderer,
                world,
                camera,
                64,
                false,
                worldPresentationState(64),
                true,
                {},
                std::nullopt,
                {},
                library,
                selected
            );
            const auto pixels = capture(window, name);
            renderer.endFrame();
            return pixels;
        };
        const auto fallback = worldFrame("pr26-world-without-art.png", nullptr);
        auto settlement = worldFrame("pr26-world-settlement.png", &art);
        PALADIN_CHECK(settlement != fallback);
        PALADIN_CHECK(
            MilitarySystem::resizeUnit(world, sim.playerRealmId(), unit, 1) ==
            MilitaryResult::Success
        );
        const auto two =
            worldFrame("pr27-one-character-two-soldiers.png", &art);
        bool labelChanged = false;
        const auto countPlate = worldArmyCountBounds(
            480,
            256,
            2,
            64,
            worldArmySprite(art, world, *world.army(unit))
        );
        for (int y = 0; y < 640; ++y)
        {
            for (int x = 0; x < 960; ++x)
            {
                if (countPlate.contains(float(x), float(y)))
                {
                    labelChanged |= two[y * 960 + x] != settlement[y * 960 + x];
                }
                else
                {
                    PALADIN_CHECK(two[y * 960 + x] == settlement[y * 960 + x]);
                }
            }
        }
        PALADIN_CHECK(labelChanged);
        const auto unselected = worldFrame("pr29-army-unselected.png", &art);
        const auto selected = worldFrame("pr29-army-selected.png", &art, unit);
        const auto body = worldArmySpriteBounds(
            480,
            256,
            64,
            worldArmySprite(art, world, *world.army(unit))
        );
        std::size_t contour = 0;
        for (int y = 0; y < 640; ++y)
        {
            for (int x = 0; x < 960; ++x)
            {
                if (unselected[y * 960 + x] != selected[y * 960 + x])
                {
                    ++contour;
                    PALADIN_CHECK(
                        x >= body.x - 5 && x <= body.x + body.width + 5 &&
                        y >= body.y - 5 && y <= body.y + body.height + 5
                    );
                    // No rectangle spanning the bounding box: transparent
                    // sprite corners remain untouched by the selected contour.
                    PALADIN_CHECK(!(x < body.x + 3 && y < body.y + 3));
                }
            }
        }
        PALADIN_CHECK(contour > 20);
        std::vector<std::uint32_t> far;
        for (const auto* library :
             {static_cast<const SceneSpriteLibrary*>(nullptr),
              static_cast<const SceneSpriteLibrary*>(&art)})
        {
            renderer.beginFrame();
            renderer.fillRectangle(0, 0, 960, 640, {35, 87, 71, 255});
            objects.render(
                renderer,
                world,
                camera,
                4,
                false,
                worldPresentationState(4),
                true,
                {},
                std::nullopt,
                {},
                library,
                unit
            );
            const auto image = capture(window, "pr29-army-far.png");
            renderer.endFrame();
            if (far.empty())
            {
                far = image;
            }
            else
            {
                PALADIN_CHECK(far == image);
            }
        }
        for (const double scale : {.25, 4., 16., 64., 256.})
        {
            const auto* sprite = worldArmySprite(art, world, *world.army(unit));
            const auto body = worldArmySpriteBounds(480, 320, scale, sprite);
            PALADIN_CHECK(body.height >= 20.F && body.height <= std::max(24.,scale*.70));
            const bool visible = worldArmyVisibility(scale) > .001F;
            const auto plate = worldArmyCountBounds(480, 320, 2, scale, sprite);
            PALADIN_CHECK(
                worldArmyHitTest(
                    body.x + body.width * .5,
                    body.y + 3,
                    480,
                    320,
                    scale,
                    sprite,
                    2
                ) == visible
            );
            PALADIN_CHECK(
                worldArmyHitTest(
                    480,
                    plate.y + plate.height * .5,
                    480,
                    320,
                    scale,
                    sprite,
                    2
                ) == visible
            );
            PALADIN_CHECK(!worldArmyHitTest(5, 5, 480, 320, scale, sprite, 2));
        }
        PALADIN_CHECK(
            world.settlement(city)->simulationState().spawnCitizens(120)
        );
        auto town = worldFrame("pr26-world-town.png", &art);
        PALADIN_CHECK(town == two);
        PALADIN_CHECK(
            world.settlement(city)->simulationState().spawnCitizens(896)
        );
        auto cityPixels = worldFrame("pr26-world-city.png", &art);
        PALADIN_CHECK(cityPixels == town);
        auto fortressProfile = defaultSettlementFoundationProfile();
        fortressProfile.kind = SettlementKind::Fortress;
        const auto fortress = world.foundSettlement(
            {48, 32},
            sim.playerRealmId(),
            fortressProfile
        );
        PALADIN_CHECK(fortress && world.renameSettlement(fortress, "Westgate"));
        camera.setPosition(48.5, 33.5);
        const auto fortImage = worldFrame("pr29-fortress-icon.png", &art);
        PALADIN_CHECK(
            world.settlement(fortress)->simulationState().spawnCitizens(900)
        );
        PALADIN_CHECK(
            worldFrame("pr29-fortress-population-invariant.png", &art) ==
            fortImage
        );
        camera.setPosition(32.5, 33.5);
        PALADIN_CHECK(
            MilitarySystem::orderMove(
                world,
                sim.playerRealmId(),
                unit,
                {33, 32}
            ) == MilitaryResult::Success
        );
        MilitarySystem::tick(world, 360, Army::MarchMinutesPerTile * .5);
        const auto march = worldFrame("pr26-world-marching.png", &art);
        PALADIN_CHECK(
            worldFrame("pr26-world-marching-paused.png", &art) == march
        );
        renderer.beginFrame();
        renderer.fillRectangle(0, 0, 960, 640, {32, 44, 67, 255});
        ui.drawPanel(renderer, {80, 60, 800, 520});
        ui.drawLabel(renderer, "Paladin | Population | Realm", 112, 92, 3);
        ui.drawLabel(renderer, "Il1 O0 S5 B8 rn m   0123456789", 112, 140, 2);
        for (int state = 0; state < 5; ++state)
        {
            ui.drawButton(
                renderer,
                {112.F, 196.F + 64 * state, 736, 44},
                "Build: Housing 20 Wood",
                state == 1,
                state == 2,
                state == 3,
                state != 4
            );
        }
        capture(window, "pr26-ui-states.png");
        renderer.endFrame();
    }
    void selectionAndConstruction(Renderer& renderer, SDL_Window* window)
    {
        GrayUiRenderer ui;
        const auto verify = [&](bool skin)
        {
            renderer.beginFrame();
            renderer.fillRectangle(0, 0, 960, 640, {32, 44, 67, 255});
            const UiRectangle button{70, 100, 252, 70}, card{360, 100, 252, 70},
                pressed{70, 220, 252, 70};
            ui.drawButton(renderer, button, "Tribal", false, false, true, true);
            ui.drawChoiceCard(renderer, card, "Civic", true, false, true);
            ui.drawButton(
                renderer,
                pressed,
                "Selected + pressed",
                true,
                true,
                true,
                true
            );
            const auto pixels = capture(
                window,
                skin ? "pr27-selection-skinned.png"
                     : "pr27-selection-fallback.png"
            );
            renderer.endFrame();
            for (const auto b : {button, card, pressed})
            {
                const auto gold = [&](int x, int y)
                {
                    const auto* rgb =
                        reinterpret_cast<const Uint8*>(&pixels[y * 960 + x]);
                    PALADIN_CHECK(
                        rgb[0] == 235 && rgb[1] == 196 && rgb[2] == 107
                    );
                };
                for (int x = int(b.x); x < int(b.x + b.width); ++x)
                {
                    gold(x, int(b.y));
                    gold(x, int(b.y + b.height) - 1);
                }
                for (int y = int(b.y); y < int(b.y + b.height); ++y)
                {
                    gold(int(b.x), y);
                    gold(int(b.x + b.width) - 1, y);
                }
            }
        };
        verify(false);
        std::vector<RenderColor> skinPixels(24 * 24, {8, 15, 27, 255});
        ButtonSpriteSkin skin;
        skin.atlas = renderer.createTextureFromPixels(24, 24, skinPixels);
        for (auto& frame : skin.frames)
        {
            frame = {0, 0, 24, 24};
        }
        skin.sliceBorder = 4;
        ui.setButtonSkin("default", skin);
        verify(true);
        CityHud hud;
        for (const bool fortress : {false, true})
        {
            hud.setFortress(fortress);
            hud.setSettlementStatus(true, 8);
            hud.layout(960, 640);
            const float x = (960.F - 76.F * (fortress ? 4 : 8)) * .5F +
                            76 * (fortress ? 3 : 6) + 38;
            const auto press = [&](float px, float py)
            {
                PALADIN_CHECK(hud.pointerPressed(px, py));
                return hud.pointerReleased(px, py);
            };
            press(x, 608);
            renderer.beginFrame();
            renderer.fillRectangle(0, 0, 960, 640, {35, 87, 71, 255});
            hud.render(renderer, ui);
            capture(
                window,
                fortress ? "pr27-fortress-rule-buildings.png"
                         : "pr27-rule-buildings.png"
            );
            renderer.endFrame();
            PALADIN_CHECK(hud.containsInteractivePoint(x, 538));
            PALADIN_CHECK(press(x, 538) == CityHudAction::BeginObjectPlacement);
            PALADIN_CHECK(
                hud.selectedObjectTypeId() == SettlementObjectTypes::Barracks
            );
            hud.closeCategoryMenus();
            press(x, 608);
            PALADIN_CHECK(press(x, 468) == CityHudAction::BeginObjectPlacement);
            PALADIN_CHECK(
                hud.selectedObjectTypeId() ==
                SettlementObjectTypes::ArmySupplyDepot
            );
            hud.closeCategoryMenus();
        }
    }
    void diplomacyAndOverflow(
        Renderer& renderer,
        SDL_Window* window,
        SceneSpriteLibrary& art
    )
    {
        WorldGenerationSettings settings;
        settings.width = 128;
        settings.height = 64;
        settings.seed = 573;
        settings.populateAiRealms = false;
        World world(settings);
        for (int y = 0; y < world.grid().height(); ++y)
        {
            for (int x = 0; x < world.grid().width(); ++x)
            {
                world.grid().tile({x, y})->terrain = TerrainType::Land;
                world.grid().tile({x, y})->biome = BiomeType::Plain;
            }
        }
        world.grid().terrainChanged();
        const auto actor = world.createRealm(), target = world.createRealm();
        const auto city = world.foundCapitalSettlement(
            {12, 24},
            actor,
            {"Amber Crown", "Amberfolk", "Amber", {}, "civic"}
        );
        PALADIN_CHECK(city);
        PALADIN_CHECK(world.foundCapitalSettlement(
            {24, 24},
            target,
            {"Blue Confederacy", "Bluefolk", "Blue", {}, "tribal"}
        ));
        world.realm(actor)->treasury->balance = 100000;
        world.realm(target)->treasury->balance = 100;
        DiplomacyPanel panel;
        GrayUiRenderer ui;
        panel.open();
        panel.layout(960, 640, world, actor);
        PALADIN_CHECK(!panel.selection());
        PALADIN_CHECK(!panel.actionBounds(DiplomaticAction::Alliance));
        const auto frame = [&](const char* name)
        {
            renderer.beginFrame();
            renderer.fillRectangle(0, 0, 960, 640, {32, 44, 67, 255});
            panel.render(renderer, ui, world, actor);
            capture(window, name);
            renderer.endFrame();
        };
        frame("pr29-diplomacy-blank.png");
        const auto click = [&](std::optional<UiRectangle> b)
        {
            PALADIN_CHECK(b);
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.button = SDL_BUTTON_LEFT;
            event.button.x = b->x + b->width * .5F;
            event.button.y = b->y + b->height * .5F;
            PALADIN_CHECK(panel.handle(event, world, actor));
            event.type = SDL_EVENT_MOUSE_BUTTON_UP;
            PALADIN_CHECK(panel.handle(event, world, actor));
        };
        click(panel.realmBounds(target));
        PALADIN_CHECK(panel.selection() == target);
        float previous = 0;
        for (const auto action :
             {DiplomaticAction::Alliance,
              DiplomaticAction::Gift,
              DiplomaticAction::Tribute,
              DiplomaticAction::Trade,
              DiplomaticAction::War})
        {
            const auto b = panel.actionBounds(action);
            PALADIN_CHECK(b && b->y > previous);
            previous = b->y;
        }
        frame("pr29-diplomacy-actions.png");
        click(panel.actionBounds(DiplomaticAction::Alliance));
        PALADIN_CHECK(world.diplomacy().between(actor, target)->allied);
        const auto gift = DiplomacySystem::suggestedGift(world, actor, target);
        click(panel.actionBounds(DiplomaticAction::Gift));
        PALADIN_CHECK(
            gift > 0 &&
            world.realm(actor)->treasury->balance == 100000 - gift &&
            world.realm(target)->treasury->balance == 100 + gift
        );
        click(panel.actionBounds(DiplomaticAction::Tribute));
        PALADIN_CHECK(world.diplomacy().overlordOf(target) == actor);
        frame("pr29-diplomacy-tributary.png");
        click(panel.actionBounds(DiplomaticAction::Tribute));
        PALADIN_CHECK(!world.diplomacy().overlordOf(target));
        click(panel.actionBounds(DiplomaticAction::Trade));
        PALADIN_CHECK(world.diplomacy().between(actor, target)->trading);
        PALADIN_CHECK(!panel.actionBounds(DiplomaticAction::Trade));
        PALADIN_CHECK(!panel.actionBounds(DiplomaticAction::Peace));
        click(panel.actionBounds(DiplomaticAction::RevokeTrade));
        PALADIN_CHECK(!world.diplomacy().between(actor, target)->trading);
        click(panel.actionBounds(DiplomaticAction::Trade));
        PALADIN_CHECK(world.diplomacy().between(actor, target)->trading);
        click(panel.actionBounds(DiplomaticAction::War));
        PALADIN_CHECK(world.diplomacy().between(actor, target)->atWar);
        click(panel.actionBounds(DiplomaticAction::Peace));
        PALADIN_CHECK(!world.diplomacy().between(actor, target)->atWar);
        click(panel.sortBounds(DiplomacyPanel::Sort::Gold));
        PALADIN_CHECK(
            panel.realmBounds(actor)->y < panel.realmBounds(target)->y
        );
        click(panel.sortBounds(DiplomacyPanel::Sort::Gold));
        PALADIN_CHECK(
            panel.realmBounds(target)->y < panel.realmBounds(actor)->y
        );
        // Adjacent 64-bit balances must not collapse into a floating-point tie
        // on Windows, where long double has the same precision as double.
        world.realm(actor)->treasury->balance =
            std::numeric_limits<Money>::max() - 1;
        world.realm(target)->treasury->balance =
            std::numeric_limits<Money>::max();
        click(panel.sortBounds(DiplomacyPanel::Sort::Gold));
        PALADIN_CHECK(
            panel.realmBounds(target)->y < panel.realmBounds(actor)->y
        );
        click(panel.sortBounds(DiplomacyPanel::Sort::Gold));
        PALADIN_CHECK(
            panel.realmBounds(actor)->y < panel.realmBounds(target)->y
        );
        world.realm(actor)->treasury->balance = 99000;
        world.realm(target)->treasury->balance = 1100;
        // A captured button must not fire if released outside, or on another
        // action.
        auto b = *panel.actionBounds(DiplomaticAction::War);
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.x = b.x + 8;
        event.button.y = b.y + 8;
        PALADIN_CHECK(panel.handle(event, world, actor));
        event.type = SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.x = 2;
        event.button.y = 2;
        PALADIN_CHECK(panel.handle(event, world, actor));
        PALADIN_CHECK(!world.diplomacy().between(actor, target)->atWar);
        click(panel.realmBounds(actor));
        click(panel.actionBounds(DiplomaticAction::War));
        PALADIN_CHECK(!world.diplomacy().between(actor, actor));
        panel.layout(320, 240, world, actor);
        PALADIN_CHECK(panel.bounds().x >= 0 && panel.bounds().y >= 0);
        PALADIN_CHECK(
            panel.bounds().x + panel.bounds().width <= 320 &&
            panel.bounds().y + panel.bounds().height <= 240
        );
        panel.close();

        // Grid overflow uses rows and a draggable thumb, not a sideways strip.
        std::vector<ArmyId> armies;
        for (int i = 0; i < 25; ++i)
        {
            const auto id = world.createArmy({12, 24});
            PALADIN_CHECK(world.assignArmyToRealm(id, actor));
            armies.push_back(id);
        }
        MilitaryPanel military;
        military.toggle(city);
        military.layout(960, 640, world, actor);
        using A = MilitaryPanel::Action;
        const auto first = military.controlBounds(A::Row, armies.front());
        PALADIN_CHECK(first && first->width == 76 && first->height == 88);
        PALADIN_CHECK(
            military.controlBounds(A::Row, armies[8]) &&
            !military.controlBounds(A::Row, armies[9])
        );
        PALADIN_CHECK(military.controlBounds(A::Row, armies[3])->y > first->y);
        PALADIN_CHECK(
            military.controlBounds(A::New)->y >
            military.controlBounds(A::Row, armies[8])->y + 88
        );
        renderer.beginFrame();
        renderer.fillRectangle(0, 0, 960, 640, {32, 44, 67, 255});
        military.render(renderer, ui, world, actor, &art);
        capture(window, "pr29-military-overflow-top.png");
        renderer.endFrame();
        event = {};
        event.type = SDL_EVENT_MOUSE_WHEEL;
        event.wheel.x = 0;
        event.wheel.y = -20;
        event.wheel.mouse_x = first->x + 10;
        event.wheel.mouse_y = first->y + 10;
        PALADIN_CHECK(military.handle(event, world, actor));
        PALADIN_CHECK(
            !military.controlBounds(A::Row, armies.front()) &&
            military.controlBounds(A::Row, armies.back())
        );
        renderer.beginFrame();
        renderer.fillRectangle(0, 0, 960, 640, {32, 44, 67, 255});
        military.render(renderer, ui, world, actor, &art);
        capture(window, "pr29-military-overflow-bottom.png");
        renderer.endFrame();
        const auto mb = military.bounds();
        event = {};
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.x = first->x + (mb.width - 88) * .5F - 6;
        event.button.y = mb.y + 94 + 270;
        PALADIN_CHECK(military.handle(event, world, actor));
        event = {};
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.x = first->x + (mb.width - 88) * .5F - 6;
        event.motion.y = mb.y + 94;
        PALADIN_CHECK(military.handle(event, world, actor));
        event = {};
        event.type = SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.x = first->x + (mb.width - 88) * .5F - 6;
        event.button.y = mb.y + 94;
        PALADIN_CHECK(military.handle(event, world, actor));
        PALADIN_CHECK(
            military.controlBounds(A::Row, armies.front()) &&
            !military.selection()
        );
        // Exercise all five sorts and a list longer than the left viewport.
        for (int i = 0; i < 20; ++i)
        {
            PALADIN_CHECK(world.createRealm());
        }
        panel.open();
        panel.layout(960, 640, world, actor);
        for (auto sort :
             {DiplomacyPanel::Sort::Soldiers,
              DiplomacyPanel::Sort::Gold,
              DiplomacyPanel::Sort::Size,
              DiplomacyPanel::Sort::Cities,
              DiplomacyPanel::Sort::Fortresses})
        {
            click(panel.sortBounds(sort));
        }
        event = {};
        event.type = SDL_EVENT_MOUSE_WHEEL;
        event.wheel.y = -40;
        event.wheel.mouse_x = panel.bounds().x + 30;
        event.wheel.mouse_y = panel.bounds().y + 140;
        PALADIN_CHECK(panel.handle(event, world, actor));
        PALADIN_CHECK(panel.realmBounds(actor) && panel.realmBounds(target));
        std::cout << "Diplomacy UI: blank/selected states, ordered actions, "
                     "conserved gift, tributary toggle, five sorts, scroll and "
                     "compact army overflow passed\n";
    }

    void workYards(Renderer& renderer, SDL_Window* window)
    {
        SettlementGrid grid(60, 60);
        for (int y = 0; y < 60; ++y)
        {
            for (int x = 0; x < 60; ++x)
            {
                auto& tile = *grid.tile({x, y});
                tile.terrain = x >= 40 ? TerrainType::Water : TerrainType::Land;
                tile.biome = BiomeType::Plain;
                tile.temperature = Temperature{.5};
                tile.rainfall = Rainfall{.7};
            }
        }
        SettlementMap map(std::move(grid), {0, 0}, 1, 1, 60, 3349);
        const std::array<std::string_view, 4>
            types{"market", "stockpile", "fishing_grounds", "trade_depot"};
        const std::array<SettlementObjectFootprint, 4> footprints{
            {{{10, 10}, 6, 6},
             {{22, 10}, 6, 6},
             {{32, 10}, 8, 5},
             {{22, 23}, 8, 5}}
        };
        for (std::size_t i = 0; i < types.size(); ++i)
        {
            auto definition = *SettlementObjectCatalog::definition(types[i]);
            definition.bypassesConstruction = true;
            PALADIN_CHECK(map.objectState().placeCompletedObject(
                map.grid(),
                definition,
                footprints[i]
            ));
            map.naturalFeatures().clear(footprints[i]);
        }
        map.logistics.synchronize(map.objectState(), 360);
        for (const auto& object : map.objectState().completedObjects())
        {
            const auto inventory = map.logistics.forObject(object.id);
            PALADIN_CHECK(map.logistics.add(inventory, "lumber", 12));
            PALADIN_CHECK(map.logistics.add(inventory, "bread", 8));
        }
        CityRenderer city;
        city.animationTimeOverride = 42;
        city.presentation.cloudsEnabled = false;
        city.artRootOverride =
            std::string(PALADIN_TEST_SOURCE_ROOT) + "/assets/sprites";
        SettlementInspectionController inspection;
        SettlementCommandController commands;
        SettlementObjectPlacementController placement;
        SettlementCitizenState people;
        TileRenderMetrics metrics;
        Camera2D camera(25, 18);
        PALADIN_CHECK(people.initialize(3, 3350));
        map.employment().synchronize(map.objectState(), people);
        const auto market = map.objectState().completedObjects().front().id;
        const auto job = map.employment().forObject(market);
        PALADIN_CHECK(map.employment().adjust(job, 2, people));
        int stall = 0;
        for (const auto& record : people.citizens())
        {
            auto& person = const_cast<SettlementCitizen&>(record);
            if (!person.workplaceId)
            {
                continue;
            }
            person.tilePosition = person.destination = marketStallTile(
                footprints[0].topLeft,
                footprints[0].width,
                footprints[0].height,
                stall++
            );
            person.hasVisualSnapshot = false;
            person.insideHome = false;
            person.path.clear();
            person.activity = CitizenActivity::AtWork;
            person.task.kind = CitizenTaskKind::Work;
            person.task.object = market;
            person.task.workTile = person.tilePosition;
        }
        const auto fishery = map.objectState().completedObjects()[2].id;
        const auto fishJob = map.employment().forObject(fishery);
        PALADIN_CHECK(map.employment().adjust(fishJob, 1, people));
        for (const auto& record : people.citizens())
        {
            if (record.workplaceId != fishJob)
            {
                continue;
            }
            auto& fisher = const_cast<SettlementCitizen&>(record);
            fisher.tilePosition = fisher.destination = {43, 12};
            fisher.hasVisualSnapshot = false;
            fisher.inFishingBoat = true;
            fisher.insideHome = false;
            fisher.activity = CitizenActivity::Fishing;
            fisher.task.kind = CitizenTaskKind::Work;
            fisher.task.object = fishery;
            fisher.task.workTile = fisher.tilePosition;
            fisher.task.target = {43, 13};
        }
        LocalTradeVisit visit;
        visit.shipment = ShipmentId{1};
        visit.resource = "lumber";
        visit.quantity = 10;
        visit.startMinute = 350;
        visit.path = {{32, 27}, {31, 27}, {30, 27}, {29, 27}, {28, 27}};
        map.trade.visits.push_back(visit);
        city.gameMinute = 360;
        const auto draw = [&](const std::string& name, double hour)
        {
            renderer.beginFrame();
            city.render(
                renderer,
                map,
                camera,
                metrics,
                placement,
                commands,
                people,
                inspection,
                1,
                hour
            );
            const auto result = name.empty() ? std::vector<std::uint32_t>{}
                                             : capture(window, name);
            renderer.endFrame();
            return result;
        };
        camera.setZoom(4);
        for (int warm = 0; warm < 12; ++warm)
        {
            draw("", 12);
        }
        draw("next-workyards-normal-day.png", 12);
        draw("next-workyards-normal-night.png", 0);
        camera.setZoom(16);
        camera.setPosition(43.5, 12.5);
        draw("next-fishing-boat-close-day.png", 12);
        draw("next-fishing-boat-close-night.png", 0);
        for (std::size_t i = 0; i < types.size(); ++i)
        {
            const auto& f = footprints[i];
            camera.setPosition(
                f.topLeft.x + f.width * .5,
                f.topLeft.y + f.height * .5
            );
            for (int warm = 0; warm < 4; ++warm)
            {
                draw("", 12);
            }
            requireBlocks(
                draw("next-" + std::string(types[i]) + "-close-day.png", 12),
                960,
                640,
                4,
                true
            );
            draw("next-" + std::string(types[i]) + "-close-night.png", 0);
            if (workplaceCompound(types[i]))
            {
                city.presentation.roofsVisible = false;
                draw("next-" + std::string(types[i]) + "-interior.png", 12);
                city.presentation.roofsVisible = true;
            }
        }
        // The actual placement controller draws the same four counters as the
        // completed market, including while dragging and after release.
        PALADIN_CHECK(map.objectState().placeCompletedObject(
            map.grid(),
            *SettlementObjectCatalog::definition("city_keep"),
            {{2, 42}, 5, 7}
        ));
        map.logistics.synchronize(map.objectState(), 360);
        map.naturalFeatures().clear({{10, 23}, 6, 6});
        PALADIN_CHECK(placement.beginPlacement("market"));
        camera.setPosition(13, 26);
        placement.pointerMoved(SettlementTilePosition{10, 23});
        PALADIN_CHECK(!placement.hasDrawablePreview());
        PALADIN_CHECK(
            placement.pointerPressed(SettlementTilePosition{10, 23}, map) ==
            SettlementPlacementCommitResult::None
        );
        placement.pointerMoved(SettlementTilePosition{15, 28});
        PALADIN_CHECK(placement.hasDrawablePreview());
        draw("next-market-drag-preview.png", 12);
        PALADIN_CHECK(
            placement.pointerReleased(SettlementTilePosition{15, 28}, map)
        );
        PALADIN_CHECK(placement.visibleFootprintIsValid(map));
        draw("next-market-locked-preview.png", 12);
        placement.cancelPlacement();
    }

    void generatedRelief(Renderer& renderer, SDL_Window* window)
    {
        WorldGrid source(16, 16);
        for (int y = 0; y < 16; ++y)
        {
            for (int x = 0; x < 16; ++x)
            {
                auto& tile = *source.tile({x, y});
                tile.terrain =
                    x < 8 ? TerrainType::Mountain : TerrainType::Land;
                tile.biome = x < 8 ? BiomeType::Plain : BiomeType::Hills;
                tile.relief = x < 8 ? ReliefType::Mountain : ReliefType::Hills;
                tile.elevation = Elevation{.7};
                tile.temperature = Temperature{.5};
                tile.rainfall = Rainfall{.5};
                tile.mineral = MineralDeposit::Iron;
            }
        }
        SettlementMapGenerationSettings settings;
        settings.localTilesPerWorldTile = 32;
        auto map = SettlementMapGenerator{}
                       .generate(source, {7, 7}, 8, 8, 48391, settings);
        CityRenderer city;
        city.animationTimeOverride = 42;
        city.presentation.cloudsEnabled = false;
        city.artRootOverride =
            std::string(PALADIN_TEST_SOURCE_ROOT) + "/assets/sprites";
        SettlementCitizenState people;
        SettlementInspectionController inspection;
        SettlementCommandController commands;
        SettlementObjectPlacementController placement;
        TileRenderMetrics metrics;
        Camera2D camera(128, 128);
        const auto draw = [&](const char* name, double zoom, double hour)
        {
            camera.setZoom(zoom);
            for (int warm = 0; warm < 120; ++warm)
            {
                renderer.beginFrame();
                city.render(
                    renderer,
                    *map,
                    camera,
                    metrics,
                    placement,
                    commands,
                    people,
                    inspection,
                    1,
                    hour
                );
                if (warm == 119)
                {
                    capture(window, name);
                }
                renderer.endFrame();
            }
        };
        draw("terrain-pass-ranges.png", .75, 12);
        bool foundCave = false;
        for (int y = 0; y < map->grid().height() && !foundCave; ++y)
        {
            for (int x = 0; x < map->grid().width(); ++x)
            {
                if (map->grid().tile({x, y})->rockFloor)
                {
                    camera.setPosition(x, y);
                    foundCave = true;
                    break;
                }
            }
        }
        PALADIN_CHECK(foundCave);
        draw("terrain-pass-cave-day.png", 4, 12);
        draw("terrain-pass-cave-night.png", 4, 0);
        camera.setPosition(205, 128);
        draw("terrain-pass-hills.png", 4, 12);
    }

    void miningAndFlags(Renderer& renderer, SDL_Window* window)
    {
        SettlementGrid grid(40, 40);
        for (int y = 0; y < 40; ++y)
        {
            for (int x = 0; x < 40; ++x)
            {
                auto& tile = *grid.tile({x, y});
                tile.terrain = TerrainType::Land;
                tile.biome = BiomeType::Plain;
                tile.temperature = Temperature{.5};
                tile.rainfall = Rainfall{.7};
            }
        }
        SettlementMap map(std::move(grid), {0, 0}, 1, 1, 40, 3347);
        std::vector<SettlementObjectId> sites;
        for (int i = 0; i < 4; ++i)
        {
            const auto& job = MiningJobs[i];
            const SettlementObjectFootprint footprint{
                {8 + (i % 2) * 15, 8 + (i / 2) * 15},
                7,
                7
            };
            for (int y = 0; y < 7; ++y)
            {
                for (int x = 0; x < 7; ++x)
                {
                    map.grid()
                        .tile(
                            {footprint.topLeft.x + x, footprint.topLeft.y + y}
                        )
                        ->mineral = job.deposit;
                }
            }
            map.objectState().invalidateTerrainCache();
            auto definition = *SettlementObjectCatalog::definition(job.type);
            definition.bypassesConstruction = true;
            PALADIN_CHECK(map.objectState().placeCompletedObject(
                map.grid(),
                definition,
                footprint
            ));
            sites.push_back(map.objectState().completedObjects().back().id);
            map.naturalFeatures().clear(footprint);
        }
        map.logistics.synchronize(map.objectState(), 360);
        SettlementCitizenState people;
        PALADIN_CHECK(people.initialize(4, 3348));
        map.employment().synchronize(map.objectState(), people);
        for (int i = 0; i < 4; ++i)
        {
            PALADIN_CHECK(map.employment().adjust(
                map.employment().forObject(sites[i]),
                1,
                people
            ));
        }
        for (const auto& record : people.citizens())
        {
            auto& person = const_cast<SettlementCitizen&>(record);
            const auto* job = map.employment().workplace(person.workplaceId);
            PALADIN_CHECK(job);
            const auto& f =
                map.objectState().completedObject(job->objectId)->footprint;
            person.tilePosition = {f.topLeft.x + 3, f.topLeft.y + 3};
            person.hasVisualSnapshot = false;
            person.insideHome = false;
            person.activity = CitizenActivity::Mining;
            person.task.kind = CitizenTaskKind::Work;
            person.task.object = job->objectId;
            person.path.clear();
        }
        CityRenderer city;
        city.animationTimeOverride = 42;
        city.presentation.cloudsEnabled = false;
        city.artRootOverride =
            std::string(PALADIN_TEST_SOURCE_ROOT) + "/assets/sprites";
        SettlementInspectionController inspection;
        SettlementCommandController commands;
        SettlementObjectPlacementController placement;
        TileRenderMetrics metrics;
        Camera2D camera(19, 19);
        camera.setZoom(4);
        const auto draw = [&](const std::string& name, double hour)
        {
            renderer.beginFrame();
            city.render(
                renderer,
                map,
                camera,
                metrics,
                placement,
                commands,
                people,
                inspection,
                1,
                hour
            );
            const auto result = capture(window, name);
            renderer.endFrame();
            return result;
        };
        draw("next-mines-untouched.png", 12);
        for (int stage = 1; stage <= 4; ++stage)
        {
            for (const auto id : sites)
            {
                map.mining.work(
                    map.grid(),
                    *map.objectState().completedObject(id),
                    1,
                    49 * 720 / 4,
                    100000
                );
            }
            draw("next-mines-progress-" + std::to_string(stage) + ".png", 12);
        }
        draw("next-mines-normal-night.png", 0);
        camera.setPosition(11.5, 11.5);
        camera.setZoom(16);
        for (int warm = 0; warm < 3; ++warm)
        {
            draw("next-miner-warming.png", 12);
        }
        const auto still = draw("next-miner-close-day.png", 12);
        requireBlocks(still, 960, 640, 4, true);
        const auto actorPixels = [](const auto& frame)
        {
            // Terrain pages may finish streaming while paused. Isolate the
            // miner and its excavated floor, so neither streaming nor ambient
            // grass can falsely pass the animation or pause assertion.
            std::vector<std::uint32_t> actor;
            for (int y = 230; y < 350; ++y)
            {
                actor.insert(
                    actor.end(),
                    frame.begin() + y * 960 + 420,
                    frame.begin() + y * 960 + 540
                );
            }
            return actor;
        };
        PALADIN_CHECK(
            actorPixels(still) == actorPixels(draw("next-miner-paused.png", 12))
        );
        for (const auto& record : people.citizens())
        {
            const_cast<SettlementCitizen&>(record).workAnimationMinutes = 4;
        }
        PALADIN_CHECK(
            actorPixels(still) != actorPixels(draw("next-miner-swing.png", 12))
        );
        draw("next-miner-close-night.png", 0);
        camera.setPosition(26.5, 26.5);
        draw("next-quarry-close-day.png", 12);
        draw("next-quarry-close-night.png", 0);
        renderer.beginFrame();
        renderer.fillRectangle(0, 0, 960, 640, {57, 70, 88, 255});
        std::set<std::vector<std::uint32_t>> designs;
        for (std::size_t i = 0; i < RealmFlagDesignCount; ++i)
        {
            const auto flag = realmFlagDesign(i);
            std::vector<std::uint32_t> key;
            for (const auto& cell : flag.cells)
            {
                key.push_back(
                    (cell.color.red << 16) | (cell.color.green << 8) |
                    cell.color.blue
                );
            }
            PALADIN_CHECK(designs.insert(key).second);
            if (i < 96)
            {
                drawRealmFlag(
                    renderer,
                    flag,
                    20 + float(i % 16) * 58,
                    16 + float(i / 16) * 100,
                    6
                );
            }
        }
        capture(window, "next-flag-designs.png");
        renderer.endFrame();
        std::cout << "Mine render: continuous excavation, native day/night "
                     "pixels, miner swing, pause and unique flags passed\n";
    }

    void uprightSettlements(
        Renderer& renderer,
        SDL_Window* window,
        SceneSpriteLibrary& art
    )
    {
        WorldGenerationSettings settings;
        settings.width = settings.height = 64;
        settings.seed = 711;
        settings.populateAiRealms = false;
        Simulation sim(settings);
        auto& world = sim.world();
        for (int y = 0; y < world.grid().height(); ++y)
        {
            for (int x = 0; x < world.grid().width(); ++x)
            {
                world.grid().tile({x, y})->terrain = TerrainType::Land;
                world.grid().tile({x, y})->biome = BiomeType::Plain;
            }
        }
        const auto city = sim.foundPlayerCapital(
            {32, 32},
            {"Roll Realm", "Roll Folk", "Orientation", {}, "civic", {}}
        );
        PALADIN_CHECK(city);
        WorldObjectRenderer objects;
        constexpr double pi = 3.14159265358979323846;
        for (const WorldTilePosition location :
             {WorldTilePosition{32, 32}, {1, 12}, {62, 49}, {30, 1}, {30, 62}})
        {
            PALADIN_CHECK(world.setSettlementPosition(city, location));
            for (const double pixels : {16., 39.95, 40., 64., 127.5, 256.})
            {
                std::vector<std::uint32_t> reference;
                for (const double roll : {0., .37, pi * .5, pi, -pi + .00001})
                {
                    Camera2D camera(location.x + .5, location.y + .5);
                    camera.setWorldZoom(pixels * 64 / (640 * .40 * 2 * pi));
                    camera.setPlanetRotation(
                        GlobeView::orientationAt(
                            {(location.x + .5) / 64., (location.y + .5) / 64.},
                            roll
                        ),
                        64,
                        64
                    );
                    renderer.beginFrame();
                    renderer.fillRectangle(0, 0, 960, 640, {35, 87, 71, 255});
                    objects.render(
                        renderer,
                        world,
                        camera,
                        pixels,
                        true,
                        worldPresentationState(pixels),
                        true,
                        {},
                        std::nullopt,
                        {},
                        &art
                    );
                    const auto image = capture(
                        window,
                        "pr27-upright-" + std::to_string(location.x) + "-" +
                            std::to_string(location.y) + "-" +
                            std::to_string(int(pixels)) + "-" +
                            std::to_string(roll) + ".png"
                    );
                    renderer.endFrame();
                    if (reference.empty())
                    {
                        reference = image;
                    }
                    else
                    {
                        PALADIN_CHECK(reference == image);
                    }
                }
            }
        }
        // The former .999 local-surface switch must not rotate sprite art.
        Camera2D camera(30.5, 62.5);
        camera.setWorldZoom(64 * 64 / (640 * .40 * 2 * pi));
        camera.setPlanetRotation(
            GlobeView::orientationAt({30.5 / 64, 62.5 / 64}, pi),
            64,
            64
        );
        std::vector<std::uint32_t> reference;
        for (const float local : {.9989F, .9991F, 1.F})
        {
            auto p = worldPresentationState(64);
            p.localWorldWeight = local;
            p.regionalWeight = 1 - local;
            renderer.beginFrame();
            renderer.fillRectangle(0, 0, 960, 640, {35, 87, 71, 255});
            objects.render(
                renderer,
                world,
                camera,
                64,
                true,
                p,
                true,
                {},
                std::nullopt,
                {},
                &art
            );
            const auto image = capture(
                window,
                "pr27-billboard-transition-" + std::to_string(local) + ".png"
            );
            renderer.endFrame();
            if (reference.empty())
            {
                reference = image;
            }
            else
            {
                PALADIN_CHECK(reference == image);
            }
        }
        std::cout << "Upright world art: five geographic regions, six zooms, "
                     "five rolls and former flip threshold passed.\n";
    }

} // namespace
int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    int result = 0;
    try
    {
        Window window("Paladin military and animation verification", 960, 640);
        PALADIN_CHECK(window.isValid());
        SDL_HideWindow(window.nativeHandle());
        Renderer renderer(window.nativeHandle());
        PALADIN_CHECK(renderer.isValid());
        SceneSpriteLibrary art;
        art.load(
            renderer,
            std::string(PALADIN_TEST_SOURCE_ROOT) + "/assets/sprites"
        );
        PALADIN_CHECK(art.find("citizen.militia.male.front.walk"));
        actors(renderer, window.nativeHandle(), art);
        citizenPicking(renderer, window.nativeHandle());
        militaryAndCities(renderer, window.nativeHandle(), art);
        selectionAndConstruction(renderer, window.nativeHandle());
        diplomacyAndOverflow(renderer, window.nativeHandle(), art);
        uprightSettlements(renderer, window.nativeHandle(), art);
        generatedRelief(renderer, window.nativeHandle());
        miningAndFlags(renderer, window.nativeHandle());
        workYards(renderer, window.nativeHandle());
        std::cout << "Military UI, universal markers, walking/gathering, sleep "
                     "pixel blocks and pause checks passed.\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    SDL_Quit();
    return result;
}
