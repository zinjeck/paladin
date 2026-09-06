#pragma once
#include "rendering/ScenePresentation.h"
#include "rendering/Texture.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Paladin
{
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
            int part
        ) const;

    private:
        bool loaded_ = false;
        std::unordered_map<std::string, SceneSprite> sprites_;
    };
} // namespace Paladin
