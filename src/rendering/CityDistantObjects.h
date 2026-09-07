#pragma once
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"

namespace Paladin
{
    // Small map-space silhouettes survive camera movement without rebuilding
    // artwork.
    class CityDistantObjects
    {
        mutable std::unique_ptr<Texture> texture_;
        mutable std::uint64_t instance_ = 0, version_ = ~std::uint64_t(0);
        mutable bool roofed_ = true;
        mutable int left_ = 0, top_ = 0, right_ = 0, bottom_ = 0;

    public:
        void render(
            Renderer& renderer,
            const SceneProjection& p,
            const SettlementMap& map,
            const SceneSpriteLibrary& sprites,
            bool roofed,
            double opacity
        ) const
        {
            if (opacity <= 0)
            {
                return;
            }
            const int width = map.grid().width(), height = map.grid().height();
            if (instance_ != map.instanceId() ||
                version_ != map.objectState().navigationVersion() ||
                roofed_ != roofed)
            {
                instance_ = map.instanceId();
                version_ = map.objectState().navigationVersion();
                roofed_ = roofed;
                left_ = width;
                top_ = height;
                right_ = bottom_ = 0;
                std::vector<RenderColor> pixels(
                    std::size_t(width) * height,
                    {0, 0, 0, 0}
                );
                for (const auto& object : map.objectState().completedObjects())
                {
                    const auto& f = object.footprint;
                    left_ = std::min(left_, f.topLeft.x);
                    top_ = std::min(top_, f.topLeft.y);
                    right_ = std::max(right_, f.topLeft.x + f.width);
                    bottom_ = std::max(bottom_, f.topLeft.y + f.height);
                    const auto& style =
                        sprites.objectStyle(object.objectTypeId);
                    for (int y = std::max(0, f.topLeft.y);
                         y < std::min(height, f.topLeft.y + f.height);
                         ++y)
                    {
                        for (int x = std::max(0, f.topLeft.x);
                             x < std::min(width, f.topLeft.x + f.width);
                             ++x)
                        {
                            RenderColor color;
                            if (object.objectTypeId ==
                                SettlementObjectTypes::Road)
                            {
                                color = {0x74, 0x51, 0x3F, 255};
                            }
                            else if (object.objectTypeId == SettlementObjectTypes::Pastureland)
                            {
                                const bool edge = x == f.topLeft.x || y == f.topLeft.y || x == f.topLeft.x + f.width - 1 || y == f.topLeft.y + f.height - 1;
                                color = edge ? RenderColor{0x74,0x51,0x3F,255} : RenderColor{0,0,0,0};
                            }
                            else if (style.mode == "enclosed")
                            {
                                color =
                                    roofed && x < f.topLeft.x + f.width * .5
                                        ? RenderColor{0xBD, 0x86, 0x4C, 255}
                                        : RenderColor{0x88, 0x60, 0x44, 255};
                            }
                            else if (
                                style.mode == "ground" ||
                                style.mode == "modules"
                            )
                            {
                                color = {0xA9, 0x94, 0x78, 255};
                            }
                            else
                            {
                                color = {0x7A, 0x50, 0x38, 255};
                            }
                            pixels[std::size_t(y) * width + x] = color;
                        }
                    }
                }
                if (!texture_ || texture_->width() != width ||
                    texture_->height() != height ||
                    !renderer.updateTexturePixels(*texture_, pixels))
                {
                    texture_ =
                        renderer.createTextureFromPixels(width, height, pixels);
                }
            }
            if (texture_ && right_ > left_ && bottom_ > top_)
            {
                renderer.drawTexture(
                    *texture_,
                    float(left_),
                    float(top_),
                    float(right_ - left_),
                    float(bottom_ - top_),
                    float(
                        p.screenWidth * .5 + (left_ - p.cameraX) * p.tilePixels
                    ),
                    float(
                        p.screenHeight * .5 + (top_ - p.cameraY) * p.tilePixels
                    ),
                    float((right_ - left_) * p.tilePixels),
                    float((bottom_ - top_) * p.tilePixels),
                    std::uint8_t(255 * opacity)
                );
            }
        }
    };
} // namespace Paladin
