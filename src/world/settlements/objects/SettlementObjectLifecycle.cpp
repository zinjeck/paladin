#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include <algorithm>
#include <cmath>

namespace Paladin
{
void SettlementObjectState::rebuildOccupancy()
{
    std::fill(
        structureOccupiedTiles_.begin(),
        structureOccupiedTiles_.end(),
        0
    );
    std::fill(
        infrastructureOccupiedTiles_.begin(),
        infrastructureOccupiedTiles_.end(),
        0
    );
    std::fill(movementBlockedTiles_.begin(), movementBlockedTiles_.end(), 0);
    for (const auto& object : completedObjects_)
    {
        if (const auto* definition =
                SettlementObjectCatalog::definition(object.objectTypeId))
        {
            occupy(*definition, object.footprint);
            // Outdoor workplaces remain walkable while keeping their building
            // footprint exclusive.
            if (object.objectTypeId == SettlementObjectTypes::CityKeep ||
                object.objectTypeId == SettlementObjectTypes::House ||
                object.objectTypeId == SettlementObjectTypes::Bakery)
            {
                const auto& f = object.footprint;
                for (int y = f.topLeft.y; y < f.topLeft.y + f.height; ++y)
                {
                    for (int x = f.topLeft.x; x < f.topLeft.x + f.width; ++x)
                    {
                        movementBlockedTiles_[tileIndex({x, y})] = 1;
                    }
                }
            }
        }
    }
    for (const auto& site : constructionSites_)
    {
        if (const auto* definition =
                SettlementObjectCatalog::definition(site.objectTypeId))
        {
            occupy(*definition, site.footprint);
        }
    }
    ++navigationVersion_;
}
bool SettlementObjectState::deliverMaterials(
    ConstructionSiteId id,
    std::string_view resource,
    int amount
)
{
    for (auto& site : constructionSites_)
    {
        if (site.id != id)
        {
            continue;
        }
        for (auto& delivery : site.resourceDeliveries)
        {
            if (delivery.resourceId == resource)
            {
                delivery.deliveredAmount = std::min(
                    delivery.requiredAmount,
                    std::uint32_t(std::max(0, amount))
                );
            }
        }
        if (std::all_of(
                site.resourceDeliveries.begin(),
                site.resourceDeliveries.end(),
                [](const auto& d)
                { return d.deliveredAmount >= d.requiredAmount; }
            ))
        {
            site.phase = ConstructionSitePhase::ReadyToBuild;
        }
        ++presentationVersion_;
        return true;
    }
    return false;
}
SettlementObjectId SettlementObjectState::build(
    ConstructionSiteId id,
    double labor,
    double required,
    SettlementTilePosition workerTile
)
{
    for (auto& site : constructionSites_)
    {
        if (site.id != id || !site.footprint.contains(workerTile) ||
            labor <= 0 || required <= 0)
        {
            continue;
        }
        if (!std::all_of(
                site.resourceDeliveries.begin(),
                site.resourceDeliveries.end(),
                [](const auto& d)
                { return d.deliveredAmount >= d.requiredAmount; }
            ))
        {
            return {};
        }
        site.phase = ConstructionSitePhase::UnderConstruction;
        site.laborMinutes += labor;
        site.progressPermille = std::uint16_t(
            std::clamp(site.laborMinutes / required * 1000, 0.0, 1000.0)
        );
        ++presentationVersion_;
        if (site.laborMinutes + 1e-8 < required)
        {
            return {};
        }
        auto completed = CompletedSettlementObject{
            objectIds_.generate(),
            site.objectTypeId,
            site.footprint,
            site.productionWater
        };
        if (site.objectTypeId == SettlementObjectTypes::Road)
        {
            completed.footprint = {workerTile, 1, 1};
            // A road run is only a compact designation; each tile needs its own
            // labor.
            site.laborMinutes = 0;
            site.progressPermille = 0;
            cancelConstructionWithin(completed.footprint);
        }
        else
        {
            std::erase_if(
                constructionSites_,
                [id](const auto& s) { return s.id == id; }
            );
        }
        const auto objectId = completed.id;
        completedObjects_.push_back(std::move(completed));
        rebuildOccupancy();
        ++presentationVersion_;
        return objectId;
    }
    return {};
}
bool SettlementObjectState::demolish(
    SettlementObjectId id,
    SettlementTilePosition tile
)
{
    const auto* object = completedObject(id);
    if (!object)
    {
        return false;
    }
    const auto copy = *object;
    std::erase_if(
        completedObjects_,
        [id](const auto& o) { return o.id == id; }
    );
    // Finished road objects in normal gameplay are single tiles.
    if (copy.objectTypeId == SettlementObjectTypes::Road)
    {
        const auto& f = copy.footprint;
        for (int y = f.topLeft.y; y < f.topLeft.y + f.height; ++y)
        {
            for (int x = f.topLeft.x; x < f.topLeft.x + f.width; ++x)
            {
                if (SettlementTilePosition{x, y} != tile)
                {
                    completedObjects_.push_back(
                        {objectIds_.generate(),
                         copy.objectTypeId,
                         {{x, y}, 1, 1}}
                    );
                }
            }
        }
    }
    rebuildOccupancy();
    ++presentationVersion_;
    return true;
}
double SettlementObjectState::accrueProduction(
    SettlementObjectId id,
    double amount
)
{
    for (auto& object : completedObjects_)
    {
        if (object.id != id)
        {
            continue;
        }
        object.productionProgress =
            std::max(0.0, object.productionProgress + amount);
        const double whole = std::floor(object.productionProgress + 1e-9);
        object.productionProgress -= whole;
        return whole;
    }
    return 0;
}
} // namespace Paladin
