#include "simulation/Simulation.h"

#include "simulation/WorldSimulationPipeline.h"

#include "world/World.h"
#include "world/generation/WorldGenerationSeed.h"
#include "world/settlements/SettlementMap.h"

#include <cmath>
#include <limits>
#include <memory>

namespace Paladin
{
    Simulation::Simulation()
        : Simulation(withRandomWorldSeed())
    {
    }


    Simulation::Simulation(
        const WorldGenerationSettings& generationSettings
    )
        : world_(std::make_unique<World>(generationSettings)),
          worldSimulationPipeline_(
              std::make_unique<WorldSimulationPipeline>()
          )
    {
        playerPolityId_ = world_->createPolity();
    }


    Simulation::~Simulation() = default;


    void Simulation::tick(
        double realDeltaSeconds
    )
    {
        const double multiplier =
            speedMultiplier();

        if (multiplier <= 0.0)
        {
            return;
        }

        ++tickCount_;

        constexpr double realSecondsPerGameMinute = 60.0;

        // Preserve the existing real-time pace while committing only whole
        // authoritative world minutes.
        const double gameDeltaMinutes =
            realDeltaSeconds * multiplier /
            realSecondsPerGameMinute;

        if (
            !std::isfinite(gameDeltaMinutes) ||
            gameDeltaMinutes <= 0.0
        )
        {
            return;
        }

        pendingGameMinutes_ += gameDeltaMinutes;

        const double wholeMinutes = std::floor(pendingGameMinutes_);

        if (wholeMinutes < 1.0)
        {
            return;
        }

        const double maximumMinutes = static_cast<double>(
            std::numeric_limits<std::uint64_t>::max()
        );

        const std::uint64_t gameMinutes =
            wholeMinutes >= maximumMinutes
                ? std::numeric_limits<std::uint64_t>::max()
                : static_cast<std::uint64_t>(wholeMinutes);

        pendingGameMinutes_ -= static_cast<double>(gameMinutes);

        world_->advanceTime(gameMinutes);
        worldSimulationPipeline_->tick(
            *world_,
            gameMinutes
        );
    }


    void Simulation::setSpeed(
        SimulationSpeed speed
    ) noexcept
    {
        speed_ = speed;
    }


    SimulationSpeed Simulation::speed() const noexcept
    {
        return speed_;
    }


    bool Simulation::isPaused() const noexcept
    {
        return speed_ ==
            SimulationSpeed::Paused;
    }


    World& Simulation::world() noexcept
    {
        return *world_;
    }


    const World& Simulation::world() const noexcept
    {
        return *world_;
    }


    std::uint64_t Simulation::tickCount() const noexcept
    {
        return tickCount_;
    }


    PolityId Simulation::playerPolityId() const noexcept
    {
        return playerPolityId_;
    }


    SettlementId Simulation::presentedSettlementId() const noexcept
    {
        return presentedSettlementId_;
    }


    SettlementId
    Simulation::detailedSimulationSettlementId() const noexcept
    {
        return detailedSimulationSettlementId_;
    }


    bool Simulation::setPresentedSettlement(
        SettlementId settlementId
    ) noexcept
    {
        const Settlement* settlement =
            world_->settlement(settlementId);

        if (
            !settlement ||
            settlement->ownerPolityId() != playerPolityId_
        )
        {
            return false;
        }

        presentedSettlementId_ = settlementId;
        return true;
    }


    bool Simulation::setDetailedSimulationSettlement(
        SettlementId settlementId
    )
    {
        const Settlement* settlement =
            world_->settlement(settlementId);

        if (
            !settlement ||
            settlement->ownerPolityId() != playerPolityId_
        )
        {
            return false;
        }

        const SettlementId previousSettlementId =
            detailedSimulationSettlementId_;

        detailedSimulationSettlementId_ = settlementId;

        if (synchronizeSettlementSimulationTiers())
        {
            return true;
        }

        detailedSimulationSettlementId_ = previousSettlementId;
        static_cast<void>(synchronizeSettlementSimulationTiers());
        return false;
    }


    bool Simulation::clearDetailedSimulationSettlement()
    {
        const SettlementId previousSettlementId =
            detailedSimulationSettlementId_;

        detailedSimulationSettlementId_ = {};

        if (synchronizeSettlementSimulationTiers())
        {
            return true;
        }

        detailedSimulationSettlementId_ = previousSettlementId;
        static_cast<void>(synchronizeSettlementSimulationTiers());
        return false;
    }


    bool Simulation::prepareSettlementMap(
        SettlementId settlementId,
        const SettlementMapGenerationSettings& settings
    )
    {
        Settlement* settlement = world_->settlement(settlementId);

        if (
            !settlement ||
            settlement->ownerPolityId() != playerPolityId_ ||
            !settlement->simulationState().isInitialized()
        )
        {
            return false;
        }

        SettlementSimulationState& state =
            settlement->simulationState();

        const TerritoryFoundationPolicy& territoryPolicy =
            world_->territoryFoundationPolicy();

        const SettlementMap* existingMap = state.localMap();

        if (
            existingMap &&
            existingMap->sourceRegionCenter() == settlement->position() &&
            existingMap->sourceRegionWidth() ==
                territoryPolicy.settlementRegionWidth &&
            existingMap->sourceRegionHeight() ==
                territoryPolicy.settlementRegionHeight &&
            existingMap->localTilesPerWorldTile() ==
                settings.localTilesPerWorldTile
        )
        {
            return true;
        }

        std::unique_ptr<SettlementMap> generatedMap =
            settlementMapGenerator_.generate(
                world_->grid(),
                settlement->position(),
                territoryPolicy.settlementRegionWidth,
                territoryPolicy.settlementRegionHeight,
                world_->generationSeed(),
                settings
            );

        if (!generatedMap)
        {
            return false;
        }

        state.setLocalMap(std::move(generatedMap));
        return true;
    }


    SettlementMap* Simulation::settlementMap(
        SettlementId settlementId
    ) noexcept
    {
        Settlement* settlement = world_->settlement(settlementId);

        return settlement
            ? settlement->simulationState().localMap_.get()
            : nullptr;
    }


    const SettlementMap* Simulation::settlementMap(
        SettlementId settlementId
    ) const noexcept
    {
        const Settlement* settlement = world_->settlement(settlementId);

        return settlement
            ? settlement->simulationState().localMap()
            : nullptr;
    }


    SettlementId Simulation::foundPlayerCapital(
        WorldPosition position,
        const FoundingIdentity& identity
    )
    {
        const SettlementId settlementId =
            world_->foundCapitalSettlement(
            position,
            playerPolityId_,
            identity
        );

        if (settlementId.isValid())
        {
            static_cast<void>(
                setPresentedSettlement(settlementId)
            );

        }

        return settlementId;
    }


    bool Simulation::renamePlayerCapital(std::string name)
    {
        const Polity* polity = world_->polity(playerPolityId_);

        return
            polity &&
            world_->renameSettlement(
                polity->capitalSettlementId(),
                std::move(name)
            );
    }


    bool Simulation::editPlayerPolity(
        const FoundingIdentity& identity
    )
    {
        return world_->editPolityIdentity(playerPolityId_, identity);
    }


    bool Simulation::movePlayerCapital(WorldPosition position)
    {
        const Polity* polity = world_->polity(playerPolityId_);

        if (!polity)
        {
            return false;
        }

        Settlement* capital = world_->settlement(
            polity->capitalSettlementId()
        );

        if (
            !capital ||
            !world_->relocateSoleCapital(playerPolityId_, position)
        )
        {
            return false;
        }

        capital->simulationState().clearLocalMap();
        return true;
    }


    bool Simulation::synchronizeSettlementSimulationTiers()
    {
        for (Settlement& settlement : world_->settlements())
        {
            if (!settlement.simulationState().isInitialized())
            {
                continue;
            }

            SettlementSimulationTier tier =
                SettlementSimulationTier::Strategic;

            if (
                settlement.id() ==
                detailedSimulationSettlementId_
            )
            {
                tier = SettlementSimulationTier::Detailed;
            }
            else if (
                settlement.ownerPolityId() == playerPolityId_
            )
            {
                tier = SettlementSimulationTier::Inactive;
            }

            if (
                !worldSimulationPipeline_->transitionSettlementTier(
                    *world_,
                    settlement.id(),
                    tier
                )
            )
            {
                return false;
            }
        }

        return true;
    }


    double Simulation::speedMultiplier() const noexcept
    {
        switch (speed_)
        {
            case SimulationSpeed::Paused:
                return 0.0;

            case SimulationSpeed::Normal:
                return 1.0;

            case SimulationSpeed::Fast:
                return 2.0;

            case SimulationSpeed::VeryFast:
                return 3.0;
        }

        return 1.0;
    }
}
