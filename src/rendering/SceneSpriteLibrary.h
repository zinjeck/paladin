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
    };
    struct SceneSprite
    {
        std::shared_ptr<Texture> texture;
        double width = 1, height = 1, pivotX = .5, pivotY = 1, elevation = 0;
    };
    // Optional artist-owned exports. Missing catalog = existing placeholders.
    // Loaded once, independent of working directory and simulation RNG.
    class SceneSpriteLibrary
    {
    public:
        void load(Renderer& renderer, const std::string& root);
        const SceneSprite* find(const std::string& id) const;
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
        bool loaded_ = false;
        std::unordered_map<std::string, SceneSprite> sprites_;
        std::unordered_map<std::string, ObjectPresentation> objects_;
    };
} // namespace Paladin
