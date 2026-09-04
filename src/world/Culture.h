#pragma once

#include "core/StrongId.h"

#include <string>
#include <string_view>
#include <utility>

namespace Paladin
{
    class World;

    class Culture
    {
    public:
        Culture(
            CultureId id,
            std::string name
        )
            : id_(id),
              name_(std::move(name))
        {
        }

        [[nodiscard]]
        CultureId id() const noexcept
        {
            return id_;
        }

        [[nodiscard]]
        std::string_view name() const noexcept
        {
            return name_;
        }

    private:
        friend class World;

        void setName(std::string name)
        {
            name_ = std::move(name);
        }

        CultureId id_;
        std::string name_;
    };
}
