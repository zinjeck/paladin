#pragma once

#include "core/StrongId.h"

#include <cstddef>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Paladin
{
    template<typename EntityType, typename IdType>
    class EntityRegistry
    {
    public:
        template<typename... Args>
        [[nodiscard]]
        IdType create(Args&&... args)
        {
            const IdType id = idGenerator_.generate();
            const std::size_t index = entities_.size();

            entities_.emplace_back(
                id,
                std::forward<Args>(args)...
            );

            try
            {
                indexById_.emplace(id, index);
            }
            catch (...)
            {
                entities_.pop_back();
                throw;
            }

            return id;
        }

        [[nodiscard]]
        EntityType* find(IdType id) noexcept
        {
            const auto iterator = indexById_.find(id);

            if (iterator == indexById_.end())
            {
                return nullptr;
            }

            return &entities_[iterator->second];
        }

        [[nodiscard]]
        const EntityType* find(IdType id) const noexcept
        {
            const auto iterator = indexById_.find(id);

            if (iterator == indexById_.end())
            {
                return nullptr;
            }

            return &entities_[iterator->second];
        }

        [[nodiscard]]
        bool contains(IdType id) const noexcept
        {
            return indexById_.contains(id);
        }

        bool erase(IdType id)
        {
            const auto iterator = indexById_.find(id);

            if (iterator == indexById_.end())
            {
                return false;
            }

            const std::size_t index = iterator->second;
            const std::size_t lastIndex =
                entities_.size() - 1;

            if (index != lastIndex)
            {
                std::swap(
                    entities_[index],
                    entities_[lastIndex]
                );

                indexById_[entities_[index].id()] = index;
            }

            entities_.pop_back();
            indexById_.erase(iterator);

            return true;
        }

        [[nodiscard]]
        std::size_t size() const noexcept
        {
            return entities_.size();
        }

        [[nodiscard]]
        bool empty() const noexcept
        {
            return entities_.empty();
        }

        [[nodiscard]]
        std::span<EntityType> entities() noexcept
        {
            return {
                entities_.data(),
                entities_.size()
            };
        }

        [[nodiscard]]
        std::span<const EntityType> entities() const noexcept
        {
            return {
                entities_.data(),
                entities_.size()
            };
        }

    private:
        IdGenerator<IdType> idGenerator_;

        // Dense storage for iteration.
        std::vector<EntityType> entities_;

        // This map has exactly one job:
        // stable ID -> current dense-array position.
        std::unordered_map<
            IdType,
            std::size_t,
            StrongIdHash
        > indexById_;
    };
}