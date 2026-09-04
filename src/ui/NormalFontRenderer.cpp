#include "ui/NormalFontRenderer.h"

#include "rendering/Texture.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>

namespace Paladin
{
    struct NormalFontRenderer::Implementation
    {
        struct CachedText
        {
            std::unique_ptr<Texture> texture;
            int width = 0;
            int height = 0;
        };

        TTF_Font* font = nullptr;
        bool initializedTtf = false;
        std::unordered_map<std::string, CachedText> cache;
    };


    namespace
    {
        constexpr float normalFontPointSize = 18.0F;

        std::filesystem::path arimoFontPath()
        {
            const char* basePath = SDL_GetBasePath();

            return std::filesystem::path(
                basePath ? basePath : ""
            ) / "assets" / "fonts" / "Arimo-Regular.ttf";
        }


        std::string cacheKey(
            std::string_view text,
            RenderColor color
        )
        {
            std::string key;
            key.reserve(text.size() + 4U);
            key.push_back(static_cast<char>(color.red));
            key.push_back(static_cast<char>(color.green));
            key.push_back(static_cast<char>(color.blue));
            key.push_back(static_cast<char>(color.alpha));
            key.append(text);
            return key;
        }
    }


    NormalFontRenderer::NormalFontRenderer()
        : implementation_(std::make_unique<Implementation>())
    {
        if (!TTF_Init())
        {
            SDL_Log("TTF_Init failed: %s", SDL_GetError());
            return;
        }

        implementation_->initializedTtf = true;

        const std::string fontPath = arimoFontPath().string();
        implementation_->font = TTF_OpenFont(
            fontPath.c_str(),
            normalFontPointSize
        );

        if (!implementation_->font)
        {
            SDL_Log(
                "Unable to load Arimo from '%s': %s",
                fontPath.c_str(),
                SDL_GetError()
            );
        }
    }


    NormalFontRenderer::~NormalFontRenderer()
    {
        implementation_->cache.clear();

        if (implementation_->font)
        {
            TTF_CloseFont(implementation_->font);
        }

        if (implementation_->initializedTtf)
        {
            TTF_Quit();
        }
    }


    bool NormalFontRenderer::isValid() const noexcept
    {
        return implementation_->font != nullptr;
    }


    float NormalFontRenderer::measureWidth(
        std::string_view text
    ) const noexcept
    {
        if (!implementation_->font || text.empty())
        {
            return 0.0F;
        }

        int width = 0;
        int height = 0;

        if (!TTF_GetStringSize(
                implementation_->font,
                text.data(),
                text.size(),
                &width,
                &height
            ))
        {
            return 0.0F;
        }

        return static_cast<float>(width);
    }


    void NormalFontRenderer::drawText(
        Renderer& renderer,
        std::string_view text,
        float x,
        float y,
        RenderColor color
    )
    {
        if (!implementation_->font || text.empty())
        {
            return;
        }

        const std::string key = cacheKey(text, color);
        auto iterator = implementation_->cache.find(key);

        if (iterator == implementation_->cache.end())
        {
            constexpr std::size_t maximumCachedTextCount = 256U;

            if (
                implementation_->cache.size() >=
                    maximumCachedTextCount
            )
            {
                implementation_->cache.clear();
            }

            SDL_Surface* surface = TTF_RenderText_Blended(
                implementation_->font,
                text.data(),
                text.size(),
                {color.red, color.green, color.blue, color.alpha}
            );

            if (!surface)
            {
                SDL_Log("Unable to render Arimo text: %s", SDL_GetError());
                return;
            }

            Implementation::CachedText cached;
            cached.width = surface->w;
            cached.height = surface->h;
            cached.texture = renderer.createTextureFromSurface(
                surface,
                true
            );
            SDL_DestroySurface(surface);

            if (!cached.texture)
            {
                return;
            }

            iterator = implementation_->cache.emplace(
                key,
                std::move(cached)
            ).first;
        }

        const Implementation::CachedText& cached = iterator->second;
        renderer.drawTexture(
            *cached.texture,
            0.0F,
            0.0F,
            static_cast<float>(cached.width),
            static_cast<float>(cached.height),
            x,
            y,
            static_cast<float>(cached.width),
            static_cast<float>(cached.height)
        );
    }
}
