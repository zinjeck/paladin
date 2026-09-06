#include "TestFramework.h"
#include "core/Application.h"
#include "core/SimulationClock.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "interaction/SettlementPlacementController.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/CityRenderer.h"
#include "rendering/Renderer.h"
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

namespace Paladin
{
    struct SettlementActivityTestFixture
    {
        static void setRenderPerson(
            SettlementCitizenState& state,
            SettlementTilePosition tile
        )
        {
            state.citizens_.resize(1);
            auto& c = state.citizens_[0];
            c.id = CitizenId{1};
            c.tilePosition = c.destination = tile;
            c.path.clear();
            c.hasVisualSnapshot = false;
            c.child = false;
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
            auto& renderer = *app.renderer_;
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
            placeholders.load(
                renderer,
                std::string(SDL_GetBasePath()) + "assets/sprites"
            );
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
            std::ofstream(directory / "sprites.catalog")
                << "citizen alpha.png 2 1 0.5 1 0 0\ninvalid missing.png 1 1 "
                   ".5 1 0 0\n";
            std::ofstream(directory / "objects.catalog")
                << "house single - - - citizen 3 3 1 .12 0 0 0 0 0 dbd6c2 "
                   "524d3d .6 .45\n"
                   "invalid modules - - - - 0 1 1 .1 0 0 0 0 0 ffffff 555555 "
                   ".6 .45\n";
            SceneSpriteLibrary library;
            library.load(renderer, directory.string());
            PALADIN_CHECK(library.find("citizen"));
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
        }

        static void run()
        {
            Application app;
            PALADIN_CHECK(app.renderer_ && app.renderer_->isValid());
            presentationChecks(app);
            const float width = float(app.renderer_->outputWidth());
            const float height = float(app.renderer_->outputHeight());
            frame(app);
            PALADIN_CHECK(click(app, width / 2, height / 2)); // Tutorial.
            PALADIN_CHECK(app.screen_ == Application::Screen::MainMenu);
            PALADIN_CHECK(click(app, width / 2, height / 2 - 60)); // Play.
            PALADIN_CHECK(app.screen_ == Application::Screen::World);
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
