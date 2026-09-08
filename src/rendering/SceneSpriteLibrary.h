#pragma once
#include "rendering/ScenePresentation.h"
#include "assets/PresentationData.h"
#include "assets/AssetManager.h"
#include "rendering/Texture.h"
#include "rendering/TreeSpecies.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Paladin
{
    struct SceneSprite : SpriteAsset {
        std::shared_ptr<Texture> texture,shadow;
    };
    // Optional artist-owned exports. Missing catalog = existing placeholders.
    // Loaded once, independent of working directory and simulation RNG.
    class SceneSpriteLibrary
    {
    public:
        // Shared presentation preference: entity artwork remains independent.
        static bool environmentArtEnabled()
        {
            return environmentArtEnabled_;
        }
        static void setEnvironmentArtEnabled(bool enabled)
        {
            environmentArtEnabled_ = enabled;
        }
        void load(Renderer& renderer, const std::string& root);
        void reset()
        {
            loaded_ = false;
            // Keep the previous valid generation alive if replacement validation fails.
        }
        const std::vector<BlueprintLight>& lights() const
        {
            return lights_;
        }
        const std::vector<BuildingPiece>& pieces() const
        {
            return pieces_;
        }
        void setShadowsEnabled(bool enabled)
        {
            shadowsEnabled_ = enabled;
        }
        bool shadowsEnabled() const
        {
            return shadowsEnabled_;
        }
        void setTime(double seconds)
        {
            seconds_ = seconds;
        }
        double time() const
        {
            return seconds_;
        }
        RenderRectangle frame(
            const SceneSprite& sprite,
            bool animate = true
        ) const;
        bool placed(
            SceneDrawQueue&,
            const SceneProjection&,
            const std::string&,
            double x,
            double y,
            double depth,
            std::uint64_t id,
            int part = 0,
            double fittedWidth = 0,
            double fittedHeight = 0,
            double doorOpen = 0
        ) const;
        const SceneSprite* find(const std::string& id) const;
        bool submitTree(
            SceneDrawQueue&,
            const SceneProjection&,
            double x,
            double y,
            std::uint64_t id,
            double scale,
            double crownScale = 1,
            TreeSpecies species = TreeSpecies::Broadleaf
        ) const;
        const ObjectPresentation& objectStyle(const std::string& id) const;
        bool objectHasArt(const std::string& id) const;
        bool submit(
            SceneDrawQueue&,
            const SceneProjection&,
            const std::string& id,
            double x,
            double y,
            std::uint64_t stableId,
            double scale = 1
        ) const;
        // Repeated modules, never stretch one roof across an arbitrary
        // footprint.
        void surface(
            SceneDrawQueue&,
            const SceneProjection&,
            const std::string& id,
            const SceneVisual&,
            RenderColor fallback,
            double depth,
            std::uint64_t stableId,
            int part,
            bool placeholder = true
        ) const;

    private:
        std::shared_ptr<AssetManager> assets_;
        bool shadowsEnabled_ = true;
        void submitWind(
            SceneDrawQueue&,
            const SceneSprite&,
            const RenderRectangle&,
            double depth,
            std::uint64_t id,
            int part,
            bool roof
        ) const;
        inline static bool environmentArtEnabled_ = true;
        bool loaded_ = false;
        double seconds_ = 0;
        std::vector<BuildingPiece> pieces_;
        std::vector<BlueprintLight> lights_;
        std::unordered_map<std::string, SceneSprite> sprites_;
        std::unordered_map<std::string, ObjectPresentation> objects_;
    };
} // namespace Paladin
