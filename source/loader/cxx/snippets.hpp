// .cxx snippets are C++ files loaded like ASI plugins and compiled in-process by cxxsnippets/. The same file also
// builds as a DLL with a real compiler. Globals initialize at compile time, then Init() runs. Shutdown() runs on unload.
// A crash in global init or Init unloads the snippet and reports the line. Later crashes are logged to
// <loader name>.log, and crash reports name the snippet line.
// CxxHotReload=1 swaps in a changed snippet only if it compiles (Shutdown, hooks released, memory writes undone).
// Header changes (.h, .hpp, .inl) reload every snippet in the folder.
#pragma once
#include <string>

namespace ual::cxx
{
    // *.cxx directly in dir, in name order
    void LoadFolder(const std::wstring& dir);

    // With CxxHotReload, starts watching every folder LoadFolder saw. Call once all plugin folders are loaded.
    void StartHotReload();
}
