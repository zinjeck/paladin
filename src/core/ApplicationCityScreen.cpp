#include "rendering/WorldMapNavigation.h"
#include "core/Application.h"
#include "core/SimulationClock.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "rendering/CityRenderer.h"
#include "rendering/Renderer.h"
#include "simulation/Simulation.h"
#include "simulation/CitizenshipSystem.h"
#include "ui/CityHud.h"
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
#include "ui/GrayUiRenderer.h"
#include "ui/SettlementInspectionPanel.h"
#include "ui/SimulationSpeedControls.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <SDL3/SDL.h>
#include <array>

namespace Paladin
{
    void Application::layoutCityScreen()
    {
        const auto* settlement =
            simulation_->world().settlement(activeCitySettlementId_);
        cityHud_->setFortress(settlement && settlement->isFortress());
        cityHud_->setResourcesVisible(cityRenderer_->resourcesVisible);
        cityHud_->setRoofsVisible(cityRenderer_->presentation.roofsVisible);
        cityHud_->layout(renderer_->outputWidth(), renderer_->outputHeight());
    }

    void Application::synchronizeCityStatus()
    {
        auto* currentMap = simulation_->settlementMap(activeCitySettlementId_);
        auto* currentSettlement =
            simulation_->world().settlement(activeCitySettlementId_);
        if (currentMap && currentSettlement)
        {
            auto& citizens = currentSettlement->simulationState().citizens();
            currentMap->employment().synchronize(
                currentMap->objectState(),
                citizens
            );
            currentMap->employment().record(
                simulation_->world().time().totalGameMinutes(),
                citizens
            );
            cityHud_->setSettlementStatus(
                currentMap->logistics.founded(),
                citizens.residentCount()
            );
        }
    }

    void Application::updateCityScreen()
    {
        if (!settlementInspectionPanel_->editingName())
        {
            updateCameraMovement(simulationClock_->frameDeltaSeconds());
            updateCameraZoom(simulationClock_->frameDeltaSeconds());
        }

        if (settlementObjectPlacementController_->isActive())
        {
            float mouseX = 0.0F;
            float mouseY = 0.0F;
            SDL_GetMouseState(&mouseX, &mouseY);

            settlementObjectPlacementController_->pointerMoved(
                cityHud_->containsInteractivePoint(mouseX, mouseY)
                    ? std::nullopt
                    : cityTileAtScreen(mouseX, mouseY)
            );
        }

        if (settlementCommandController_->isActive())
        {
            float mouseX = 0.0F;
            float mouseY = 0.0F;
            SDL_GetMouseState(&mouseX, &mouseY);
            settlementCommandController_->pointerMoved(
                cityHud_->containsInteractivePoint(mouseX, mouseY)
                    ? std::nullopt
                    : cityTileAtScreen(mouseX, mouseY)
            );
        }
    }

    void Application::updateCityHud()
    {
        auto* settlementMap =
            simulation_->settlementMap(activeCitySettlementId_);
        const Settlement* citySettlement =
            simulation_->world().settlement(activeCitySettlementId_);

        const WorldTime& worldTime = simulation_->world().time();
        const auto* hudRealm =
            simulation_->world().realm(simulation_->playerRealmId());
        cityHud_->setRealmFlag(hudRealm ? hudRealm->flag() : RealmFlag{});

        const double localMinute =
            citySettlement ? PlanetAstronomy::localMinute(
                                 worldTime.totalGameMinutes(),
                                 (citySettlement->position().x + .5) /
                                     simulation_->world().grid().width()
                             )
                           : worldTime.hour() * 60 + worldTime.minute();
        cityHud_->setCityInformation(
            citySettlement ? std::string(citySettlement->name())
                           : std::string(),
            worldTime.day(),
            int(localMinute) / 60,
            int(localMinute) % 60
        );

        cityHud_->setHousingCapacity(
            settlementMap ? settlementMap->activities.housingCapacity() : 0
        );
        cityHud_->clearGoods();
        if (settlementMap && citySettlement)
        {
            const auto& report = settlementMap->commerce.dailyResourceReport(
                *settlementMap, citySettlement->simulationState().citizens(),
                double(worldTime.totalGameMinutes()));
            for (const auto& resource : SettlementResourceCatalog::definitions())
            {
                double amount = 0;
                for (const auto& inventory : settlementMap->logistics.inventories())
                {
                    if (!countsAsCityStorage(inventory.kind)) { continue; }
                    for (const auto& goods : inventory.goods)
                    {
                        if (goods.resource == resource.id) { amount += goods.amount; }
                    }
                }
                const auto found = report.find(std::string(resource.id));
                cityHud_->setGoodsResource(resource.id, amount,
                    found != report.end() ? found->second : ResourceDailyRates{});
            }
        }
        if (const auto* realm =
                simulation_->world().realm(simulation_->playerRealmId()))
        {
            cityHud_->setTreasuryGold(realm->treasury->balance);
        }
    }

    void Application::renderCityScreen()
    {
        cityRenderer_->gameMinute = double(simulation_->world().time().totalGameMinutes());
        auto* settlementMap =
            simulation_->settlementMap(activeCitySettlementId_);


        if (settlementMap)
        {
            const Settlement* renderedSettlement =
                simulation_->world().settlement(activeCitySettlementId_);
            if (renderedSettlement)
            {
                cityRenderer_->animationSeconds =
                    simulationClock_->presentationSeconds();
                cityRenderer_->render(
                    *renderer_,
                    *settlementMap,
                    *camera_,
                    *tileRenderMetrics_,
                    *settlementObjectPlacementController_,
                    *settlementCommandController_,
                    renderedSettlement->simulationState().citizens(),
                    *settlementInspectionController_,
                    simulationClock_->interpolationAlpha(),
                    PlanetAstronomy::localMinute(
                        simulation_->world().time().totalGameMinutes(),
                        (renderedSettlement->position().x + .5) /
                            simulation_->world().grid().width()
                    ) / 60.,
                    globeSunDot(
                        (renderedSettlement->position().x + .5) /
                            simulation_->world().grid().width(),
                        (renderedSettlement->position().y + .5) /
                            simulation_->world().grid().height(),
                        simulation_->world().time().secondsIntoDay()
                    )
                );

            }
        }


        const auto* citySettlement =
            simulation_->world().settlement(activeCitySettlementId_);
        const auto& worldTime = simulation_->world().time();
        cityHud_->render(*renderer_, *grayUiRenderer_);
        if (settlementMap)
        {
            cityRenderer_->renderMinimap(
                *renderer_,
                *settlementMap,
                *camera_,
                *tileRenderMetrics_,
                cityHud_->minimapBounds()
            );
        }

        simulationSpeedControls_->render(*renderer_, *grayUiRenderer_);
        // The inspector and its embedded depot controls are one foreground
        // surface. Paint its background after the HUD and minimap so those
        // layers cannot show through the later-rendered trade controls.
        if (settlementMap && citySettlement)
        {
            settlementInspectionPanel_->render(
                *renderer_,
                *grayUiRenderer_,
                *settlementInspectionController_,
                *settlementMap,
                citySettlement->simulationState().citizens(),
                *camera_,
                *tileRenderMetrics_
            );
        }
        else
        {
            settlementInspectionPanel_->clearLayout();
        }
        if (const auto* realm =
                simulation_->world().realm(simulation_->playerRealmId()))
        {
            employmentPanel_->setRealmWorkDayHours(realm->workDayHours());
        }
        if (settlementMap && citySettlement)
        {
            if (employmentPanel_->showsPopulation())
                employmentPanel_->setImmigrationOriginAvailable(!CitizenshipSystem::nearbyOrigins(
                    simulation_->world(), activeCitySettlementId_).empty());
            employmentPanel_->render(
                *renderer_,
                *grayUiRenderer_,
                *settlementMap,
                citySettlement->simulationState().citizens(),
                worldTime.totalGameMinutes()
            );
        }
        renderCityTooltip();
    }

    void Application::renderCityTooltip()
    {
        float x = 0, y = 0;
        SDL_GetMouseState(&x, &y);
        std::string text, key;
        if (employmentPanel_->containsPoint(x, y))
        {
            text = employmentPanel_->tooltipAt(x, y);
            key = employmentPanel_->tooltipKeyAt(x, y);
        }
        else
        {
            text = cityHud_->tooltipAt(x, y);
        }
        if (screen_ == Screen::World && text.empty())
            text = WorldMapNavigation::modeTooltip(x,y,renderer_->outputWidth(),renderer_->outputHeight());
        if (key.empty())
        {
            key = text;
        }
        if (screen_ == Screen::City && text.empty() && !activeHudContainsPoint(x, y))
        {
            const auto* map =
                simulation_->settlementMap(activeCitySettlementId_);
            const auto* settlement =
                simulation_->world().settlement(activeCitySettlementId_);
            const auto tile = cityTileAtScreen(x, y);
            if (map && settlement && tile)
            {
                key = std::to_string(tile->x) + ":" + std::to_string(tile->y);
                const auto& citizens = settlement->simulationState().citizens();
                if (const auto* citizen = citizens.citizenAt(*tile))
                {
                    text = citizen->name + " - " +
                           SettlementActivitySystem::activityLabel(*citizen);
                    key = "citizen:" + std::to_string(citizen->id.value());
                }
                else if (
                    const auto* site =
                        map->objectState().constructionSiteAt(*tile)
                )
                {
                    if (const auto* d = SettlementObjectCatalog::definition(
                            site->objectTypeId
                        ))
                    {
                        text =
                            std::string(d->displayName) + " - construction " +
                            std::to_string(site->progressPermille / 10) + "%";
                    }
                }
                else if (
                    const auto* object =
                        map->objectState().completedObjectAt(*tile)
                )
                {
                    if (const auto* d = SettlementObjectCatalog::definition(
                            object->objectTypeId
                        ))
                    {
                        text = std::string(d->displayName);
                    }
                    if (const auto* job = map->employment().workplace(
                            map->employment().forObject(object->id)
                        ))
                    {
                        text = job->name + " - " +
                               std::to_string(
                                   map->employment().employed(job->id, citizens)
                               ) +
                               "/" + std::to_string(job->capacity) + " workers";
                    }
                }
                if (text.empty())
                {
                    for (const auto& inventory : map->logistics.inventories())
                    {
                        if (inventory.kind != InventoryKind::Groundpile ||
                            !inventory.footprint.contains(*tile))
                        {
                            continue;
                        }
                        for (const auto& goods : inventory.goods)
                        {
                            if (!text.empty())
                            {
                                text += ", ";
                            }
                            const auto* d =
                                SettlementResourceCatalog::definition(
                                    goods.resource
                                );
                            text += std::to_string(goods.amount) + " " +
                                    (d ? std::string(d->displayName)
                                       : goods.resource);
                        }
                    }
                }
                if (text.empty())
                {
                    if (const auto* ground = map->grid().tile(*tile);
                        ground && ground->mineral != MineralDeposit::None)
                    {
                        text = ground->mineral == MineralDeposit::Coal
                                   ? "Coal deposit"
                               : ground->mineral == MineralDeposit::Iron
                                   ? "Iron deposit"
                                   : "Rare gold deposit";
                    }
                    const auto feature = map->naturalFeatures().at(*tile).kind;
                    if (feature == NaturalFeatureKind::Tree)
                    {
                        text = "Tree - 4 lumber";
                    }
                    if (feature == NaturalFeatureKind::Rock)
                    {
                        text = "Rock - 4 stone";
                    }
                }
            }
        }
        tooltip_.render(*renderer_, *grayUiRenderer_, text, key, x, y);
    }

} // namespace Paladin
