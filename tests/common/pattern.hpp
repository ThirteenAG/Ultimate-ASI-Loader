// Deterministic content the runner zips and the host verifies
#pragma once
#include <cstdint>
#include <string>

namespace ualtest
{
    inline std::string Pattern(size_t n, uint32_t seed)
    {
        std::string s(n, '\0');
        uint32_t x = seed * 2654435761u + 12345u;
        for (size_t i = 0; i < n; ++i)
        {
            x = x * 1103515245u + 12345u;
            // compressible but never constant
            s[i] = (char)('a' + ((x >> 16) % 4) + (i % 512 == 0 ? 10 : 0));
        }
        return s;
    }

    constexpr int kZipConcurrentFiles = 12;
    constexpr size_t kZipConcurrentFileSize = 1024 * 1024 + 17;
}
