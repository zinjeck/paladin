#include "rendering/ScenePresentation.h"
namespace Paladin
{
    void SceneDrawQueue::render(
        Renderer& renderer,
        int minimumLayer,
        int maximumLayer
    )
    {
        if (!sorted_)
        {
            std::stable_sort(items_.begin(), items_.end(), before);
            sorted_ = true;
        }
        thread_local std::vector<TextureDrawItem> draws;
        draws.clear();
        const auto flush = [&]
        {
            renderer.drawTextureItems(draws);
            draws.clear();
        };
        const int viewportWidth = renderer.outputWidth(),
                  viewportHeight = renderer.outputHeight();
        for (const auto& item : items_)
        {
            if (item.layer < minimumLayer || item.layer > maximumLayer)
            {
                continue;
            }
            const auto& b = item.bounds;
            if (b.x + b.width < 0 || b.y + b.height < 0 ||
                b.x >= viewportWidth || b.y >= viewportHeight)
            {
                continue;
            }
            if (item.texture && item.thatchPixelPitch > 0)
            {
                flush();
                drawThatch(renderer, item);
                continue;
            }
            draws.push_back(
                {item.texture, item.atlasFrame, b, item.color, item.opacity}
            );
        }
        flush();
    }
    void SceneDrawQueue::drawThatch(
        Renderer& renderer,
        const SceneDrawItem& item
    )
    {
        roofVertices_.clear();
        roofIndices_.clear();
        const auto& b = item.bounds;
        const auto& f = item.atlasFrame;
        const bool side = item.ridgeAlongDepth;
        const float length = side ? b.height : b.width;
        const float breadth = side ? b.width : b.height;
        const int columns =
            std::clamp(int(std::round(length / item.thatchPixelPitch)), 8, 512);
        const float pixel = item.thatchPixelPitch / breadth;
        const auto vertex = [&](float u, float v, float tu, float tv, int shade)
        {
            if (side)
            {
                std::swap(u, v);
                std::swap(tu, tv);
            }
            return MeshVertex{
                b.x + b.width * u,
                b.y + b.height * v,
                (f.x + f.width * tu) / item.texture->width(),
                (f.y + f.height * tv) / item.texture->height(),
                {std::uint8_t(shade),
                 std::uint8_t(shade),
                 std::uint8_t(shade),
                 item.opacity}
            };
        };
        const auto quad = [&](float l,
                              float r,
                              float top,
                              float bottom,
                              float st,
                              float sb,
                              int shade)
        {
            const int base = int(roofVertices_.size());
            roofVertices_.push_back(vertex(l, top, l, st, shade));
            roofVertices_.push_back(vertex(r, top, r, st, shade));
            roofVertices_.push_back(vertex(r, bottom, r, sb, shade));
            roofVertices_.push_back(vertex(l, bottom, l, sb, shade));
            for (int i : {0, 1, 2, 0, 2, 3})
            {
                roofIndices_.push_back(base + i);
            }
        };
        if (!side)
        {
            // Keep the foreshortened rear slope continuous. Thin, separately
            // rasterized quads can lose whole source-texel columns on software
            // backends. Three broad joined planes preserve the hipped outline.
            const auto rear = [&](float l, float r, float leftTop, float rightTop, int shade)
            {
                const int base = int(roofVertices_.size());
                roofVertices_.push_back(vertex(l,leftTop,l,0,shade));
                roofVertices_.push_back(vertex(r,rightTop,r,0,shade));
                roofVertices_.push_back(vertex(r,.29F,r,.5F,shade));
                roofVertices_.push_back(vertex(l,.29F,l,.5F,shade));
                for (int i : {0,1,2,0,2,3}) { roofIndices_.push_back(base+i); }
            };
            rear(0,.24F,.21F,0,190);
            rear(.24F,.76F,0,0,213);
            rear(.76F,1,0,.21F,190);
            const float eave = std::max(.29F, 1 - 3 * pixel);
            const float eaveV = .5F + .5F * (eave - .29F) / .71F;
            quad(0,1,.29F,eave,.5F,eaveV,255);
            const auto hipFace = [&](bool right)
            {
                const float edge = right ? 1.F : 0.F;
                const float shoulder = right ? .76F : .24F;
                const int base = int(roofVertices_.size());
                roofVertices_.push_back(vertex(edge,.29F,edge,.5F,205));
                roofVertices_.push_back(vertex(shoulder,.29F,shoulder,.5F,205));
                roofVertices_.push_back(vertex(edge,eave,edge,eaveV,205));
                for (int i : {0,1,2}) { roofIndices_.push_back(base+i); }
            };
            hipFace(false); hipFace(true);
            quad(0,1,.29F-pixel,.29F,.46F,.49F,255);
        }
        for (int col = 0; col < columns; ++col)
        {
            const float l = float(col) / columns, r = float(col + 1) / columns;
            const float u = (l + r) * .5F;
            const float hip = std::max(0.F, 1.F - std::min(u, 1 - u) / .24F);
            const auto hash =
                std::uint32_t(col * 2654435761u + item.stableId * 2246822519u);
            // Ragged ends are small, stable whole art pixels, never
            // random frame-to-frame changes to the structural roof.
            // The front elevation has a foreshortened rear slope. A
            // depth-facing ridge instead needs two balanced roof planes;
            // transposing that .29 ridge made a lopsided slab in side view.
            const float ridge = side ? .5F : .29F;
            // In side view the near eave still covers the full front
            // wall. Only the far end foreshortens; clipping both ends
            // exposes the front wall corners beneath the roof.
            const float silhouetteHip =
                side ? std::max(0.F, 1.F - u / .24F) : hip;
            const float inset =
                std::round((.21F * silhouetteHip) / pixel) * pixel;
            const float fringe = (hash >> 27) % 3 * pixel;
            const float wind =
                item.windSeconds > 0 &&
                        std::sin(item.windSeconds * 2.1 + (hash % 97) * .31) >
                            .85
                    ? pixel
                    : 0;
            const float top = inset + (side ? fringe + wind : 0);
            const float bottom = 1 - fringe - wind - (side ? inset : 0);
            if (!side)
            {
                const float eave = std::max(ridge,1-3*pixel);
                if (bottom > eave)
                {
                    quad(l,r,eave,bottom,.5F+.5F*(eave-ridge)/(1-ridge),1,
                         hip > .85F ? 205 : 255);
                }
                continue;
            }
            if (side) { quad(
                l,
                r,
                top,
                ridge,
                0,
                .5F,
                side ? (hip > .05F ? 218 : 250) : (hip > .05F ? 190 : 213)
            ); }
            const float seam = ridge + (bottom - ridge) * hip;
            if (hip > 0)
            {
                quad(l, r, ridge, seam, .5F, .5F + .5F * hip, 205);
            }
            if (hip < 1)
            {
                quad(l, r, seam, bottom, .5F + .5F * hip, 1, side ? 215 : 255);
            }
            // A narrow binding catches the light at the pitched ridge.
            if (!side || hip < .05F)
            {
                quad(l, r, ridge - pixel, ridge, .46F, .49F, 255);
            }
        }
        renderer.drawMesh(*item.texture, roofVertices_, roofIndices_);
    }
} // namespace Paladin
