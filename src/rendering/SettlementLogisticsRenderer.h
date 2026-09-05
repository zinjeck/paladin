#pragma once
#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
#include <optional>

namespace Paladin
{
class Renderer;
class Camera2D;
class SettlementMap;
class SettlementObjectPlacementController;
class SettlementInspectionController;
struct TileRenderMetrics;
class SettlementLogisticsRenderer
{
  public:
    void render(
        Renderer&,
        const SettlementMap&,
        const Camera2D&,
        const TileRenderMetrics&,
        const SettlementObjectPlacementController&,
        const SettlementInspectionController&
    ) const;

  private:
    mutable std::uint64_t mapId_ = 0;
    mutable std::uint64_t version_ = 0;
    mutable std::optional<SettlementObjectFootprint> footprint_;
    mutable FisheryZonePreview preview_;
};
} // namespace Paladin
