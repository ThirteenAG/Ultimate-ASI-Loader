// Xidi (github.com/samuelgr/Xidi). Its dinput.dll, dinput8.dll and winmm.dll are forwarders to
// Xidi.32.dll / Xidi.64.dll, which exports the functions as <dll>_<name>, e.g. dinput8_DirectInput8Create.
// The loader under one of those names takes the same exports, so Xidi works without renaming anything.
#pragma once
#include <windows.h>
#include <string>

namespace ual::compat::xidi
{
    // proxy is "dinput", "dinput8" or "winmm". Loads Xidi.32.dll / Xidi.64.dll from dir (with a trailing
    // backslash), null if it isn't there or Xidi doesn't replace this DLL.
    HMODULE Load(const char* proxy, const std::wstring& dir);

    // <proxy>_<name> from Xidi, null if Xidi leaves that function to the system DLL
    FARPROC Export(HMODULE xidi, const char* proxy, const char* name);
}
