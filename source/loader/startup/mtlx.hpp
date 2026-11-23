// GTA IV's Rockstar Games Launcher exe starts in a protection stub that loads MTLX.dll, whose worker thread checks, at
// random, whether kernel32 functions start with a jump, and crashes the game if one does. Overlays (Steam's hooks
// LoadLibrary and CreateProcess) always patch them, so the game crashed on some starts. While MTLX.dll is loaded, such
// patched kernel32 functions get their original first bytes back; the patches return when it unloads, before the game's
// own code runs.
#pragma once

namespace ual::startup::mtlx
{
    void Install();
}
