// 01 - Basics: the shape of a snippet
//
// Use for: learning what the loader does with a .cxx file.
//
// Put a .cxx file where .asi files go (the game folder, scripts\ or plugins\).
// When the game starts, the loader compiles it in memory and runs it:
//
//   1. globals are initialized (top to bottom),
//   2. Init() is called, if the snippet has one.
//
// Shutdown() is called when the snippet is unloaded: when the file changes and
// is reloaded (CxxHotReload=1 in global.ini), not when the game exits.
// After Shutdown, everything made with the built-in headers is undone
// automatically: hooks are removed and bytes written with injector:: are
// restored (see 13_hot_reload.cxx).
//
// Errors: a compile error is shown in a dialog with the file and line, and
// the snippet is skipped. A crash in Init() is caught, written to the log
// with the line that crashed, and the snippet is unloaded.
#include <windows.h>
#include <cstdio>

// Globals live as long as the snippet.
int initCount = 0;
char message[128];

void Init()
{
    ++initCount;

    // snprintf, printf, memcpy, strlen, ... from <cstdio>/<cstring>.
    snprintf(message, sizeof(message), "basics: Init called %d time(s)\n", initCount);

    // View with DebugView or a debugger.
    OutputDebugStringA(message);
}

void Shutdown()
{
    OutputDebugStringA("basics: Shutdown\n");
}

// The same file can also be built as a normal .asi with Visual Studio
// (handy for debugging with breakpoints). __CXXSNIPPETS__ is defined only
// when the loader compiles the file.
#ifndef __CXXSNIPPETS__
extern "C" __declspec(dllexport) void InitializeASI()
{
    Init();
}
#endif
