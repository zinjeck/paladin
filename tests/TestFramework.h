#pragma once

#include <stdexcept>
#include <string>

#define PALADIN_CHECK(condition)                                               \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            throw std::runtime_error(                                          \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) +       \
                " Test failed: " + #condition                                  \
            );                                                                 \
        }                                                                      \
    } while (false)
