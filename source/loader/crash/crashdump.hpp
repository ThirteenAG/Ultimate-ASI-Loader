// On a crash, writes a minidump and a readable log to a CrashDumps folder next to the loader or the exe.
// Creating that folder enables the feature.
#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace crashdump
{
    struct Settings
    {
        bool disabled = false;   // [GlobalSets] DisableCrashDumps
        bool fullMemory = false; // [GlobalSets] CrashDumpFullMemory
        bool zip = true;         // [GlobalSets] CrashDumpZip, packs dump, log and ini files into one .zip
        int maxReports = 10;     // [GlobalSets] CrashDumpMaxReports, 0 keeps all
        std::vector<std::wstring> iniPaths; // added to the archive
        // Called first thing on a crash, from the crashing thread. Used to put hooks that take locks
        // into pass-through mode, since other threads are suspended while the dump is written.
        void (*onCrash)() = nullptr;
    };

    // False if crash dumps are disabled or there is no CrashDumps folder.
    bool Install(HMODULE loader, const Settings& settings);

    // Empty if not installed.
    const wchar_t* Folder();

    // Names code outside any module, e.g. .cxx snippets ("scripts\fix.cxx line 12, in OnUpdate").
    // Runs while the report is written, so it must not wait for locks.
    using CodeDescriber = bool (*)(const void* address, char* out, size_t size);
    void SetCodeDescriber(CodeDescriber describer);
}
