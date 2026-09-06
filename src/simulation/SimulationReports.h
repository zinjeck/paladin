#pragma once
#include "core/StrongId.h"
#include "world/WorldTilePosition.h"
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
namespace Paladin
{
    class World;
    struct ReportPolicy
    {
        double refreshMinutes = 10, sampleMinutes = 60;
        double lowHealth = 65, recoveryMargin = 5, lowHappiness = 35;
        std::size_t maximumEvents = 128, maximumSamples = 240;
    };
    struct ReportSample
    {
        double minute = 0, population = 0, health = 0, happiness = 0, food = 0,
               gold = 0;
    };
    struct ReportEvent
    {
        double minute = 0;
        std::string key, text;
        SettlementId city;
        RealmId realm;
        bool warning = true, resolved = false;
    };
    struct CityReport
    {
        ReportSample current;
        std::deque<ReportSample> history;
        std::unordered_map<std::string, bool> warnings;
        std::size_t starving = 0;
        bool detailed = false;
    };
    class SimulationReports
    {
    public:
        ReportPolicy policy;
        void update(const World&, RealmId player, bool force = false);
        const CityReport* city(SettlementId id) const;
        const std::deque<ReportEvent>& events() const
        {
            return events_;
        }
        const std::deque<ReportSample>& realmHistory() const
        {
            return realmHistory_;
        }
        std::uint64_t version() const
        {
            return version_;
        }

    private:
        void emit(ReportEvent);
        std::unordered_map<SettlementId, CityReport, StrongIdHash> cities_;
        struct RealmState
        {
            std::string name, origin;
            SettlementId capital;
            WorldTilePosition capitalPosition;
            int tax = 0;
        };
        std::unordered_map<RealmId, RealmState, StrongIdHash> knownRealms_;
        std::unordered_map<SettlementId, std::string, StrongIdHash>
            knownCities_;
        std::deque<ReportEvent> events_;
        std::deque<ReportSample> realmHistory_;
        double nextRefresh_ = -1;
        bool initialized_ = false;
        std::uint64_t version_ = 0;
    };
} // namespace Paladin
