#pragma once
#include <memory>

struct SDL_Texture;

namespace Paladin
{
    class Renderer;

    class Texture
    {
    public:
        ~Texture();

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        [[nodiscard]]
        int width() const noexcept;

        [[nodiscard]]
        int height() const noexcept;

        // Runtime optical effects such as emitted light need additive
        // composition without exposing SDL_Texture outside this abstraction.
        // Authored sprites continue to use their existing alpha/premultiplied
        // paths unless a caller opts into this explicitly.
        void setAdditiveBlending(bool enabled) noexcept;

    private:
        friend class Renderer;

        Texture(SDL_Texture* texture, int width, int height) noexcept;

        SDL_Texture* texture_ = nullptr;
        int width_ = 0;
        int height_ = 0;
        bool premultiplied_ = false;
        std::shared_ptr<Texture> parent_;
        int atlasX_ = 0, atlasY_ = 0;
        float uvX(float u) const
        {
            return parent_ ? (atlasX_ + u * width_) / parent_->width_ : u;
        }
        float uvY(float v) const
        {
            return parent_ ? (atlasY_ + v * height_) / parent_->height_ : v;
        }
    };
} // namespace Paladin
