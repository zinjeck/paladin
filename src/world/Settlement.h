#pragma once

#include "core/StrongId.h"

namespace Paladin
{
    class Settlement
    {
    public:
        explicit Settlement(
            SettlementId id
        ) noexcept
            : id_(id)
        {
        }

        [[nodiscard]]
        SettlementId id() const noexcept
        {
            return id_;
        }

    private:
        SettlementId id_;
    };
}