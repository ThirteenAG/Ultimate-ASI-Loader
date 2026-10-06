// Bink proxies forward to the game's original, renamed to <name>Hooked.dll.
#pragma once
#include <string>

namespace ual::proxy::bink
{
    // False if selfName is not a bink proxy.
    bool Load(const std::wstring& selfName);
}
