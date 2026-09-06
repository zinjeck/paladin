#pragma once
#include "world/entities/EntityState.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include <span>
#include <string_view>
#include <vector>

namespace Paladin
{
    class SettlementMap;
    class SettlementCitizenState;
    enum class AnimalOrder
    {
        None,
        Gather,
        Hunt
    };
    struct AnimalSpecies
    {
        std::string_view id, name;
        int pastureSpace;
        double handlerLoad;
        double foodPerWorkday;
        int huntMeat;
        double markerWidth, markerHeight;
        std::uint32_t coatRgb;
        double breedingChancePerDay;
        double maturationDays;
    };
    std::span<const AnimalSpecies> animalSpecies();
    const AnimalSpecies* animalSpecies(std::string_view);
    struct AnimalPolicy
    {
        int herdsPerSpecies = 3;
        int animalsPerHerd = 4;
        int spawnAttemptsPerHerd = 128;
        int roamingRadius = 5;
        double wanderMinutes = 10;
        double handlerCapacity = 6;
        double productiveWorkdayMinutes = 720;
        double huntMinutes = 30;
        double usablePastureFraction = .9;
        double herdRadius = 3;
        double cohesionWeight = 1.5;
        double tendingMinutes = 20;
        double tendingCooldownMinutes = 15;
        double tendingReservationMinutes = 90;
    };
    struct SettlementAnimal : EntityState
    {
        std::string species;
        SettlementTilePosition herdCenter;
        SettlementTilePosition previousTile;
        double visualProgress = 1;
        SettlementObjectId pasture;
        SettlementObjectId reservedPasture;
        CitizenId handler;
        bool beingLed = false;
        CitizenId tender;
        double tendingReservedUntil = 0;
        double lastTendedMinute = -100000;
        AnimalOrder order = AnimalOrder::None;
        double nextWanderMinute = 0;
        std::uint64_t sequence = 0;
        bool female = false;
        bool juvenile = false;
        double ageMinutes = 0;
        double breedingExposure = 0;
        double breedingTarget = 0;
        double visualX() const
        {
            return previousTile.x +
                   (tilePosition.x - previousTile.x) * visualProgress;
        }
        double visualY() const
        {
            return previousTile.y +
                   (tilePosition.y - previousTile.y) * visualProgress;
        }
    };
    class SettlementAnimals
    {
    public:
        AnimalPolicy policy;
        void captureVisualPositions()
        {
            for (auto& a : animals_)
            {
                a.captureVisual(a.visualX(), a.visualY());
            }
        }
        void initialize(const SettlementMap&, std::uint64_t seed);
        EntityId spawn(
            const SettlementMap&,
            std::string_view species,
            SettlementTilePosition
        );
        std::span<const SettlementAnimal> all() const
        {
            return animals_;
        }
        bool hasOrders() const
        {
            return pendingOrders_ > 0;
        }
        SettlementAnimal* find(EntityId);
        const SettlementAnimal* find(EntityId) const;
        std::size_t designate(const SettlementObjectFootprint&, AnimalOrder);
        std::size_t cancel(const SettlementObjectFootprint&);
        void release(CitizenId);
        int usedSpace(SettlementObjectId) const;
        std::int64_t capacity(const SettlementObjectFootprint& footprint) const
        {
            const auto area = std::int64_t(footprint.width) * footprint.height;
            return std::min(
                area,
                std::max<std::int64_t>(
                    4,
                    std::int64_t(
                        std::floor(
                            area *
                            std::clamp(policy.usablePastureFraction, 0.0, 1.0)
                        )
                    )
                )
            );
        }
        int containedCount(SettlementObjectId) const;
        bool reserve(
            EntityId,
            CitizenId,
            SettlementObjectId,
            const SettlementMap&
        );
        bool contain(EntityId, CitizenId, const SettlementMap&);
        void follow(
            EntityId,
            CitizenId,
            SettlementTilePosition,
            const SettlementMap&
        );
        bool hunt(EntityId, CitizenId, SettlementMap&, double minute);
        void tick(
            SettlementMap&,
            const SettlementCitizenState&,
            double minute,
            double elapsed,
            bool move = true
        );
        void produce(
            SettlementMap&,
            SettlementObjectId,
            int workers,
            double minute,
            double elapsed
        );

    private:
        std::vector<SettlementAnimal> animals_;
        std::uint64_t seed_ = 0;
        std::uint64_t nextId_ = 1;
        bool initialized_ = false;
        std::size_t pendingOrders_ = 0;
        double lifeElapsed_ = 0;
        void breed(SettlementMap&, double minute, double elapsed);
    };
} // namespace Paladin
