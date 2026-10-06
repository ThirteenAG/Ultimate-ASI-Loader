// Returned by tests/probe/fake_original.cpp, seen by the host when the loader chains to <name>Hooked.dll
#pragma once

namespace ualtest
{
    constexpr long kFakeDirectInput8CreateResult = 0x7A170001;
    constexpr unsigned long kFakeFileVersionInfoSize = 0x00C0FFEE;
    constexpr unsigned long kFakeTimeGetTime = 0x7A1E0001;
    constexpr long kFakeOvStreams = 0x5EED;
}
