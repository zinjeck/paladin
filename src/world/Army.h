#pragma once

#include "core/StrongId.h"

namespace Paladin
{
    class Army
    {
    public:
        explicit Army(
            ArmyId id
        ) noexcept
            : id_(id)
        {
        }

        [[nodiscard]]
        ArmyId id() const noexcept
        {
            return id_;
        }

    private:
        ArmyId id_;
    };
}