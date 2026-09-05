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
        : Simulation(withRandomWorldSeed(), defaultSimulationTimingSettings())
    {
    }


    Simulation::Simulation(
        const WorldGenerationSettings& generationSettings,
        SimulationTimingSettings timingSettings
    )
        : world_(std::make_unique<World>(generationSettings)),
          worldSimulationPipeline_(std::make_unique<WorldSimulationPipeline>()),
          timingSettings_(timingSettings)
    {
        if (timingSettings_.gameMinutesPerStep == 0 ||
            !std::isfinite(timingSettings_.realSecondsPerStep) ||
            timingSettings_.realSecondsPerStep <= 0.0)
        {
            timingSettings_ = defaultSimulationTimingSettings();
        }

        playerRealmId_ = world_->createRealm();
    }


    Simulation::~Simulation() = default;


    void Simulation::tick(double realDeltaSeconds)
    {
        const double multiplier = speedMultiplier();

        if (multiplier <= 0.0)
        {
            return;
        }

        ScopedTiming totalTimer{tickTiming};
        ++tickCount_;

        const double gameMinutesPerRealSecond =
            static_cast<double>(timingSettings_.gameMinutesPerStep) /
            timingSettings_.realSecondsPerStep;

        // Commit only whole authoritative world minutes while retaining the
        // fractional phase between fixed simulation updates.
        const double gameDeltaMinutes =
            realDeltaSeconds * multiplier * gameMinutesPerRealSecond;

        if (!std::isfinite(gameDeltaMinutes) || gameDeltaMinutes <= 0.0)
        {
            return;
        }

        {
            ScopedTiming citizenTimer{citizenTiming};
            for (auto& settlement : world_->settlements())
            {
                auto& state = settlement.simulationState();
                if (auto* map = settlementMap(settlement.id()))
                {
                    map->activities.tick(
                        *map,
                        state.citizens(),
                        world_->time().totalGameMinutes() + pendingGameMinutes_,
                        gameDeltaMinutes
                    );
                    if (map->logistics.founded())
                    {
                        state.synchronizeCitizenPopulation();
                    }
                }
            }
        }

        pendingGameMinutes_ += gameDeltaMinutes;

        const double wholeMinutes = std::floor(pendingGameMinutes_);

        if (wholeMinutes < 1.0)
        {
            return;
        }

        const double maximumMinutes =
            static_cast<double>(std::numeric_limits<std::uint64_t>::max());

        const std::uint64_t gameMinutes =
            wholeMinutes >= maximumMinutes
                ? std::numeric_limits<std::uint64_t>::max()
                : static_cast<std::uint64_t>(wholeMinutes);

        pendingGameMinutes_ -= static_cast<double>(gameMinutes);

        world_->advanceTime(gameMinutes);
        ScopedTiming aggregateTimer{aggregateTiming};
        worldSimulationPipeline_->tick(*world_, gameMinutes);
    }


    void Simulation::setSpeed(SimulationSpeed speed) noexcept
    {
        speed_ = speed;
    }


    SimulationSpeed Simulation::speed() const noexcept
    {
        return speed_;
    }


    bool Simulation::isPaused() const noexcept
    {
        return speed_ == SimulationSpeed::Paused;
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


    RealmId Simulation::playerRealmId() const noexcept
    {
        return playerRealmId_;
    }


    SettlementId Simulation::presentedSettlementId() const noexcept
    {
        return presentedSettlementId_;
    }


    SettlementId Simulation::detailedSimulationSettlementId() const noexcept
    {
        return detailedSimulationSettlementId_;
    }


    bool Simulation::setPresentedSettlement(SettlementId settlementId) noexcept
    {
        const Settlement* settlement = world_->settlement(settlementId);

        if (!settlement || settlement->ownerRealmId() != playerRealmId_)
        {
            return false;
        }

        presentedSettlementId_ = settlementId;
        return true;
    }


    bool Simulation::setDetailedSimulationSettlement(SettlementId settlementId)
    {
        const Settlement* settlement = world_->settlement(settlementId);

        if (!settlement || settlement->ownerRealmId() != playerRealmId_)
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

        if (!settlement || settlement->ownerRealmId() != playerRealmId_ ||
            !settlement->simulationState().isInitialized())
        {
            return false;
        }

        SettlementSimulationState& state = settlement->simulationState();

        const TerritoryFoundationPolicy& territoryPolicy =
            world_->territoryFoundationPolicy();

        const SettlementMap* existingMap = state.localMap();

        if (existingMap &&
            existingMap->sourceRegionCenter() == settlement->position() &&
            existingMap->sourceRegionWidth() ==
                territoryPolicy.settlementRegionWidth &&
            existingMap->sourceRegionHeight() ==
                territoryPolicy.settlementRegionHeight &&
            existingMap->localTilesPerWorldTile() ==
                settings.localTilesPerWorldTile)
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

        if (const auto* realm = world_->realm(settlement->ownerRealmId()))
        {
            generatedMap->activities.policy.setWorkDayHours(
                realm->workDayHours()
            );
        }
        state.setLocalMap(std::move(generatedMap));
        return true;
    }


    SettlementMap* Simulation::settlementMap(SettlementId settlementId) noexcept
    {
        Settlement* settlement = world_->settlement(settlementId);

        return settlement ? settlement->simulationState().localMap_.get()
                          : nullptr;
    }


    const SettlementMap* Simulation::settlementMap(
        SettlementId settlementId
    ) const noexcept
    {
        const Settlement* settlement = world_->settlement(settlementId);

        return settlement ? settlement->simulationState().localMap() : nullptr;
    }


    SettlementId Simulation::foundPlayerCapital(
        WorldTilePosition position,
        const FoundingIdentity& identity
    )
    {
        const SettlementId settlementId = world_->foundCapitalSettlement(
            position,
            playerRealmId_,
            identity,
            playerSettlementFoundationProfile(world_->generationSeed())
        );

        if (settlementId.isValid())
        {
            static_cast<void>(setPresentedSettlement(settlementId));
        }

        return settlementId;
    }


    SettlementId Simulation::foundPlayerSettlement(
        WorldTilePosition position,
        std::string name
    )
    {
        if (!isValidFoundingName(name) ||
            !world_->canFoundAdditionalSettlementAt(position, playerRealmId_))
        {
            return {};
        }
        const auto& policy = world_->territoryFoundationPolicy();
        auto map = settlementMapGenerator_.generate(
            world_->grid(),
            position,
            policy.settlementRegionWidth,
            policy.settlementRegionHeight,
            world_->generationSeed(),
            SettlementMapGenerationSettings{}
        );
        if (!map)
        {
            return {};
        }
        map->activities.policy.setWorkDayHours(
            world_->realm(playerRealmId_)->workDayHours()
        );
        auto profile = playerSettlementFoundationProfile(
            world_->generationSeed() ^ (std::uint64_t(position.x) << 32) ^
            std::uint32_t(position.y)
        );
        const auto id =
            world_->foundSettlement(position, playerRealmId_, profile);
        if (!id)
        {
            return {};
        }
        world_->settlement(id)->simulationState().setLocalMap(std::move(map));
        static_cast<void>(world_->renameSettlement(id, std::move(name)));
        static_cast<void>(setPresentedSettlement(id));
        return id;
    }

    bool Simulation::renamePlayerCapital(std::string name)
    {
        const Realm* realm = world_->realm(playerRealmId_);

        return realm && world_->renameSettlement(
                            realm->capitalSettlementId(),
                            std::move(name)
                        );
    }


    bool Simulation::editPlayerRealm(const FoundingIdentity& identity)
    {
        return world_->editRealmIdentity(playerRealmId_, identity);
    }


    bool Simulation::movePlayerCapital(WorldTilePosition position)
    {
        const Realm* realm = world_->realm(playerRealmId_);

        if (!realm)
        {
            return false;
        }

        Settlement* capital = world_->settlement(realm->capitalSettlementId());

        if (!capital || !world_->relocateSoleCapital(playerRealmId_, position))
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

            SettlementSimulationTier tier = SettlementSimulationTier::Strategic;

            if (settlement.id() == detailedSimulationSettlementId_)
            {
                tier = SettlementSimulationTier::Detailed;
            }
            else if (settlement.ownerRealmId() == playerRealmId_)
            {
                tier = SettlementSimulationTier::Inactive;
            }

            if (!worldSimulationPipeline_
                     ->transitionSettlementTier(*world_, settlement.id(), tier))
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
} // namespace Paladin

namespace Paladin
{
    std::string Simulation::systemTimingText() const
    {
        std::string text;
        for (std::size_t i = 0;
             i < worldSimulationPipeline_->systemTimings.size();
             ++i)
        {
            text += std::string(
                        i == 0   ? "Economy: "
                        : i == 1 ? "Population: "
                                 : "System: "
                    ) +
                    worldSimulationPipeline_->systemTimings[i].text() + "\n";
        }
        return text;
    }
} // namespace Paladin

namespace Paladin
{
    void Simulation::changeWorkDay(SettlementId id, bool realm, int delta)
    {
        const auto* settlement = world_->settlement(id);
        if (!settlement || settlement->ownerRealmId() != playerRealmId_)
        {
            return;
        }
        if (realm)
        {
            auto* realm = world_->realm(playerRealmId_);
            if (!realm)
            {
                return;
            }
            const int hours = std::clamp(realm->workDayHours() + delta, 0, 14);
            realm->setWorkDayHours(hours);
            // A realm enactment applies to every controlled city, including
            // future maps through the realm's persistent default.
            for (const auto& city : world_->settlements())
            {
                if (city.ownerRealmId() == playerRealmId_)
                {
                    if (auto* map = settlementMap(city.id()))
                    {
                        map->activities.policy.setWorkDayHours(hours);
                    }
                }
            }
        }
        else if (auto* map = settlementMap(id))
        {
            const auto& policy = map->activities.policy;
            map->activities.policy.setWorkDayHours(
                (policy.shiftEndMinute - policy.shiftStartMinute) / 60 + delta
            );
        }
    }
} // namespace Paladin
