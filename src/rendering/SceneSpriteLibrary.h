#pragma once
#include "rendering/ScenePresentation.h"
#include "rendering/Texture.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Paladin
{
    // Artist-facing recipe; object IDs and artwork never select renderer code.
    struct ObjectPresentation
    {
        std::string mode = "ground";
        std::string floor, wall, roof, sprite;
        double moduleWidth = 1, moduleDepth = 1;
        double height = 0, thickness = .12;
        int frontShade = 30, sideShade = 65, edgeLight = 32, shadowAlpha = 65;
        bool outline = true;
        std::uint32_t fillRgb = 0x999999, frameRgb = 0x555555;
        double bodyWidth = .6, bodyDepth = .45;
        // Optional shared accessory recipe, independent of wall/roof materials.
        std::string decor = "-";
    };
    struct BlueprintLight
    {
        std::string object;
        double x = 0, y = 0, radius = 4.5, intensity = 1;
        RenderColor color{255, 180, 90, 255};
    };
    struct BuildingPiece
    {
        std::string object, sprite, state;
        double x = 0, y = 0, depth = 0;
        unsigned choices = 1, choice = 0;
    };
    struct SceneSprite
    {
        std::shared_ptr<Texture> texture;
        std::shared_ptr<Texture> shadow;
        RenderColor overviewColor{0, 0, 0, 0};
        std::shared_ptr<const std::vector<RenderColor>> materialPixels;
        RenderColor materialBase{};
        int materialWidth = 0, materialHeight = 0;
        double width = 1, height = 1, pivotX = .5, pivotY = 1, elevation = 0;
        int frames = 1;
        double fps = 0;
        RenderRectangle
            doorRegion{0, 0, 0, 0}; // Normalized leaf crop; optional.
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
            sprites_.clear();
            objects_.clear();
            pieces_.clear();
            lights_.clear();
        }
        const std::vector<BlueprintLight>& lights() const
        {
            return lights_;
        }
        const std::vector<BuildingPiece>& pieces() const
        {
            return pieces_;
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
            double crownScale = 1
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
