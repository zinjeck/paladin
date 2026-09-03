#pragma once

#include "core/StrongId.h"

namespace Paladin
{
    class Polity
    {
    public:
        explicit Polity(
            PolityId id
        ) noexcept
            : id_(id)
        {
        }

        [[nodiscard]]
        PolityId id() const noexcept
        {
            return id_;
        }

    private:
        PolityId id_;
    };
}