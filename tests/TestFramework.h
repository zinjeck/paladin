#pragma once

#include <stdexcept>
#include <string>

#define PALADIN_CHECK(condition)                                      \
    do                                                                \
    {                                                                 \
        if (!(condition))                                             \
        {                                                             \
            throw std::runtime_error(                                 \
                std::string("Test failed: ") + #condition             \
            );                                                        \
        }                                                             \
    } while (false)
