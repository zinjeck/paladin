#include "world/settlements/objects/SettlementDoor.h"
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
        std::fill(
            movementBlockedTiles_.begin(),
            movementBlockedTiles_.end(),
            0
        );
        for (const auto& object : completedObjects_)
        {
            if (const auto* definition =
                    SettlementObjectCatalog::definition(object.objectTypeId))
            {
                occupy(*definition, object.footprint);
                // Outdoor workplaces remain walkable while keeping their
                // building footprint exclusive.
                if (definition->wallThickness > 0)
                {
                    const auto& f = object.footprint;
                    for (int y = f.topLeft.y; y < f.topLeft.y + f.height; ++y)
                    {
                        for (int x = f.topLeft.x; x < f.topLeft.x + f.width;
                             ++x)
                        {
                            const int band = definition->wallThickness;
                            const bool wall =
                                x < f.topLeft.x + band ||
                                y < f.topLeft.y + band ||
                                x >= f.topLeft.x + f.width - band ||
                                y >= f.topLeft.y + f.height - band;
                            movementBlockedTiles_[tileIndex({x, y})] =
                                wall &&
                                (!object.door ||
                                 !validDoorTile(f, *object.door) ||
                                 *object.door != SettlementTilePosition{x, y});
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
        synchronizeIdIndexes();
        const auto found = siteIndex_.find(id);
        if (found == siteIndex_.end())
        {
            return false;
        }
        auto& site = constructionSites_[found->second];
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
    SettlementObjectId SettlementObjectState::build(
        ConstructionSiteId id,
        double labor,
        double required,
        SettlementTilePosition workerTile
    )
    {
        synchronizeIdIndexes();
        const auto found = siteIndex_.find(id);
        if (found == siteIndex_.end())
        {
            return {};
        }
        auto& site = constructionSites_[found->second];
        if (!site.footprint.contains(workerTile) || labor <= 0 || required <= 0)
        {
            return {};
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
        completed.door = site.door;
        if (site.objectTypeId == SettlementObjectTypes::Road)
        {
            completed.footprint = {workerTile, 1, 1};
            // Complete exactly one tile; retain the other run tiles in place.
            // No full occupancy rebuild or copy of every outstanding site.
            site.laborMinutes = 0;
            site.progressPermille = 0;
            site.phase = ConstructionSitePhase::ReadyToBuild;
            const auto f = site.footprint;
            const int left = workerTile.x - f.topLeft.x;
            const int right = f.topLeft.x + f.width - workerTile.x - 1;
            if (left > 0 && right > 0)
            {
                auto remainder = site;
                remainder.id = constructionSiteIds_.generate();
                remainder
                    .footprint = {{workerTile.x + 1, workerTile.y}, right, 1};
                site.footprint.width = left;
                siteIndex_[remainder.id] = constructionSites_.size();
                constructionSites_.push_back(std::move(remainder));
            }
            else if (left > 0)
            {
                site.footprint.width = left;
            }
            else if (right > 0)
            {
                site.footprint = {{workerTile.x + 1, workerTile.y}, right, 1};
            }
            else
            {
                const auto index =
                    std::size_t(&site - constructionSites_.data());
                siteIndex_.erase(id);
                if (index + 1 < constructionSites_.size())
                {
                    siteIndex_[constructionSites_.back().id] = index;
                    constructionSites_[index] =
                        std::move(constructionSites_.back());
                }
                constructionSites_.pop_back();
            }
            const auto objectId = completed.id;
            objectIndex_[objectId] = completedObjects_.size();
            completedObjects_.push_back(std::move(completed));
            ++navigationVersion_;
            indexedVersion_ = navigationVersion_;
            ++presentationVersion_;
            return objectId;
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
