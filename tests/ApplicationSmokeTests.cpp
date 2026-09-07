#include "EnvironmentTerrainSmokeChecks.h"
#include "TestFramework.h"
#include "core/Application.h"
#include "core/SimulationClock.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "interaction/SettlementPlacementController.h"
#include "platform/Window.h"
#include "rendering/BuildingView.h"
#include "rendering/Camera2D.h"
#include "rendering/CityRenderer.h"
#include "rendering/GrassPresentation.h"
#include "rendering/HomePresentation.h"
#include "rendering/PasturePresentation.h"
#include "rendering/Renderer.h"
#include "rendering/SettlementCitizenRenderer.h"
#include "rendering/SettlementEnvironmentDetails.h"
#include "rendering/SpriteStyle.h"
#include "rendering/StockpilePresentation.h"
#include "rendering/TileRenderMetrics.h"
#include "simulation/Simulation.h"
#include "ui/CityHud.h"
#include "ui/DebugConsole.h"
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
#include "ui/LedgerPanel.h"
#include "ui/SettlementInspectionPanel.h"
#include "ui/SimulationSpeedControls.h"
#include "world/World.h"
#include "world/generation/WorldGenerationSettings.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace Paladin
{
    struct SettlementActivityTestFixture
    {
        static void setLogger(
            SettlementCitizenState& state,
            SettlementObjectId workplace
        )
        {
            setRenderPerson(state, {13, 14});
            auto& c = state.citizens_[0];
            c.task.kind = CitizenTaskKind::Work;
            c.task.object = workplace;
            c.task.workTile = c.tilePosition;
            c.activity = CitizenActivity::AtWork;
        }
        static void setRenderPerson(
            SettlementCitizenState& state,
            SettlementTilePosition tile,
            SettlementObjectId home = {}
        )
        {
            state.citizens_.resize(1);
            auto& c = state.citizens_[0];
            c.id = CitizenId{1};
            c.tilePosition = c.destination = tile;
            c.path.clear();
            c.hasVisualSnapshot = false;
            c.child = false;
            c.homeId = home;
            c.insideHome = bool(home);
        }
        static void marryFirstTwo(SettlementCitizenState& state)
        {
            state.citizens_[0].spouseId = state.citizens_[1].id;
            state.citizens_[1].spouseId = state.citizens_[0].id;
            state.citizens_[1].tilePosition =
                state.citizens_[1].destination = {15, 10};
        }
        static void setNeeds(
            SettlementCitizenState& people,
            double hunger,
            double health
        )
        {
            for (auto& c : people.citizens_)
            {
                c.hunger = hunger;
                c.health = health;
            }
        }
    };
    // One integration scenario exercises real application routing, not copies
    // of the dispatch logic. Enabled explicitly via CMake for UI refactors.
    struct ApplicationSmokeTest
    {
        static void requireUniformWorldPixels(Application& app, int pitch)
        {
            auto* raw = SDL_RenderReadPixels(
                SDL_GetRenderer(app.window_->nativeHandle()),
                nullptr
            );
            PALADIN_CHECK(raw);
            auto* rgba = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(raw);
            PALADIN_CHECK(rgba);
            std::unordered_set<std::uint32_t> colors;
            for (int y = 0; y < rgba->h; ++y)
            {
                for (int x = 0; x < rgba->w; ++x)
                {
                    const auto* row = reinterpret_cast<const std::uint32_t*>(
                        static_cast<const Uint8*>(rgba->pixels) +
                        y * rgba->pitch
                    );
                    const auto* anchor = reinterpret_cast<const std::uint32_t*>(
                        static_cast<const Uint8*>(rgba->pixels) +
                        (y / pitch * pitch) * rgba->pitch
                    );
                    PALADIN_CHECK(row[x] == anchor[x / pitch * pitch]);
                    colors.insert(row[x]);
                }
            }
            PALADIN_CHECK(colors.size() > 8);
            SDL_DestroySurface(rgba);
        }
        static void capture(Application& app, const char* name)
        {
            const char* directory = SDL_getenv("PALADIN_SMOKE_SCREENSHOTS");
            if (!directory)
            {
                return;
            }
            auto* surface = SDL_RenderReadPixels(
                SDL_GetRenderer(app.window_->nativeHandle()),
                nullptr
            );
            PALADIN_CHECK(surface);
            const auto path = std::string(directory) + "/" + name;
            PALADIN_CHECK(SDL_SaveBMP(surface, path.c_str()));
            SDL_DestroySurface(surface);
        }
        static bool send(Application& app, SDL_Event event)
        {
            return app.handleEvent(event, app.simulationControlsVisible());
        }

        static bool click(Application& app, float x, float y)
        {
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.button = SDL_BUTTON_LEFT;
            event.button.x = x;
            event.button.y = y;
            const bool controlsVisible = app.simulationControlsVisible();
            PALADIN_CHECK(app.handleEvent(event, controlsVisible));
            event.type = SDL_EVENT_MOUSE_BUTTON_UP;
            return app.handleEvent(event, controlsVisible);
        }

        static void key(Application& app, SDL_Scancode code)
        {
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = code;
            PALADIN_CHECK(send(app, event));
        }

        static void frame(Application& app)
        {
            app.simulationClock_->beginFrame();
            app.layoutFrame();
            app.updateFrame();
            app.renderFrame();
        }

        static void presentationChecks(Application& app)
        {
            // Feature summaries must follow resource changes, including
            // irregular map edges, rather than retain stale distant trees.
            SettlementNaturalFeatures summary(7, 5);
            summary.set({6, 4}, NaturalFeatureKind::Tree);
            summary.set({5, 4}, NaturalFeatureKind::Rock);
            PALADIN_CHECK(summary.overview({6, 4})[0] == 1);
            PALADIN_CHECK(summary.overview({6, 4})[1] == 1);
            summary.harvest({6, 4}, 0);
            PALADIN_CHECK(summary.overview({6, 4})[0] == 0);
            summary.clear({{4, 4}, 3, 1});
            PALADIN_CHECK(summary.overview({6, 4})[1] == 0);
            // Each animation frame stays separate and uses only source colors.
            auto* art = SDL_CreateSurface(64, 32, SDL_PIXELFORMAT_RGBA32);
            PALADIN_CHECK(art);
            SDL_FillSurfaceRect(
                art,
                nullptr,
                SDL_MapSurfaceRGBA(art, 73, 151, 91, 255)
            );
            SDL_Rect second{32, 0, 32, 32};
            SDL_FillSurfaceRect(
                art,
                &second,
                SDL_MapSurfaceRGBA(art, 99, 62, 75, 255)
            );
            auto* simple = simplifySprite(art, 1, 1, 2);
            PALADIN_CHECK(simple && simple->w == 32 && simple->h == 16);
            Uint8 r, g, b, a;
            SDL_ReadSurfacePixel(simple, 15, 8, &r, &g, &b, &a);
            PALADIN_CHECK(r == 73 && g == 151 && b == 91 && a == 255);
            SDL_ReadSurfacePixel(simple, 16, 8, &r, &g, &b, &a);
            PALADIN_CHECK(r == 99 && g == 62 && b == 75 && a == 255);
            SDL_DestroySurface(simple);
            SDL_DestroySurface(art);
            const SettlementObjectFootprint facingFootprint{{10, 10}, 3, 3};
            PALADIN_CHECK(
                buildingView(facingFootprint, SettlementTilePosition{11, 10}) ==
                0
            );
            PALADIN_CHECK(
                buildingView(facingFootprint, SettlementTilePosition{12, 11}) ==
                1
            );
            PALADIN_CHECK(
                buildingView(facingFootprint, SettlementTilePosition{11, 12}) ==
                2
            );
            PALADIN_CHECK(
                buildingView(facingFootprint, SettlementTilePosition{10, 11}) ==
                3
            );
            PALADIN_CHECK(
                buildingView(
                    facingFootprint,
                    SettlementTilePosition{11, 12},
                    90
                ) == 1
            );
            SimulationClock visualClock(20);
            visualClock.beginFrame();
            SDL_Delay(30);
            visualClock.beginFrame();
            PALADIN_CHECK(visualClock.presentationSeconds() == 0);
            visualClock.setPaused(false);
            SDL_Delay(30);
            visualClock.beginFrame();
            PALADIN_CHECK(visualClock.presentationSeconds() > 0);
            visualClock.setPaused(true);
            const auto frozenTime = visualClock.presentationSeconds();
            SDL_Delay(30);
            visualClock.beginFrame();
            PALADIN_CHECK(visualClock.presentationSeconds() == frozenTime);
            auto& renderer = *app.renderer_;
            environmentTerrainSmokeChecks(
                renderer,
                SDL_GetRenderer(app.window_->nativeHandle())
            );
            SettlementGrid grid(32, 32);
            for (int y = 0; y < 32; ++y)
            {
                for (int x = 0; x < 32; ++x)
                {
                    grid.tile({x, y})->terrain = TerrainType::Land;
                    grid.tile({x, y})->biome = BiomeType::Plain;
                }
            }
            SettlementMap map(std::move(grid), {0, 0}, 1, 1, 32, 701);
            const auto* house = SettlementObjectCatalog::definition(
                SettlementObjectTypes::House
            );
            auto fixtureHouse = *house;
            fixtureHouse.bypassesConstruction = true; // Rendering fixture only.
            PALADIN_CHECK(map.objectState().placeCompletedObject(
                map.grid(),
                fixtureHouse,
                {{12, 12}, 3, 3},
                SettlementTilePosition{13, 14}
            ));
            map.naturalFeatures().set({10, 12}, NaturalFeatureKind::Tree);
            map.naturalFeatures().set({16, 13}, NaturalFeatureKind::Tree);
            SettlementCitizenState people;
            SettlementActivityTestFixture::setRenderPerson(people, {13, 13});
            Camera2D camera(13.5, 13.5);
            camera.setZoom(16);
            TileRenderMetrics metrics;
            CityRenderer city;
            const auto fixtureRoot = std::filesystem::path(SDL_GetBasePath()) /
                                     "presentation-fixture";
            std::filesystem::create_directories(fixtureRoot);
            std::filesystem::copy_file(
                std::filesystem::path(SDL_GetBasePath()) /
                    "assets/sprites/objects.catalog",
                fixtureRoot / "objects.catalog",
                std::filesystem::copy_options::overwrite_existing
            );
            std::filesystem::copy_file(
                std::filesystem::path(SDL_GetBasePath()) /
                    "assets/sprites/lights.catalog",
                fixtureRoot / "lights.catalog",
                std::filesystem::copy_options::overwrite_existing
            );
            city.artRootOverride = fixtureRoot.string();
            SettlementObjectPlacementController placement;
            SettlementCommandController commands;
            SettlementInspectionController inspection;
            const auto version = map.objectState().presentationVersion();
            auto pixel = [&](int x, int y)
            {
                auto* surface = SDL_RenderReadPixels(
                    SDL_GetRenderer(app.window_->nativeHandle()),
                    nullptr
                );
                PALADIN_CHECK(surface);
                Uint8 red, green, blue, alpha;
                PALADIN_CHECK(SDL_ReadSurfacePixel(
                    surface,
                    x,
                    y,
                    &red,
                    &green,
                    &blue,
                    &alpha
                ));
                SDL_DestroySurface(surface);
                return RenderColor{red, green, blue, alpha};
            };
            // Cached translucent foundation patches must match direct
            // compositing, without applying their opacity a second time.
            const std::array<RenderColor, 1> redPixel{{{240, 20, 10, 255}}};
            auto redTexture = renderer.createTextureFromPixels(1, 1, redPixel);
            PALADIN_CHECK(redTexture);
            const std::array<TextureDrawItem, 1> translucent{
                {{redTexture.get(), {0, 0, 1, 1}, {0, 0, 1, 1}, {}, 128}}
            };
            auto cachedPatch =
                renderer.createTextureFromDrawItems(1, 1, translucent, true);
            PALADIN_CHECK(cachedPatch);
            renderer.beginFrame();
            renderer.fillRectangle(0, 0, 40, 20, {20, 70, 210, 255});
            renderer.drawTexture(*redTexture, 0, 0, 1, 1, 0, 0, 20, 20, 128);
            renderer.drawTexture(*cachedPatch, 0, 0, 1, 1, 20, 0, 20, 20);
            const auto directPatch = pixel(10, 10),
                       composedPatch = pixel(30, 10);
            PALADIN_CHECK(
                std::abs(int(directPatch.red) - composedPatch.red) <= 1
            );
            PALADIN_CHECK(
                std::abs(int(directPatch.green) - composedPatch.green) <= 1
            );
            PALADIN_CHECK(
                std::abs(int(directPatch.blue) - composedPatch.blue) <= 1
            );
            auto draw = [&](double hour)
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
            };
            draw(12);
            capture(app, "presentation-roofs.bmp");
            const auto covered =
                pixel(renderer.outputWidth() / 2, renderer.outputHeight() / 2);
            PALADIN_CHECK(covered.red == house->visual.fillColor[0]);
            city.presentation.roofsVisible = false;
            draw(12);
            capture(app, "presentation-interior.bmp");
            const auto uncovered =
                pixel(renderer.outputWidth() / 2, renderer.outputHeight() / 2);
            PALADIN_CHECK(uncovered.red == 210 && uncovered.green == 180);
            // Real indoor state, including front/corner tiles previously
            // covered by the full-height south wall.
            for (int y = 12; y < 15; ++y)
            {
                for (int x = 12; x < 15; ++x)
                {
                    SettlementActivityTestFixture::setRenderPerson(
                        people,
                        {x, y},
                        map.objectState().completedObjects().front().id
                    );
                    draw(12);
                    const auto occupant = pixel(
                        int(renderer.outputWidth() * .5 +
                            (x + .5 - camera.tileX()) * 64),
                        int(renderer.outputHeight() * .5 +
                            (y + .5 - camera.tileY()) * 64)
                    );
                    if (occupant.red != 210 || occupant.green != 180)
                    {
                        std::cerr << "Hidden interior tile " << x << "," << y
                                  << " pixel " << int(occupant.red) << ","
                                  << int(occupant.green) << "\n";
                        capture(app, "presentation-interior-failure.bmp");
                    }
                    PALADIN_CHECK(occupant.red == 210 && occupant.green == 180);
                }
            }
            capture(app, "presentation-interior-cutaway.bmp");
            SettlementActivityTestFixture::setRenderPerson(people, {13, 13});
            PALADIN_CHECK(map.objectState().presentationVersion() == version);
            PALADIN_CHECK(
                people.citizens()[0].tilePosition ==
                SettlementTilePosition(13, 13)
            );
            // The same citizen crosses an existing canopy using interpolated
            // positions. Rendered pixels prove cross-category ordering.
            SettlementActivityTestFixture::setRenderPerson(people, {10, 11});
            draw(12);
            const int treeX =
                int(renderer.outputWidth() * .5 + (10.5 - camera.tileX()) * 64);
            const int behindY =
                int(renderer.outputHeight() * .5 +
                    (11.5 - camera.tileY()) * 64);
            const auto behind = pixel(treeX, behindY);
            PALADIN_CHECK(!(behind.red == 210 && behind.green == 180));
            SettlementActivityTestFixture::setRenderPerson(people, {10, 12});
            draw(12);
            const int frontY = behindY + 64;
            const auto front = pixel(treeX, frontY);
            PALADIN_CHECK(front.red == 210 && front.green == 180);
            capture(app, "presentation-overlap.bmp");
            city.presentation.roofsVisible = true;
            draw(0);
            capture(app, "presentation-night.bmp");
            camera.setZoom(.25);
            draw(12); // Cached overview path; no detailed full-map queue.
            capture(app, "presentation-far.bmp");

            // Large modular footprints must not expand into per-tile work.
            SettlementGrid largeGrid(512, 512);
            for (int y = 0; y < 512; ++y)
            {
                for (int x = 0; x < 512; ++x)
                {
                    largeGrid.tile({x, y})->terrain = TerrainType::Land;
                }
            }
            SettlementMap large(std::move(largeGrid), {0, 0}, 1, 1, 512, 702);
            auto largeDefinition = fixtureHouse;
            largeDefinition.selectionMode =
                SettlementFootprintSelectionMode::DragRectangle;
            PALADIN_CHECK(large.objectState().placeCompletedObject(
                large.grid(),
                largeDefinition,
                {{1, 1}, 500, 500}
            ));
            SceneDrawQueue bounded;
            SceneSpriteLibrary placeholders;
            placeholders.load(renderer, fixtureRoot.string());
            for (const auto& definition :
                 SettlementObjectCatalog::definitions())
            {
                PALADIN_CHECK(!placeholders
                                   .objectStyle(std::string(definition.id))
                                   .floor.empty());
            }
            SettlementStructurePresentation structure;
            const SceneProjection close{
                250,
                250,
                64,
                renderer.outputWidth(),
                renderer.outputHeight()
            };
            structure
                .submit(bounded, close, large, city.presentation, placeholders);
            PALADIN_CHECK(bounded.size() < 100);
            // Pan with stable ground anchors; visual bounds include overhang.
            SceneProjection p1{0, 0, 10, 100, 100}, p2{1, 2, 10, 100, 100};
            const SceneVisual tall{2, 3, .5, 2, 4, .5, 1};
            const auto b1 = p1.bounds(tall), b2 = p2.bounds(tall);
            PALADIN_CHECK(b2.x == b1.x - 10 && b2.y == b1.y - 20);
            PALADIN_CHECK(p1.visible(p1.bounds({5, 10, 0, 2, 8, .5, 1})));

            // Actual PNG decode, alpha, optional catalog and fixed anchors.
            const auto directory = std::filesystem::path(SDL_GetBasePath()) /
                                   "renderer-test-fixture";
            std::filesystem::create_directories(directory);
            const RenderColor pixels[2] = {{255, 0, 0, 255}, {0, 0, 0, 0}};
            auto* image = SDL_CreateSurfaceFrom(
                2,
                1,
                SDL_PIXELFORMAT_RGBA32,
                const_cast<RenderColor*>(pixels),
                8
            );
            PALADIN_CHECK(image);
            PALADIN_CHECK(
                IMG_SavePNG(image, (directory / "alpha.png").string().c_str())
            );
            SDL_DestroySurface(image);
            std::ofstream(directory / "art-palette.hex") << "#FF0000\n";
            std::ofstream(directory / "sprites.catalog")
                << "citizen alpha.png 2 1 0.5 1 0 0\ntree alpha.png 2 1 0.5 1 "
                   "0 0\ninvalid missing.png 1 1 "
                   ".5 1 0 0\n";
            std::ofstream(directory / "objects.catalog")
                << "house single - - - citizen 3 3 1 .12 0 0 0 0 0 dbd6c2 "
                   "524d3d .6 .45\n"
                   "invalid modules - - - - 0 1 1 .1 0 0 0 0 0 ffffff 555555 "
                   ".6 .45\n";
            SceneSpriteLibrary library;
            library.load(renderer, directory.string());
            PALADIN_CHECK(library.find("citizen"));
            const auto* originalTree = library.find("tree");
            PALADIN_CHECK(originalTree);
            SceneSpriteLibrary::setEnvironmentArtEnabled(false);
            PALADIN_CHECK(!library.find("tree"));
            PALADIN_CHECK(library.find("citizen"));
            SceneSpriteLibrary::setEnvironmentArtEnabled(true);
            PALADIN_CHECK(library.find("tree") == originalTree);
            PALADIN_CHECK(!library.find("invalid"));
            PALADIN_CHECK(library.objectHasArt("house"));
            PALADIN_CHECK(library.objectStyle("invalid").mode == "ground");
            SceneDrawQueue replacement;
            const SceneProjection replacementView{
                13.5,
                13.5,
                64,
                renderer.outputWidth(),
                renderer.outputHeight()
            };
            structure.submit(
                replacement,
                replacementView,
                map,
                city.presentation,
                library
            );
            PALADIN_CHECK(replacement.size() == 1);
            PALADIN_CHECK(replacement.items().front().texture != nullptr);
            PALADIN_CHECK(!renderer.loadImageTexture(
                (directory / "missing.png").string().c_str()
            ));
            SceneDrawQueue queue;
            SceneProjection projection{
                0,
                0,
                10,
                renderer.outputWidth(),
                renderer.outputHeight()
            };
            PALADIN_CHECK(
                library.submit(queue, projection, "citizen", 0, 0, 1)
            );
            renderer.beginFrame();
            queue.render(renderer);
            const auto red = pixel(
                renderer.outputWidth() / 2 - 5,
                renderer.outputHeight() / 2 - 5
            );
            const auto transparent = pixel(
                renderer.outputWidth() / 2 + 5,
                renderer.outputHeight() / 2 - 5
            );
            PALADIN_CHECK(red.red == 255 && red.green == 0);
            PALADIN_CHECK(transparent.red == 18 && transparent.green == 20);
            renderer.endFrame();
            SettlementGrid galleryGrid(48, 48);
            for (int y = 0; y < 48; ++y)
            {
                for (int x = 0; x < 48; ++x)
                {
                    galleryGrid.tile({x, y})->terrain = TerrainType::Land;
                    galleryGrid.tile({x, y})->biome = BiomeType::Plain;
                }
            }
            SettlementMap
                gallery(std::move(galleryGrid), {0, 0}, 1, 1, 48, 703);
            int slot = 0;
            for (auto definition : SettlementObjectCatalog::definitions())
            {
                definition.bypassesConstruction = true;
                const bool fixed = definition.selectionMode ==
                                   SettlementFootprintSelectionMode::Fixed;
                PALADIN_CHECK(gallery.objectState().placeCompletedObject(
                    gallery.grid(),
                    definition,
                    {{4 + (slot % 3) * 12, 4 + (slot / 3) * 12},
                     fixed ? definition.previewWidth : 6,
                     fixed ? definition.previewHeight : 6}
                ));
                ++slot;
            }
            Camera2D galleryCamera(21, 20);
            galleryCamera.setZoom(4);
            SettlementCitizenState noPeople;
            renderer.beginFrame();
            city.render(
                renderer,
                gallery,
                galleryCamera,
                metrics,
                placement,
                commands,
                noPeople,
                inspection,
                1,
                12
            );
            capture(app, "presentation-all-objects.bmp");
            renderer.endFrame();
            // Real artist assets: no assumptions about their exact paint
            // colors.
            city.artRootOverride =
                (std::filesystem::path(SDL_GetBasePath()) / "assets/sprites")
                    .string();
            SceneSpriteLibrary installed;
            installed.load(renderer, city.artRootOverride);
            PALADIN_CHECK(installed.find("house.roof"));
            PALADIN_CHECK(installed.find("house.wall"));
            PALADIN_CHECK(installed.find("house.floor"));
            // Test the final scene, not just PNG sizes: fitted roofs, animated
            // sprites, cached ground and lighting must obey the same lattice.
            const auto previousZoom = camera.zoom();
            camera.setZoom(16);
            map.naturalFeatures().set({10, 13}, NaturalFeatureKind::Rock);
            draw(12);
            requireUniformWorldPixels(app, 4);
            capture(app, "uniform-pixels-day.bmp");
            renderer.endFrame();
            draw(0);
            requireUniformWorldPixels(app, 4);
            capture(app, "uniform-pixels-night.bmp");
            renderer.endFrame();
            camera.setZoom(previousZoom);
            {
                const SceneProjection view{10, 10, 64, 1280, 720};
                SceneDrawQueue fence;
                for (bool art : {false, true})
                {
                    SceneSpriteLibrary::setEnvironmentArtEnabled(art);
                    fence.clear();
                    pastureFence(fence, view, installed, {{8, 8}, 6, 5}, 998);
                    PALADIN_CHECK(fence.size() > 8);
                    for (const auto& item : fence.items())
                    {
                        PALADIN_CHECK(item.layer == 0);
                    }
                }
                SceneSpriteLibrary::setEnvironmentArtEnabled(true);
                SettlementCitizenState handler;
                SettlementActivityTestFixture::setRenderPerson(handler, {6, 6});
                const auto animalId = map.animals.spawn(map, "cow", {6, 6});
                PALADIN_CHECK(animalId);
                auto* animal = map.animals.find(animalId);
                animal->handler = handler.citizens()[0].id;
                animal->beingLed = true;
                SceneDrawQueue escort;
                Camera2D escortCamera(6.5, 6.5);
                escortCamera.setZoom(16);
                SettlementCitizenRenderer personRenderer;
                personRenderer.render(
                    renderer,
                    handler,
                    escortCamera,
                    metrics,
                    &map.animals,
                    1,
                    &escort,
                    &installed
                );
                const SceneDrawItem* personItem = nullptr;
                for (const auto& item : escort.items())
                {
                    if (item.stableId == ((std::uint64_t(1) << 62) |
                                          handler.citizens()[0].id.value()))
                    {
                        personItem = &item;
                    }
                }
                PALADIN_CHECK(personItem);
                for (const auto& item : escort.items())
                {
                    if (item.stableId == animalId.value() && item.part == 0)
                    {
                        PALADIN_CHECK(
                            item.bounds.x >
                            personItem->bounds.x + personItem->bounds.width * .5
                        );
                    }
                }
                animal->health = 0;
                auto house = *map.objectState().completedObjectAt({12, 12});
                house.objectTypeId = SettlementObjectTypes::House;
                house.footprint = {{8, 8}, 3, 3};
                CityPresentation cutaway;
                cutaway.roofsVisible = false;
                for (unsigned rows : {0u, 1u, 3u})
                {
                    SceneDrawQueue beds;
                    homeDetails(
                        beds,
                        view,
                        installed,
                        cutaway,
                        house,
                        999,
                        rows
                    );
                    int bedCount = 0;
                    for (const auto& item : beds.items())
                    {
                        if (item.part == 3 && item.texture)
                        {
                            ++bedCount;
                        }
                    }
                    PALADIN_CHECK(
                        bedCount == (rows == 0   ? 4
                                     : rows == 1 ? 3
                                                 : 2)
                    );
                }
            }
            city.reloadArt();
            city.presentation.roofsVisible = true;
            if (installed.find("terrain.plain"))
            {
                for (const auto& definition :
                     SettlementObjectCatalog::definitions())
                {
                    PALADIN_CHECK(
                        installed.objectHasArt(std::string(definition.id))
                    );
                }
                for (const char* id :
                     {"tree",
                      "rock",
                      "terrain.water",
                      "terrain.desert",
                      "terrain.tundra",
                      "terrain.mountain",
                      "market.stall",
                      "stockpile.stack",
                      "fishing_grounds.station",
                      "wheat_farm.crop",
                      "world.settlement"})
                {
                    PALADIN_CHECK(installed.find(id));
                }
                // The bakery is a user-sized footprint: its roof must cover
                // the complete 6x6 gallery building, preserving its overhang.
                SceneDrawQueue fitted;
                const SceneProjection fittedView{
                    21,
                    20,
                    20,
                    renderer.outputWidth(),
                    renderer.outputHeight()
                };
                structure.submit(
                    fitted,
                    fittedView,
                    gallery,
                    city.presentation,
                    installed
                );
                const auto* bakeryRoof = installed.find("bakery.roof.full");
                PALADIN_CHECK(bakeryRoof);
                bool bakeryCovered = false;
                float roofTop = 100000, roofBottom = -100000;
                for (const auto& item : fitted.items())
                {
                    if (item.texture == bakeryRoof->texture.get() &&
                        item.groundDepth == 34)
                    {
                        bakeryCovered =
                            item.bounds.width > 120 && item.bounds.width < 140;
                        roofTop = std::min(roofTop, item.bounds.y);
                        roofBottom = std::max(
                            roofBottom,
                            item.bounds.y + item.bounds.height
                        );
                    }
                }
                PALADIN_CHECK(bakeryCovered);
                PALADIN_CHECK(
                    roofBottom - roofTop >= 120 && roofBottom - roofTop < 140
                );
                if (installed.find("grass.tuft"))
                {
                    if (installed.find("tree.trunk.1"))
                    {
                        const SceneProjection treeView{0, 0, 64, 1280, 720};
                        SceneDrawQueue still, repeat, moving;
                        installed.setTime(0);
                        PALADIN_CHECK(
                            installed.submitTree(still, treeView, 0, 0, 0, 1)
                        );
                        PALADIN_CHECK(
                            installed.submitTree(repeat, treeView, 0, 0, 0, 1)
                        );
                        installed.setTime(1);
                        PALADIN_CHECK(
                            installed.submitTree(moving, treeView, 0, 0, 0, 1)
                        );
                        PALADIN_CHECK(
                            still.size() == 3 && repeat.size() == 3 &&
                            moving.size() == 3
                        );
                        PALADIN_CHECK(
                            still.items()[0].bounds.x ==
                            moving.items()[0].bounds.x
                        );
                        PALADIN_CHECK(
                            still.items()[2].bounds.x !=
                            moving.items()[2].bounds.x
                        );
                        for (std::size_t i = 0; i < 3; ++i)
                        {
                            PALADIN_CHECK(
                                still.items()[i].texture ==
                                repeat.items()[i].texture
                            );
                        }
                        bool differentTrunk = false, differentBranch = false,
                             differentCrown = false;
                        for (std::uint64_t seed = 1; seed < 64; ++seed)
                        {
                            SceneDrawQueue variant;
                            installed
                                .submitTree(variant, treeView, 0, 0, seed, 1);
                            differentTrunk |= variant.items()[0].texture !=
                                              still.items()[0].texture;
                            differentBranch |= variant.items()[1].texture !=
                                               still.items()[1].texture;
                            differentCrown |= variant.items()[2].texture !=
                                              still.items()[2].texture;
                        }
                        PALADIN_CHECK(
                            differentTrunk && differentBranch && differentCrown
                        );
                    }
                    {
                        SettlementGrid sparse(16, 16);
                        for (int y = 0; y < 16; ++y)
                        {
                            for (int x = 0; x < 16; ++x)
                            {
                                sparse.tile({x, y})->terrain =
                                    TerrainType::Land;
                            }
                        }
                        SettlementMap
                            distant(std::move(sparse), {0, 0}, 1, 1, 16, 701);
                        distant.naturalFeatures().set(
                            {8, 8},
                            NaturalFeatureKind::Tree
                        );
                        SettlementNaturalFeatureRenderer features;
                        Camera2D farCamera(8.5, 8.5);
                        farCamera.setZoom(2);
                        SceneDrawQueue farQueue;
                        const auto drawFar = [&]()
                        {
                            renderer.beginFrame();
                            farQueue.clear();
                            features.render(
                                renderer,
                                distant,
                                farCamera,
                                metrics,
                                &farQueue,
                                &installed,
                                &city.presentation
                            );
                        };
                        for (int frame = 0; frame < 12; ++frame)
                        {
                            drawFar();
                            SDL_Delay(20);
                        }
                        PALADIN_CHECK(farQueue.size() == 0);
                        const auto sample = [&]()
                        {
                            auto* surface = SDL_RenderReadPixels(
                                SDL_GetRenderer(app.window_->nativeHandle()),
                                nullptr
                            );
                            PALADIN_CHECK(surface);
                            RenderColor c;
                            PALADIN_CHECK(SDL_ReadSurfacePixel(
                                surface,
                                renderer.outputWidth() / 2,
                                renderer.outputHeight() / 2 - 5,
                                &c.red,
                                &c.green,
                                &c.blue,
                                &c.alpha
                            ));
                            SDL_DestroySurface(surface);
                            return c;
                        };
                        const auto crown = sample();
                        PALADIN_CHECK(
                            crown.green > crown.red && crown.green > 40
                        );
                        distant.naturalFeatures().set(
                            {8, 8},
                            NaturalFeatureKind::None
                        );
                        drawFar();
                        const auto cleared = sample();
                        PALADIN_CHECK(
                            cleared.red == 18 && cleared.green == 20 &&
                            cleared.blue == 24
                        );
                    }
                    const SceneProjection motionView{0, 0, 64, 1280, 720};
                    for (const auto* name : {"grass.tuft", "house.roof.full"})
                    {
                        SceneDrawQueue a, b;
                        installed.setTime(0);
                        installed.placed(a, motionView, name, 0, 0, 0, 0);
                        installed.setTime(1);
                        installed.placed(b, motionView, name, 0, 0, 0, 0);
                        if (std::string_view(name) == "grass.tuft")
                        {
                            a.clear();
                            b.clear();
                            installed.setTime(0);
                            installed.submit(a, motionView, name, 0, 0, 0);
                            installed.setTime(1);
                            installed.submit(b, motionView, name, 0, 0, 0);
                        }
                        PALADIN_CHECK(a.size() == b.size());
                        bool moved = false;
                        for (std::size_t i = 0; i < a.size(); i++)
                        {
                            moved |=
                                a.items()[i].bounds.x != b.items()[i].bounds.x;
                        }
                        PALADIN_CHECK(moved);
                    }
                    {
                        GrassPresentation grass;
                        const SceneProjection view{7, 7, 64, 1280, 720};
                        SceneDrawQueue still, bent, paused, recovered;
                        installed.setTime(0);
                        grass.submit(still, view, gallery, installed);
                        PALADIN_CHECK(still.size() > 0);
                        const auto& b = still.items().front().bounds;
                        const double gx =
                            view.cameraX +
                            (b.x + b.width * .5 - view.screenWidth * .5) /
                                view.tilePixels;
                        const double gy =
                            view.cameraY +
                            (b.y + b.height - view.screenHeight * .5) /
                                view.tilePixels;
                        SettlementCitizenState walker;
                        SettlementActivityTestFixture::setRenderPerson(
                            walker,
                            {int(gx), int(gy)}
                        );
                        auto& c = const_cast<SettlementCitizen&>(
                            walker.citizens().front()
                        );
                        c.path = {{int(gx) + 1, int(gy)}};
                        c.pathIndex = 0;
                        installed.setTime(.1);
                        grass.submit(bent, view, gallery, installed, &walker);
                        grass.submit(paused, view, gallery, installed, &walker);
                        PALADIN_CHECK(bent.size() == paused.size());
                        bool reaction = false;
                        for (std::size_t i = 0; i < bent.size(); ++i)
                        {
                            reaction |= bent.items()[i].bounds.height <
                                        still.items()[i].bounds.height;
                            PALADIN_CHECK(
                                bent.items()[i].bounds.x ==
                                paused.items()[i].bounds.x
                            );
                            PALADIN_CHECK(
                                bent.items()[i].bounds.height ==
                                paused.items()[i].bounds.height
                            );
                        }
                        PALADIN_CHECK(reaction);
                        for (int frame = 2; frame <= 40; ++frame)
                        {
                            recovered.clear();
                            installed.setTime(frame * .1);
                            grass.submit(recovered, view, gallery, installed);
                        }
                        for (std::size_t i = 0; i < still.size(); ++i)
                        {
                            PALADIN_CHECK(
                                std::abs(
                                    recovered.items()[i].bounds.height -
                                    still.items()[i].bounds.height
                                ) < .1
                            );
                        }
                        installed.setTime(0);
                    }
                    SettlementStructurePresentation doors;
                    const SceneProjection doorView{13.5, 13.5, 64, 1280, 720};
                    SettlementCitizenState passer;
                    SettlementActivityTestFixture::setRenderPerson(
                        passer,
                        {13, 14}
                    );
                    const auto doorVisible =
                        [&](double seconds,
                            const SettlementCitizenState& traffic)
                    {
                        installed.setTime(seconds);
                        SceneDrawQueue q;
                        doors.submit(
                            q,
                            doorView,
                            map,
                            city.presentation,
                            installed,
                            &traffic
                        );
                        return std::any_of(
                            q.items().begin(),
                            q.items().end(),
                            [](const auto& item)
                            {
                                return !item.texture && item.part == 13 &&
                                       item.color.red == 8;
                            }
                        );
                    };
                    PALADIN_CHECK(!doorVisible(0, passer));
                    PALADIN_CHECK(doorVisible(.3, passer));
                    PALADIN_CHECK(doorVisible(1.2, noPeople));
                    PALADIN_CHECK(!doorVisible(1.5, noPeople));
                    installed.setTime(0);
                }
                const auto renderGallery = [&](const char* filename, int hour)
                {
                    renderer.beginFrame();
                    city.render(
                        renderer,
                        gallery,
                        galleryCamera,
                        metrics,
                        placement,
                        commands,
                        noPeople,
                        inspection,
                        1,
                        hour
                    );
                    capture(app, filename);
                    renderer.endFrame();
                };
                galleryCamera.setZoom(5);
                renderGallery("environment-v4-day.bmp", 12);
                city.presentation.roofsVisible = false;
                renderGallery("environment-v4-cutaway.bmp", 12);
                city.presentation.roofsVisible = true;
                renderGallery("environment-v4-night.bmp", 0);
                SceneSpriteLibrary::setEnvironmentArtEnabled(false);
                renderGallery("environment-v4-off.bmp", 12);
                SceneSpriteLibrary::setEnvironmentArtEnabled(true);
                renderGallery("environment-v4-restored.bmp", 12);
                galleryCamera.setPosition(5.5, 7.5);
                galleryCamera.setZoom(16);
                renderGallery("keep-close.bmp", 12);
            }
            camera.setZoom(16);
            if (installed.find("logging_grounds.stack"))
            {
                const auto& home = map.objectState().completedObjects().front();
                map.logistics.synchronize(map.objectState(), 0);
                const auto fuel = map.logistics.forObject(home.id);
                PALADIN_CHECK(map.logistics.add(fuel, "lumber", 1));
                const std::unordered_set<SettlementObjectId, StrongIdHash>
                    occupied{home.id};
                const SceneProjection view{13.5, 13.5, 64, 1280, 720};
                SceneDrawQueue cold, hot, paused, moving;
                installed.setTime(0);
                homeChimney(cold, view, installed, map, home, 1);
                map.heating.advance(map.logistics, occupied, 0, 1);
                homeChimney(hot, view, installed, map, home, 1);
                homeChimney(paused, view, installed, map, home, 1);
                PALADIN_CHECK(hot.size() > cold.size());
                PALADIN_CHECK(hot.size() == paused.size());
                for (std::size_t i = 0; i < hot.size(); ++i)
                {
                    PALADIN_CHECK(
                        hot.items()[i].bounds.y == paused.items()[i].bounds.y
                    );
                }
                installed.setTime(1);
                homeChimney(moving, view, installed, map, home, 1);
                PALADIN_CHECK(
                    hot.items().back().bounds.y !=
                    moving.items().back().bounds.y
                );
                city.animationTimeOverride = 0;
                draw(12);
                capture(app, "home-heated.bmp");
                renderer.endFrame();
                city.animationTimeOverride = 1;
                draw(12);
                capture(app, "home-smoke-moving.bmp");
                renderer.endFrame();
                map.heating.advance(map.logistics, occupied, 1, 1440);
                city.animationTimeOverride = 0;
                draw(12);
                capture(app, "home-cold.bmp");
                renderer.endFrame();

                SettlementGrid loggingGrid(32, 32);
                for (int y = 0; y < 32; ++y)
                {
                    for (int x = 0; x < 32; ++x)
                    {
                        loggingGrid.tile({x, y})->terrain = TerrainType::Land;
                        loggingGrid.tile({x, y})->biome = BiomeType::Plain;
                    }
                }
                SettlementMap
                    loggingMap(std::move(loggingGrid), {0, 0}, 1, 1, 32, 701);
                auto logging = *SettlementObjectCatalog::definition(
                    SettlementObjectTypes::LoggingGrounds
                );
                logging.bypassesConstruction = true;
                PALADIN_CHECK(loggingMap.objectState().placeCompletedObject(
                    loggingMap.grid(),
                    logging,
                    {{12, 12}, 6, 6}
                ));
                SettlementCitizenState loggers;
                SettlementActivityTestFixture::setLogger(
                    loggers,
                    loggingMap.objectState().completedObjects().front().id
                );
                auto road = *SettlementObjectCatalog::definition(
                    SettlementObjectTypes::Road
                );
                road.bypassesConstruction = true;
                for (int x = 10; x < 20; ++x)
                {
                    PALADIN_CHECK(loggingMap.objectState().placeCompletedObject(
                        loggingMap.grid(),
                        road,
                        {{x, 19}, 1, 1}
                    ));
                }
                for (int y = 17; y < 19; ++y)
                {
                    PALADIN_CHECK(loggingMap.objectState().placeCompletedObject(
                        loggingMap.grid(),
                        road,
                        {{10, y}, 1, 1}
                    ));
                }
                Camera2D loggingCamera;
                loggingCamera.setPosition(15, 16);
                loggingCamera.setZoom(16);
                // This fixture's first tree has a 17.7-second phase offset.
                for (int phase : {8, 20, 0})
                {
                    city.animationTimeOverride = phase;
                    renderer.beginFrame();
                    city.render(
                        renderer,
                        loggingMap,
                        loggingCamera,
                        metrics,
                        placement,
                        commands,
                        loggers,
                        inspection,
                        1,
                        12
                    );
                    capture(
                        app,
                        phase == 8    ? "logging-mature.bmp"
                        : phase == 20 ? "logging-stump.bmp"
                                      : "logging-regrowing.bmp"
                    );
                    renderer.endFrame();
                }
                city.animationTimeOverride = -1;
                installed.setTime(0);
            }
            draw(12);
            capture(app, "art-house-day.bmp");
            renderer.endFrame();
            if (installed.find("terrain.beach"))
            {
                SettlementGrid coastGrid(64, 48);
                for (int y = 0; y < 48; ++y)
                {
                    for (int x = 0; x < 64; ++x)
                    {
                        auto& t = *coastGrid.tile({x, y});
                        t.terrain = x >= 32 + (y / 8) % 2 ? TerrainType::Water
                                                          : TerrainType::Land;
                        t.biome = t.terrain == TerrainType::Water
                                      ? BiomeType::Ocean
                                      : BiomeType::Plain;
                        t.elevation = Elevation{.30F};
                        if (y >= 16 && y < 20 && x == 31)
                        {
                            t.terrain = TerrainType::Mountain;
                            t.elevation = Elevation{.8F};
                        }
                    }
                }
                coastGrid.classifyCoast(701);
                int beaches = 0, coasts = 0, shallow = 0, deep = 0;
                for (int y = 0; y < 48; ++y)
                {
                    for (int x = 0; x < 64; ++x)
                    {
                        const auto type = coastGrid.cityTileType({x, y});
                        beaches += type == CityTileType::Beach;
                        coasts += type == CityTileType::Coast;
                        shallow += type == CityTileType::ShallowWater;
                        deep += type == CityTileType::DeepWater;
                        if (type == CityTileType::Beach)
                        {
                            PALADIN_CHECK(
                                coastGrid.tile({x, y})->terrain ==
                                TerrainType::Land
                            );
                        }
                    }
                }
                PALADIN_CHECK(
                    beaches > 0 && coasts > 0 && shallow > 0 && deep > 0
                );
                PALADIN_CHECK(
                    coastGrid.cityTileType({31, 17}) == CityTileType::Coast
                );
                SettlementMap
                    coastMap(std::move(coastGrid), {0, 0}, 1, 1, 64, 701);
                PALADIN_CHECK(coastMap.objectState().placeCompletedObject(
                    coastMap.grid(),
                    fixtureHouse,
                    {{25, 22}, 3, 3},
                    SettlementTilePosition{26, 24}
                ));
                Camera2D coastCamera(31, 24);
                coastCamera.setZoom(8);
                CityRenderer coastRenderer;
                const auto coastShot = [&](const char* name, double time)
                {
                    coastRenderer.animationSeconds = time;
                    renderer.beginFrame();
                    coastRenderer.render(
                        renderer,
                        coastMap,
                        coastCamera,
                        metrics,
                        placement,
                        commands,
                        noPeople,
                        inspection,
                        1,
                        12
                    );
                    capture(app, name);
                    renderer.endFrame();
                };
                coastShot("coast-day.bmp", 0);
                coastShot("coast-paused.bmp", 0);
                coastShot("coast-moving.bmp", .5);
                for (double zoom : {16., 4., 8.})
                {
                    coastCamera.setZoom(zoom);
                    coastShot("coast-zoom.bmp", .5);
                }
            }
            if (installed.find("tree.trunk.1"))
            {
                for (const auto at :
                     {SettlementTilePosition{11, 12},
                      {15, 12},
                      {11, 14},
                      {15, 14},
                      {12, 15},
                      {14, 15},
                      {12, 10},
                      {14, 10}})
                {
                    map.naturalFeatures().set(at, NaturalFeatureKind::Tree);
                }
                for (int x = 7; x <= 21; x += 2)
                {
                    map.naturalFeatures().set(
                        {x, 17},
                        NaturalFeatureKind::Tree
                    );
                }
                const auto featureCount =
                    map.naturalFeatures().countIn({{0, 0}, 32, 32});
                SettlementNaturalFeatureRenderer clearanceRenderer;
                SceneDrawQueue clearanceQueue;
                clearanceRenderer.render(
                    renderer,
                    map,
                    camera,
                    metrics,
                    &clearanceQueue,
                    &installed,
                    &city.presentation
                );
                const SceneProjection clearanceView{
                    camera.tileX(),
                    camera.tileY(),
                    metrics.scaledTilePixels(camera.zoom()),
                    renderer.outputWidth(),
                    renderer.outputHeight()
                };
                const auto protectedHouse =
                    clearanceView.bounds({11.70, 11.07, 0, 3.60, 4.01, 0, 0});
                int crowns = 0;
                for (const auto& item : clearanceQueue.items())
                {
                    bool crown = false;
                    for (int i = 1; i <= 3; ++i)
                    {
                        crown |=
                            item.texture ==
                            installed.find("tree.crown." + std::to_string(i))
                                ->texture.get();
                    }
                    if (!crown)
                    {
                        continue;
                    }
                    ++crowns;
                    const auto& b = item.bounds;
                    PALADIN_CHECK(
                        b.x + b.width <= protectedHouse.x ||
                        b.x >= protectedHouse.x + protectedHouse.width ||
                        b.y + b.height <= protectedHouse.y ||
                        b.y >= protectedHouse.y + protectedHouse.height
                    );
                }
                PALADIN_CHECK(crowns >= 16);
                PALADIN_CHECK(
                    map.naturalFeatures().countIn({{0, 0}, 32, 32}) ==
                    featureCount
                );
                city.animationTimeOverride = 0;
                draw(12);
                capture(app, "tree-clearance-day.bmp");
                renderer.endFrame();
                draw(0);
                capture(app, "tree-clearance-night.bmp");
                renderer.endFrame();
                city.animationTimeOverride = 1;
                draw(0);
                capture(app, "tree-clearance-night-motion.bmp");
                renderer.endFrame();
                city.animationTimeOverride = -1;
                for (const auto at :
                     {SettlementTilePosition{11, 12},
                      {15, 12},
                      {11, 14},
                      {15, 14},
                      {12, 15},
                      {14, 15},
                      {12, 10},
                      {14, 10}})
                {
                    map.naturalFeatures().set(at, NaturalFeatureKind::None);
                }
                for (int x = 7; x <= 21; x += 2)
                {
                    map.naturalFeatures().set(
                        {x, 17},
                        NaturalFeatureKind::None
                    );
                }
            }
            city.presentation.roofsVisible = false;
            draw(12);
            capture(app, "art-house-cutaway.bmp");
            renderer.endFrame();
            city.presentation.roofsVisible = true;
            city.presentation.localLightsEnabled = false;
            draw(0);
            const auto dark = pixel(
                renderer.outputWidth() / 2 + 75,
                renderer.outputHeight() / 2 + 85
            );
            renderer.endFrame();
            city.presentation.localLightsEnabled = true;
            draw(0);
            capture(app, "art-house-night.bmp");
            const auto lit = pixel(
                renderer.outputWidth() / 2 + 75,
                renderer.outputHeight() / 2 + 85
            );
            PALADIN_CHECK(lit.red > dark.red + 20);
            renderer.endFrame();
            if (installed.find("grass.tuft"))
            {
                city.animationTimeOverride = 0;
                draw(12);
                capture(app, "v5-door-closed.bmp");
                renderer.endFrame();
                SettlementActivityTestFixture::setRenderPerson(
                    people,
                    {13, 14}
                );
                city.animationTimeOverride = .08;
                draw(12);
                capture(app, "v5-door-opening.bmp");
                renderer.endFrame();
                city.animationTimeOverride = .32;
                draw(12);
                capture(app, "v5-door-open.bmp");
                renderer.endFrame();
                SettlementActivityTestFixture::setRenderPerson(
                    people,
                    {10, 12}
                );
                city.animationTimeOverride = 2;
                draw(12);
                renderer.endFrame();
                city.animationTimeOverride = 2.3;
                draw(12);
                capture(app, "v5-door-reclosed.bmp");
                renderer.endFrame();
                city.animationTimeOverride = -1;
            }
            // Strict authored palette validation; generated lighting is
            // unrestricted.
            std::ofstream(directory / "art-palette.hex") << "#00FF00\n";
            library.reset();
            library.load(renderer, directory.string());
            PALADIN_CHECK(!library.find("citizen"));
            std::ofstream(directory / "art-palette.hex") << "#FF0000\n";
            std::ofstream(directory / "sprites.catalog", std::ios::app)
                << "animated alpha.png 1 1 .5 1 0 0 2 2\n";
            library.reset();
            library.load(renderer, directory.string());
            PALADIN_CHECK(library.find("citizen"));
            PALADIN_CHECK(library.find("animated"));
            library.setTime(.75);
            PALADIN_CHECK(library.frame(*library.find("animated")).x == 1);
            PALADIN_CHECK(library.frame(*library.find("animated")).width == 1);
            SceneDrawQueue attachment;
            PALADIN_CHECK(
                library.placed(attachment, projection, "citizen", 0, 0, 7, 1)
            );
            PALADIN_CHECK(attachment.items().front().groundDepth == 7);
            // Reset fixture input for repeatability.
            std::ofstream(directory / "art-palette.hex") << "#FF0000\n";
        }

        static void run()
        {
            Application app;
            PALADIN_CHECK(app.renderer_ && app.renderer_->isValid());
            {
                SimulationClock clock(20);
                clock.setPaused(false);
                clock.setSpeedMultiplier(100);
                clock.beginFrame();
                SDL_Delay(30);
                clock.beginFrame();
                PALADIN_CHECK(clock.backlogTicks() <= 4.00001);
                PALADIN_CHECK(
                    clock.limitHits > 0 && clock.discardedSeconds > 0
                );
                PALADIN_CHECK(clock.interpolationAlpha() <= 1);
                int ticks = 0;
                while (clock.shouldTick() && ticks < 10)
                {
                    clock.consumeTick();
                    ++ticks;
                }
                PALADIN_CHECK(ticks <= 4);
                clock.setPaused(true);
                const auto time = clock.presentationSeconds();
                clock.beginFrame();
                PALADIN_CHECK(clock.presentationSeconds() == time);
            }
            if (SDL_getenv("PALADIN_ART_REVIEW"))
            {
                SettlementGrid grid(64, 64);
                for (int y = 0; y < 64; ++y)
                {
                    for (int x = 0; x < 64; ++x)
                    {
                        auto& t = *grid.tile({x, y});
                        t.terrain = TerrainType::Land;
                        t.biome = BiomeType::Plain;
                        t.temperature = Temperature{.5F};
                        t.rainfall = Rainfall{.5F};
                    }
                }
                SettlementMap map(std::move(grid), {100, 100}, 1, 1, 64, 701);
                const auto add = [&](std::string_view type,
                                     SettlementObjectFootprint f,
                                     int direction = 2)
                {
                    auto definition =
                        *SettlementObjectCatalog::definition(type);
                    definition.bypassesConstruction = true;
                    PALADIN_CHECK(map.objectState().placeCompletedObject(
                        map.grid(),
                        definition,
                        f,
                        definition.hasDoor
                            ? std::optional(centerDoor(f, direction))
                            : std::nullopt
                    ));
                    return map.objectState().completedObjects().back().id;
                };
                for (int side = 0; side < 4; ++side)
                {
                    add(SettlementObjectTypes::House,
                        {{15 + side * 5, 16}, 3, 3},
                        side);
                }
                add(SettlementObjectTypes::CityKeep, {{11, 22}, 3, 7});
                add(SettlementObjectTypes::Bakery, {{18, 23}, 3, 3});
                const auto stockpile =
                    add(SettlementObjectTypes::Stockpile, {{25, 23}, 6, 4});
                add(SettlementObjectTypes::WheatFarm, {{33, 23}, 4, 4});
                for (int x = 13; x <= 35; ++x)
                {
                    add(SettlementObjectTypes::Road, {{x, 20}, 1, 1});
                }
                for (int x = 9; x <= 36; x += 3)
                {
                    map.naturalFeatures().set(
                        {x, 31},
                        NaturalFeatureKind::Tree
                    );
                }
                for (int x = 9; x < 34; x += 5)
                {
                    map.naturalFeatures().set(
                        {x, 14},
                        NaturalFeatureKind::Rock
                    );
                }
                map.logistics.synchronize(map.objectState(), 0);
                SceneSpriteLibrary art;
                art.load(
                    *app.renderer_,
                    (std::filesystem::path(SDL_GetBasePath()) /
                     "assets/sprites")
                        .string()
                );
                SceneDrawQueue goods;
                const SceneProjection p{23, 23, 40, 1280, 720};
                const auto* store =
                    map.objectState().completedObject(stockpile);
                PALADIN_CHECK(stockpilePresentation(
                    goods,
                    p,
                    art,
                    map,
                    *store,
                    stockpile.value()
                ));
                const auto emptyItems = goods.size();
                CityRenderer city;
                city.animationTimeOverride = 0;
                Camera2D camera;
                camera.setPosition(23, 23);
                camera.setZoom(10);
                TileRenderMetrics metrics;
                SettlementObjectPlacementController placement;
                SettlementCommandController commands;
                SettlementInspectionController inspection;
                SettlementCitizenState people;
                const auto draw = [&](double hour)
                {
                    app.renderer_->beginFrame();
                    city.render(
                        *app.renderer_,
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
                };
                const auto settle = [&](double hour)
                {
                    const auto until = SDL_GetTicks() + 600;
                    do
                    {
                        draw(hour);
                        SDL_Delay(1);
                    } while (SDL_GetTicks() < until);
                };
                if (!SDL_getenv("PALADIN_TERRAIN_REVIEW"))
                {
                    settle(12);
                    capture(app, "tribal-stockpile-empty.bmp");
                    PALADIN_CHECK(map.logistics.add(
                        map.logistics.forObject(stockpile),
                        "lumber",
                        56
                    ));
                    PALADIN_CHECK(map.logistics.add(
                        map.logistics.forObject(stockpile),
                        "stone",
                        20
                    ));
                    PALADIN_CHECK(map.logistics.add(
                        map.logistics.forObject(stockpile),
                        "food",
                        25
                    ));
                    goods.clear();
                    stockpilePresentation(
                        goods,
                        p,
                        art,
                        map,
                        *store,
                        stockpile.value()
                    );
                    PALADIN_CHECK(goods.size() > emptyItems);
                    settle(12);
                    capture(app, "tribal-day.bmp");
                    PALADIN_CHECK(inspection.selectAt(
                        {15, 17},
                        map.objectState(),
                        people,
                        true,
                        &map.logistics
                    ));
                    settle(12);
                    SettlementInspectionPanel homePanel;
                    homePanel.render(
                        *app.renderer_,
                        *app.grayUiRenderer_,
                        inspection,
                        map,
                        people,
                        camera,
                        metrics
                    );
                    capture(app, "tribal-home-panel.bmp");
                    const auto panelBounds = homePanel.renderedBounds_;
                    PALADIN_CHECK(homePanel.pointerPressed(
                        panelBounds.x + 25,
                        panelBounds.y + 20
                    ));
                    homePanel.pointerReleased(
                        panelBounds.x + 25,
                        panelBounds.y + 20,
                        map,
                        people,
                        0
                    );
                    PALADIN_CHECK(
                        inspection.selectedObject(map.objectState())
                            ->homeLevel == 1
                    );
                    PALADIN_CHECK(!homePanel.detached_ && !homePanel.dragging_);
                    inspection.clear();
                    city.presentation.roofsVisible = false;
                    settle(12);
                    capture(app, "tribal-interiors.bmp");
                    city.presentation.roofsVisible = true;
                    settle(23);
                    capture(app, "tribal-night.bmp");
                    camera.setPosition(23, 23);
                    for (double pixels : {.5, 2., 4., 8., 16., 24., 40.})
                    {
                        camera.setZoom(pixels / 4);
                        settle(12);
                        capture(
                            app,
                            ("tribal-zoom-" + std::to_string(int(pixels * 10)) +
                             ".bmp")
                                .c_str()
                        );
                    }
                }
                {
                    SettlementGrid terrain(48, 48);
                    WorldGrid worldTerrain(48, 48);
                    for (int y = 0; y < 48; ++y)
                    {
                        for (int x = 0; x < 48; ++x)
                        {
                            auto& t = *terrain.tile({x, y});
                            t.terrain = x > 28 + int(2 * std::sin(y * .3))
                                            ? TerrainType::Water
                                            : TerrainType::Land;
                            t.biome = BiomeType::Plain;
                            t.temperature = Temperature{.5F};
                            if (std::hypot(x - 18., y - 19.) < 7 ||
                                std::hypot(x - 15., y - 25.) < 5)
                            {
                                t.terrain = TerrainType::Mountain;
                            }
                            *worldTerrain.tile({x, y}) = t;
                        }
                    }
                    terrain.classifyCoast(701);
                    camera.setPosition(24, 24);
                    camera.setZoom(8);
                    WorldGridRenderer cityTerrain, worldRenderer;
                    for (int frame = 0; frame < 12; ++frame)
                    {
                        app.renderer_->beginFrame();
                        cityTerrain.render(
                            *app.renderer_,
                            terrain,
                            camera,
                            metrics,
                            &art
                        );
                        SDL_Delay(30);
                    }
                    capture(app, "tribal-city-mountains-water.bmp");
                    for (int frame = 0; frame < 12; ++frame)
                    {
                        app.renderer_->beginFrame();
                        worldRenderer.render(
                            *app.renderer_,
                            worldTerrain,
                            camera,
                            metrics,
                            &art
                        );
                        SDL_Delay(30);
                    }
                    capture(app, "tribal-world-mountains-water.bmp");
                }
                return;
            }
            if (SDL_getenv("PALADIN_RENDER_BENCHMARK"))
            {
                SettlementGrid grid(256, 256);
                for (int y = 0; y < 256; ++y)
                {
                    for (int x = 0; x < 256; ++x)
                    {
                        auto& t = *grid.tile({x, y});
                        t.terrain =
                            x >= 192 ? TerrainType::Water : TerrainType::Land;
                        t.biome =
                            x >= 192 ? BiomeType::Ocean : BiomeType::Plain;
                        t.elevation = Elevation{.3F};
                    }
                }
                grid.classifyCoast(701);
                SettlementMap map(std::move(grid), {0, 0}, 1, 1, 256, 701);
                auto house = *SettlementObjectCatalog::definition(
                    SettlementObjectTypes::House
                );
                house.bypassesConstruction = true;
                auto road = *SettlementObjectCatalog::definition(
                    SettlementObjectTypes::Road
                );
                road.bypassesConstruction = true;
                for (int y = 40; y < 88; y += 6)
                {
                    for (int x = 40; x < 88; x += 6)
                    {
                        PALADIN_CHECK(map.objectState().placeCompletedObject(
                            map.grid(),
                            house,
                            {{x, y}, 3, 3}
                        ));
                    }
                }
                for (int y = 44; y < 92; y += 6)
                {
                    for (int x = 40; x < 88; ++x)
                    {
                        PALADIN_CHECK(map.objectState().placeCompletedObject(
                            map.grid(),
                            road,
                            {{x, y}, 1, 1}
                        ));
                    }
                }
                for (int y = 0; y < 256; y += 2)
                {
                    for (int x = SDL_getenv("PALADIN_DENSE_FOREST") ? 0 : 165;
                         x < 190;
                         x += 2)
                    {
                        map.naturalFeatures().set(
                            {x, y},
                            NaturalFeatureKind::Tree
                        );
                    }
                }
                CityRenderer city;
                Camera2D camera;
                TileRenderMetrics metrics;
                SettlementObjectPlacementController placement;
                SettlementCommandController commands;
                SettlementInspectionController inspection;
                SettlementCitizenState people;
                if (SDL_getenv("PALADIN_CITY_SIM_BENCHMARK"))
                {
                    auto keep = *SettlementObjectCatalog::definition(
                        SettlementObjectTypes::CityKeep
                    );
                    keep.bypassesConstruction = true;
                    PALADIN_CHECK(map.objectState().placeCompletedObject(
                        map.grid(),
                        keep,
                        {{96, 60}, keep.previewWidth, keep.previewHeight}
                    ));
                    map.logistics.synchronize(map.objectState(), 0);
                    PALADIN_CHECK(people.initialize(128, 701));
                    people.placeUnpositionedCitizens(map);
                    double total = 0, worst = 0;
                    for (int i = 0; i < 240; ++i)
                    {
                        const auto start = SDL_GetTicksNS();
                        map.activities.tick(map, people, 1200 + i * .25, .25);
                        people.tickMovement(map, .25);
                        const double ms =
                            double(SDL_GetTicksNS() - start) / 1e6;
                        total += ms;
                        worst = std::max(worst, ms);
                    }
                    std::cout
                        << "city_activity residents=128 steps=240 mean_ms="
                        << total / 240 << " worst_ms=" << worst << std::endl;
                    return;
                }
                const auto imageHash = [&]()
                {
                    auto* source = SDL_RenderReadPixels(
                        SDL_GetRenderer(app.window_->nativeHandle()),
                        nullptr
                    );
                    PALADIN_CHECK(source);
                    auto* rgba =
                        SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32);
                    PALADIN_CHECK(rgba);
                    std::uint64_t hash = 1469598103934665603ull;
                    for (int y = 0; y < rgba->h; ++y)
                    {
                        const auto* row =
                            static_cast<const Uint8*>(rgba->pixels) +
                            y * rgba->pitch;
                        for (int x = 0; x < rgba->w * 4; ++x)
                        {
                            hash = (hash ^ row[x]) * 1099511628211ull;
                        }
                    }
                    SDL_DestroySurface(rgba);
                    SDL_DestroySurface(source);
                    return hash;
                };
                for (double tp : {1., 4., 8., 16., 32., 64.})
                {
                    camera.setPosition(72, 72);
                    camera.setZoom(tp / 4);
                    auto frame = [&](double seconds, double hour = 12)
                    {
                        city.animationSeconds = seconds;
                        app.renderer_->beginFrame();
                        city.render(
                            *app.renderer_,
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
                        app.renderer_->endFrame();
                    };
                    const auto cold = SDL_GetTicksNS();
                    frame(0);
                    const double coldMs = double(SDL_GetTicksNS() - cold) / 1e6;
                    for (int i = 0; i < 3; ++i)
                    {
                        frame(i * .1);
                    }
                    const auto start = SDL_GetTicksNS();
                    for (int i = 0; i < 2; ++i)
                    {
                        frame(i * .1);
                    }
                    std::cout
                        << "render tile_px=" << tp << " cold_ms=" << coldMs
                        << " warm_ms=" << double(SDL_GetTicksNS() - start) / 2e6
                        << " items=" << city.submittedItems() << std::endl;
                    if (tp <= 16)
                    {
                        PALADIN_CHECK(city.submittedItems() == 0);
                    }
                    PALADIN_CHECK(
                        tp >= StaticDetailPixels || city.submittedItems() < 2000
                    );
                    if (tp < StaticDetailPixels)
                    {
                        for (int i = 0; i < 280; ++i)
                        {
                            frame(0);
                        }
                        const auto day = imageHash();
                        frame(10);
                        PALADIN_CHECK(day == imageHash());
                        frame(0, 23);
                        const auto night = imageHash();
                        frame(10, 23);
                        PALADIN_CHECK(night == imageHash());
                        PALADIN_CHECK(day != night);
                        frame(0);
                    }
                    capture(
                        app,
                        ("performance-" + std::to_string(int(tp)) + "px.bmp")
                            .c_str()
                    );
                }
                for (const double focus : {72., 175.})
                {
                    camera.setPosition(focus, focus == 72 ? 72 : 128);
                    double zoomWorst = 0, zoomTotal = 0;
                    for (int i = 0; i < 40; ++i)
                    {
                        const double tp = 2 + (i < 20 ? i : 39 - i) * 1.6;
                        camera.setZoom(tp / 4);
                        const auto start = SDL_GetTicksNS();
                        app.renderer_->beginFrame();
                        city.render(
                            *app.renderer_,
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
                        app.renderer_->endFrame();
                        const double ms = (SDL_GetTicksNS() - start) / 1e6;
                        zoomWorst = std::max(zoomWorst, ms);
                        zoomTotal += ms;
                    }
                    std::cout << "continuous_zoom focus_x=" << focus
                              << " mean_ms=" << zoomTotal / 40
                              << " worst_ms=" << zoomWorst << std::endl;
                }
                SceneSpriteLibrary cacheArt;
                cacheArt.load(
                    *app.renderer_,
                    (std::filesystem::path(SDL_GetBasePath()) /
                     "assets/sprites")
                        .string()
                );
                SettlementGroundCache groundCache;
                SceneDrawQueue groundQueue;
                const auto roadId =
                    map.objectState().completedObjectAt({40, 44})->id;
                const SceneProjection groundProjection{40, 44, 32, 1280, 720};
                const auto cachedRoad = [&]()
                {
                    groundQueue.clear();
                    groundCache.begin(map, cacheArt);
                    PALADIN_CHECK(groundCache.submit(
                        app.renderer_.get(),
                        groundQueue,
                        groundProjection,
                        map,
                        cacheArt,
                        *map.objectState().completedObject(roadId),
                        roadId.value()
                    ));
                };
                cachedRoad();
                const auto firstBuilds = groundCache.buildCount();
                PALADIN_CHECK(map.objectState().placeCompletedObject(
                    map.grid(),
                    road,
                    {{10, 10}, 1, 1}
                ));
                cachedRoad();
                PALADIN_CHECK(groundCache.buildCount() == firstBuilds);
                PALADIN_CHECK(map.objectState().placeCompletedObject(
                    map.grid(),
                    road,
                    {{40, 43}, 1, 1}
                ));
                cachedRoad();
                PALADIN_CHECK(groundCache.buildCount() == firstBuilds + 1);
                camera.setPosition(4096, 4096);
                app.renderer_->beginFrame();
                city.render(
                    *app.renderer_,
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
                PALADIN_CHECK(city.submittedItems() == 0);
                return;
            }
            // Permit targeted UI verification while artist-owned exports are
            // changing; the default run still checks placeholder rendering.
            if (!SDL_getenv("PALADIN_SMOKE_UI_ONLY"))
            {
                presentationChecks(app);
            }
            else
            {
                std::cout << "UI-only check: placeholder rendering excluded.\n";
            }
            const float width = float(app.renderer_->outputWidth());
            const float height = float(app.renderer_->outputHeight());
            frame(app);
            PALADIN_CHECK(click(app, width / 2, height / 2)); // Tutorial.
            PALADIN_CHECK(app.screen_ == Application::Screen::MainMenu);
            PALADIN_CHECK(click(app, width / 2, height / 2 - 60)); // Play.
            PALADIN_CHECK(app.screen_ == Application::Screen::World);
            key(app, SDL_SCANCODE_F8);
            PALADIN_CHECK(!SceneSpriteLibrary::environmentArtEnabled());
            key(app, SDL_SCANCODE_F8);
            PALADIN_CHECK(SceneSpriteLibrary::environmentArtEnabled());
            PALADIN_CHECK(!app.simulationControlsVisible());

            // Keep this routing scenario deterministic and small; generation
            // and simulation mechanics have their own existing tests.
            WorldGenerationSettings settings;
            settings.width = settings.height = 64;
            settings.seed = 701;
            app.simulation_ = std::make_unique<Simulation>(settings);
            auto& grid = app.simulation_->world().grid();
            for (int y = 0; y < grid.height(); ++y)
            {
                for (int x = 0; x < grid.width(); ++x)
                {
                    grid.tile({x, y})->terrain = TerrainType::Land;
                    grid.tile({x, y})->elevation = Elevation{.5F};
                }
            }
            app.camera_->setPosition(32, 32);
            frame(app);
            PALADIN_CHECK(click(app, width - 70, height - 22));
            PALADIN_CHECK(app.screen_ == Application::Screen::World);
            PALADIN_CHECK(click(app, width / 2, 38)); // Select Region.
            PALADIN_CHECK(app.settlementPlacementController_->isSelecting());
            PALADIN_CHECK(click(app, width / 2, height / 2));
            PALADIN_CHECK(app.foundingPanel_->isOpen());
            const double beforeModalZoom = app.camera_->zoom();
            key(app, SDL_SCANCODE_EQUALS);
            PALADIN_CHECK(app.camera_->zoom() == beforeModalZoom);
            key(app, SDL_SCANCODE_ESCAPE);
            PALADIN_CHECK(!app.foundingPanel_->isOpen());

            const auto capital = app.simulation_->foundPlayerCapital(
                {32, 32},
                {"Smoke Realm", "Smoke Culture", "Smoke City", {}, "tribal", {}}
            );
            PALADIN_CHECK(capital);
            const auto treasury = app.simulation_->world()
                                      .realm(app.simulation_->playerRealmId())
                                      ->treasury;
            app.executeConsoleCommand("money 20");
            PALADIN_CHECK(treasury->balance == 2000);
            app.executeConsoleCommand("money");
            PALADIN_CHECK(treasury->balance == 2000);
            app.executeConsoleCommand("money -2000");
            PALADIN_CHECK(treasury->balance == -200000);
            app.executeConsoleCommand("money 0");
            PALADIN_CHECK(treasury->balance == 0);
            PALADIN_CHECK(treasury->usesMoney());
            app.foundingPanel_->openForCapitalRename("Renamed City");
            key(app, SDL_SCANCODE_RETURN);
            PALADIN_CHECK(!app.foundingPanel_->isOpen());
            PALADIN_CHECK(
                app.simulation_->world().settlement(capital)->name() ==
                "Renamed City"
            );
            SettlementMapGenerationSettings local;
            local.localTilesPerWorldTile = 8;
            PALADIN_CHECK(
                app.simulation_->prepareSettlementMap(capital, local)
            );
            frame(app);
            PALADIN_CHECK(click(app, width - 70, height - 22)); // World Play.
            PALADIN_CHECK(app.screen_ == Application::Screen::City);
            PALADIN_CHECK(app.simulationControlsVisible());
            PALADIN_CHECK(
                app.simulation_->detailedSimulationSettlementId() == capital
            );
            frame(app);

            const auto roofCameraX = app.camera_->tileX();
            app.settlementInspectionController_->selectCitizen(CitizenId{1});
            PALADIN_CHECK(app.cityRenderer_->presentation.roofsVisible);
            key(app, SDL_SCANCODE_O);
            PALADIN_CHECK(!app.cityRenderer_->presentation.roofsVisible);
            PALADIN_CHECK(app.camera_->tileX() == roofCameraX);
            PALADIN_CHECK(
                app.settlementInspectionController_->kind() ==
                SettlementInspectionKind::Citizen
            );
            key(app, SDL_SCANCODE_O);
            PALADIN_CHECK(app.cityRenderer_->presentation.roofsVisible);
            app.settlementInspectionController_->clear();
            frame(app);
            // Roof button sits in the existing spare information cell.
            app.settlementInspectionController_->selectCitizen(CitizenId{1});
            PALADIN_CHECK(app.cityHud_->roofControlAt(180, 118));
            PALADIN_CHECK(click(app, 180, 118));
            PALADIN_CHECK(!app.cityRenderer_->presentation.roofsVisible);
            PALADIN_CHECK(
                app.settlementInspectionController_->kind() ==
                SettlementInspectionKind::Citizen
            );
            PALADIN_CHECK(click(app, 180, 118));
            PALADIN_CHECK(app.cityRenderer_->presentation.roofsVisible);
            app.settlementInspectionController_->clear();
            capture(app, "presentation-hud.bmp");
            PALADIN_CHECK(click(app, 260, 118)); // Art F8 beside roof control.
            PALADIN_CHECK(!SceneSpriteLibrary::environmentArtEnabled());
            key(app, SDL_SCANCODE_F8);
            PALADIN_CHECK(SceneSpriteLibrary::environmentArtEnabled());
            auto& placement = *app.settlementObjectPlacementController_;
            auto* map = app.simulation_->settlementMap(capital);
            PALADIN_CHECK(
                placement.beginPlacement(SettlementObjectTypes::CityKeep)
            );
            placement.pointerMoved(SettlementTilePosition{20, 20});
            const auto original = placement.visibleFootprint();
            PALADIN_CHECK(original);
            key(app, SDL_SCANCODE_F);
            PALADIN_CHECK(
                placement.visibleFootprint()->width == original->height
            );
            PALADIN_CHECK(
                placement.visibleFootprint()->height == original->width
            );

            const float side = SimulationSpeedControls::ButtonSide;
            const auto speedButton = [&](int offset)
            { return click(app, width - side * offset + side / 2, side / 2); };
            PALADIN_CHECK(speedButton(1)); // Pause, without placing a keep.
            PALADIN_CHECK(app.simulationClock_->isPaused());
            PALADIN_CHECK(map->objectState().completedObjects().empty());
            PALADIN_CHECK(!placement.hasLockedFootprint());
            PALADIN_CHECK(speedButton(4));
            PALADIN_CHECK(app.simulationClock_->speedMultiplier() == 1);
            PALADIN_CHECK(!app.simulationClock_->isPaused());
            PALADIN_CHECK(speedButton(3));
            PALADIN_CHECK(app.simulationClock_->speedMultiplier() == 2);
            PALADIN_CHECK(speedButton(2));
            PALADIN_CHECK(app.simulationClock_->speedMultiplier() == 3);
            PALADIN_CHECK(speedButton(2));
            PALADIN_CHECK(app.simulationClock_->speedMultiplier() == 5);
            placement.cancelPlacement();
            app.simulationClock_->setPaused(true);
            const auto keepDefinition = *SettlementObjectCatalog::definition(
                SettlementObjectTypes::CityKeep
            );
            PALADIN_CHECK(map->objectState().placeCompletedObject(
                map->grid(),
                keepDefinition,
                {{2, 2}, 3, 7}
            ));
            map->logistics.synchronize(map->objectState(), 360);
            auto& people = app.simulation_->world()
                               .settlement(capital)
                               ->simulationState()
                               .citizens();
            SettlementActivityTestFixture::setNeeds(people, 90, 50);
            app.simulation_->reports.update(
                app.simulation_->world(),
                app.simulation_->playerRealmId(),
                true
            );
            PALADIN_CHECK(app.simulation_->reports.city(capital)->warnings.at(
                "starvation"
            ));
            PALADIN_CHECK(
                app.simulation_->reports.city(capital)->warnings.at("health")
            );
            const auto warningCount = app.simulation_->reports.events().size();
            app.simulation_->reports.update(
                app.simulation_->world(),
                app.simulation_->playerRealmId(),
                true
            );
            PALADIN_CHECK(
                app.simulation_->reports.events().size() == warningCount
            );

            // Reports and ledger remain presentation-only and capture input.
            PALADIN_CHECK(app.handleReportAction(CityHudAction::Ledger));
            PALADIN_CHECK(app.ledgerPanel_->isOpen());
            frame(app);
            auto& ledger = *app.ledgerPanel_;
            PALADIN_CHECK(
                ledger.rows_.size() == app.simulation_->world()
                                           .settlement(capital)
                                           ->simulationState()
                                           .citizens()
                                           .citizens()
                                           .size()
            );
            capture(app, "ledger-city.bmp");
            const auto ageHeader = ledger.columns_[1];
            PALADIN_CHECK(click(app, ageHeader.x + 8, ageHeader.y + 8));
            for (std::size_t i = 1; i < ledger.rows_.size(); ++i)
            {
                PALADIN_CHECK(
                    ledger.rows_[i - 1].cells[1].number <=
                    ledger.rows_[i].cells[1].number
                );
            }
            for (int page = 1; page < 4; ++page)
            {
                const auto tab = ledger.tabs_[page];
                PALADIN_CHECK(click(app, tab.x + 8, tab.y + 8));
                frame(app);
                PALADIN_CHECK(ledger.page_ == page);
                if (page == 2)
                {
                    capture(app, "ledger-graphs.bmp");
                }
            }
            PALADIN_CHECK(speedButton(1));
            PALADIN_CHECK(app.ledgerPanel_->isOpen());
            key(app, SDL_SCANCODE_ESCAPE);
            PALADIN_CHECK(!app.ledgerPanel_->isOpen());
            PALADIN_CHECK(app.screen_ == Application::Screen::City);
            PALADIN_CHECK(app.handleReportAction(CityHudAction::Events));
            frame(app);
            PALADIN_CHECK(app.ledgerPanel_->isOpen());
            capture(app, "city-events.bmp");
            SettlementActivityTestFixture::setNeeds(people, 0, 100);
            app.simulation_->reports.update(
                app.simulation_->world(),
                app.simulation_->playerRealmId(),
                true
            );
            PALADIN_CHECK(!app.simulation_->reports.city(capital)->warnings.at(
                "starvation"
            ));
            PALADIN_CHECK(
                !app.simulation_->reports.city(capital)->warnings.at("health")
            );
            key(app, SDL_SCANCODE_ESCAPE);

            app.employmentPanel_->toggle("Employment");
            app.employmentPanel_->close();
            // Population UI: choosing is not admitting; admission updates the
            // authoritative count and inherits attributes, not identities/jobs.
            auto stockedStore = *SettlementObjectCatalog::definition(
                SettlementObjectTypes::Stockpile
            );
            stockedStore.bypassesConstruction =
                true; // Established-city fixture.
            PALADIN_CHECK(map->objectState().placeCompletedObject(
                map->grid(),
                stockedStore,
                {{32, 32}, 4, 4}
            ));
            map->logistics.synchronize(map->objectState(), 360);
            for (const auto& inventory : map->logistics.inventories())
            {
                if (inventory.kind == InventoryKind::Stockpile)
                {
                    PALADIN_CHECK(
                        map->logistics.add(inventory.id, "fish", 100)
                    );
                    break;
                }
            }
            map->immigration.advance(*map, people, 360, 1440);
            PALADIN_CHECK(map->immigration.available() > 0);
            auto& populationPanel = *app.employmentPanel_;
            populationPanel.toggle("Population");
            frame(app);
            const auto clickPopulationAction =
                [&](std::string_view type, int delta = 0)
            {
                for (const auto& hit : populationPanel.hits_)
                {
                    if (hit.type == type && hit.delta == delta)
                    {
                        const auto bounds = hit.bounds;
                        return click(
                            app,
                            bounds.x + bounds.width * .5F,
                            bounds.y + bounds.height * .5F
                        );
                    }
                }
                return false;
            };
            PALADIN_CHECK(!clickPopulationAction("immigration"));
            const auto beforeAdmission = people.citizens().size();
            const auto expectedAttributes = people.averageAttributes();
            PALADIN_CHECK(clickPopulationAction("migrate", 1));
            PALADIN_CHECK(people.citizens().size() == beforeAdmission);
            frame(app);
            capture(app, "population-immigration.bmp");
            PALADIN_CHECK(clickPopulationAction("admit"));
            frame(app);
            PALADIN_CHECK(people.citizens().size() == beforeAdmission + 1);
            PALADIN_CHECK(
                app.simulation_->world()
                    .settlement(capital)
                    ->simulationState()
                    .population()
                    .residents() == people.citizens().size()
            );
            const auto& immigrant = people.citizens().back();
            PALADIN_CHECK(!immigrant.child && !immigrant.workplaceId);
            for (std::size_t a = 0; a < entityAttributeCount; ++a)
            {
                PALADIN_CHECK(
                    std::abs(
                        immigrant.value(EntityAttribute(a)) -
                        expectedAttributes.value(EntityAttribute(a))
                    ) < 1e-8
                );
            }
            for (std::size_t i = 0; i < populationPanel.attributeBounds_.size();
                 ++i)
            {
                const auto& row = populationPanel.attributeBounds_[i];
                PALADIN_CHECK(
                    populationPanel.tooltipAt(row.x + 2, row.y + 2)
                        .find(i == 0 ? "Happiness" : "1.00") !=
                    std::string::npos
                );
            }
            const auto attributeRow = populationPanel.attributeBounds_[0];
            const auto attributeText = populationPanel.tooltipAt(
                attributeRow.x + 2,
                attributeRow.y + 2
            );
            const auto attributeKey = populationPanel.tooltipKeyAt(
                attributeRow.x + 2,
                attributeRow.y + 2
            );
            app.tooltip_.render(
                *app.renderer_,
                *app.grayUiRenderer_,
                attributeText,
                attributeKey,
                attributeRow.x + 2,
                attributeRow.y + 2
            );
            SDL_Delay(360);
            app.tooltip_.render(
                *app.renderer_,
                *app.grayUiRenderer_,
                attributeText,
                attributeKey,
                attributeRow.x + 2,
                attributeRow.y + 2
            );
            capture(app, "population-modifiers.bmp");
            populationPanel.close();
            SettlementActivityTestFixture::marryFirstTwo(people);
            app.settlementInspectionController_->selectCitizen(
                people.citizens()[0].id
            );
            frame(app);
            const auto spouseBounds =
                app.settlementInspectionPanel_->renderedBounds_;
            PALADIN_CHECK(
                click(app, spouseBounds.x + 60, spouseBounds.y + 246)
            );
            PALADIN_CHECK(
                app.settlementInspectionController_->selectedCitizen(people)
                    ->id == people.citizens()[0].id
            );
            PALADIN_CHECK(click(
                app,
                spouseBounds.x + 31 +
                    BitmapFontRenderer{}.measureWidth("Married: ", 1.7F),
                spouseBounds.y + 246
            ));
            PALADIN_CHECK(
                app.settlementInspectionController_->selectedCitizen(people)
                    ->id == people.citizens()[1].id
            );
            PALADIN_CHECK(
                app.camera_->tileX() == 15.5 && app.camera_->tileY() == 10.5
            );
            app.settlementInspectionController_->clear();
            app.settlementInspectionPanel_->clearLayout();
            app.employmentPanel_->toggle("Employment");
            frame(app);
            key(app, SDL_SCANCODE_ESCAPE);
            PALADIN_CHECK(!app.employmentPanel_->isOpen());
            PALADIN_CHECK(app.screen_ == Application::Screen::City);
            key(app, SDL_SCANCODE_GRAVE);
            PALADIN_CHECK(app.debugConsole_->wantsText());
            const double beforeConsoleZoom = app.camera_->zoom();
            key(app, SDL_SCANCODE_EQUALS);
            PALADIN_CHECK(app.camera_->zoom() == beforeConsoleZoom);
            key(app, SDL_SCANCODE_GRAVE);
            PALADIN_CHECK(!app.debugConsole_->wantsText());

            const auto beforeTicks = app.simulation_->tickCount();
            app.simulationClock_->reset();
            app.simulationClock_->setPaused(false);
            app.simulationClock_->beginFrame();
            SDL_Delay(70);
            frame(app);
            PALADIN_CHECK(app.simulation_->tickCount() > beforeTicks);
            key(app, SDL_SCANCODE_ESCAPE);
            PALADIN_CHECK(app.screen_ == Application::Screen::World);
            PALADIN_CHECK(app.simulationControlsVisible());
            PALADIN_CHECK(!app.simulation_->detailedSimulationSettlementId());
            frame(app);
            PALADIN_CHECK(speedButton(1));
            PALADIN_CHECK(app.simulationClock_->isPaused());
            PALADIN_CHECK(app.handleReportAction(CityHudAction::Ledger));
            frame(app);
            PALADIN_CHECK(app.ledgerPanel_->isOpen());
            PALADIN_CHECK(app.ledgerPanel_->world_);
            PALADIN_CHECK(!app.ledgerPanel_->rows_.empty());
            capture(app, "ledger-world.bmp");
            key(app, SDL_SCANCODE_ESCAPE);
            const auto pausedTicks = app.simulation_->tickCount();
            SDL_Delay(70);
            frame(app);
            PALADIN_CHECK(app.simulation_->tickCount() == pausedTicks);
            PALADIN_CHECK(click(app, 70, height - 22)); // World Back.
            PALADIN_CHECK(app.screen_ == Application::Screen::MainMenu);
            PALADIN_CHECK(!app.simulation_);
            frame(app);
            PALADIN_CHECK(!click(app, width / 2, height / 2 + 60)); // Exit.

            SDL_Event quit{};
            quit.type = SDL_EVENT_QUIT;
            PALADIN_CHECK(SDL_PushEvent(&quit));
            PALADIN_CHECK(app.run() == 0);
        }
    };
} // namespace Paladin

int main()
{
#if defined(_MSC_VER) && defined(_DEBUG)
    // Failed automated checks belong in the test log, not a blocking dialog.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    try
    {
        Paladin::ApplicationSmokeTest::run();
        std::cout << "Application routing smoke test passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
