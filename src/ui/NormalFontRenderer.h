#pragma once

#include "rendering/Renderer.h"

#include <memory>
#include <string_view>

namespace Paladin
{
    class NormalFontRenderer
    {
    public:
        NormalFontRenderer();
        ~NormalFontRenderer();

        NormalFontRenderer(const NormalFontRenderer&) = delete;
        NormalFontRenderer& operator=(const NormalFontRenderer&) = delete;

        [[nodiscard]]
        bool isValid() const noexcept;

        [[nodiscard]]
        float measureWidth(std::string_view text) const noexcept;

        void drawText(
            Renderer& renderer,
            std::string_view text,
            float x,
            float y,
            RenderColor color = {242, 242, 244, 255}
        );

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;
    };
}
