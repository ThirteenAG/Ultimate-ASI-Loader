#include "mtlx.hpp"
#include "../core/log.hpp"
#include "../core/pe.hpp"
#include "../core/strings.hpp"
#include <windows.h>
#include <winternl.h>
#include <cstring>
#include <string>
#include <vector>

namespace ual::startup::mtlx
{
    namespace
    {
        constexpr size_t kPatchSize = 5; // jmp rel32

        struct Patch
        {
            BYTE* at;
            BYTE patched[kPatchSize];
            BYTE original[kPatchSize];
        };

        std::vector<Patch> g_patches;
        PVOID g_cookie = nullptr;

        // An aligned 8-byte compare-exchange, so threads running into the function see the old or the new bytes
        bool Swap(BYTE* at, const BYTE* expected, const BYTE* replacement)
        {
            DWORD protect;
            if (!VirtualProtect(at, kPatchSize, PAGE_EXECUTE_READWRITE, &protect)) return false;
            bool done = false;
            auto block = (volatile LONG64*)((uintptr_t)at & ~uintptr_t(7));
            size_t offset = at - (BYTE*)block;
            if (offset + kPatchSize <= 8)
            {
                LONG64 now = *block, want = now;
                if (!memcmp((BYTE*)&now + offset, expected, kPatchSize))
                {
                    memcpy((BYTE*)&want + offset, replacement, kPatchSize);
                    done = InterlockedCompareExchange64(block, want, now) == now;
                }
            }
            else if (!memcmp(at, expected, kPatchSize))
            {
                memcpy(at, replacement, kPatchSize);
                done = true;
            }
            VirtualProtect(at, kPatchSize, protect, &protect);
            FlushInstructionCache(GetCurrentProcess(), at, kPatchSize);
            return done;
        }

        std::vector<BYTE> ReadFileBytes(const std::wstring& path)
        {
            std::vector<BYTE> data;
            HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            if (file == INVALID_HANDLE_VALUE) return data;
            LARGE_INTEGER size;
            DWORD read = 0;
            if (GetFileSizeEx(file, &size) && size.QuadPart < 64 * 1024 * 1024)
            {
                data.resize((size_t)size.QuadPart);
                if (!ReadFile(file, data.data(), (DWORD)data.size(), &read, nullptr) || read != data.size()) data.clear();
            }
            CloseHandle(file);
            return data;
        }

        // Exports of kernel32 that start with a jump in memory but not on disk get the bytes from disk
        void Unpatch()
        {
            HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
            pe::Image img(k32);
            wchar_t path[MAX_PATH];
            DWORD n = GetModuleFileNameW(k32, path, MAX_PATH);
            if (!img.Valid() || !n || n >= MAX_PATH) return;
            auto file = ReadFileBytes(path);
            if (file.size() < sizeof(IMAGE_DOS_HEADER)) return;
            auto fileNt = (IMAGE_NT_HEADERS*)(file.data() + ((IMAGE_DOS_HEADER*)file.data())->e_lfanew);
            if ((BYTE*)fileNt + sizeof(IMAGE_NT_HEADERS) > file.data() + file.size() || fileNt->Signature != IMAGE_NT_SIGNATURE) return;
            auto onDisk = [&](DWORD rva) -> const BYTE* {
                auto sec = IMAGE_FIRST_SECTION(fileNt);
                for (WORD i = 0; i < fileNt->FileHeader.NumberOfSections; ++i)
                    if (rva >= sec[i].VirtualAddress && rva + kPatchSize <= sec[i].VirtualAddress + sec[i].SizeOfRawData &&
                        sec[i].PointerToRawData + (rva - sec[i].VirtualAddress) + kPatchSize <= file.size())
                        return file.data() + sec[i].PointerToRawData + (rva - sec[i].VirtualAddress);
                return nullptr;
            };

            // bytes the loader relocates can't come from the file as they are
            std::vector<DWORD> relocated;
            auto& relocs = img.nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
            for (auto block = img.At<IMAGE_BASE_RELOCATION>(relocs.VirtualAddress);
                 relocs.VirtualAddress && img.Contains(block, sizeof(*block)) && block->SizeOfBlock >= sizeof(*block) &&
                 (BYTE*)block < img.At<BYTE>(relocs.VirtualAddress + relocs.Size);
                 block = (IMAGE_BASE_RELOCATION*)((BYTE*)block + block->SizeOfBlock))
            {
                auto entries = (WORD*)(block + 1);
                for (size_t i = 0; i < (block->SizeOfBlock - sizeof(*block)) / sizeof(WORD); ++i)
                    if (entries[i] >> 12) relocated.push_back(block->VirtualAddress + (entries[i] & 0xFFF));
            }
            auto touchesReloc = [&](DWORD rva) {
                for (DWORD r : relocated)
                    if (r + 4 > rva && r < rva + kPatchSize) return true;
                return false;
            };

            auto& dir = img.nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            auto exports = img.At<IMAGE_EXPORT_DIRECTORY>(dir.VirtualAddress);
            auto functions = img.At<DWORD>(exports->AddressOfFunctions);
            auto names = img.At<DWORD>(exports->AddressOfNames);
            auto ordinals = img.At<WORD>(exports->AddressOfNameOrdinals);
            for (DWORD i = 0; i < exports->NumberOfNames; ++i)
            {
                DWORD rva = functions[ordinals[i]];
                if (rva >= dir.VirtualAddress && rva < dir.VirtualAddress + dir.Size) continue; // forwarded
                BYTE* live = img.At<BYTE>(rva);
                const BYTE* original = onDisk(rva);
                if (!original || live[0] != 0xE9 || original[0] == 0xE9 || touchesReloc(rva)) continue;
                Patch p{ live };
                memcpy(p.patched, live, kPatchSize);
                memcpy(p.original, original, kPatchSize);
                if (!Swap(live, p.patched, p.original)) continue;
                UAL_LOG("MTLX.dll: kernel32!%s jumps to %s, original bytes back while it runs", img.At<const char>(names[i]),
                        log::Describe(live + kPatchSize + *(INT32*)(p.patched + 1)).c_str());
                g_patches.push_back(p);
            }
        }

        // The patches return where nobody changed the bytes since
        void Repatch()
        {
            int back = 0;
            for (auto& p : g_patches)
                back += Swap(p.at, p.original, p.patched);
            UAL_LOG("MTLX.dll unloaded, %d of %d kernel32 patches back", back, (int)g_patches.size());
            g_patches.clear();
        }

        struct DllNotificationData
        {
            ULONG Flags;
            PCUNICODE_STRING FullDllName;
            PCUNICODE_STRING BaseDllName;
            PVOID DllBase;
            ULONG SizeOfImage;
        };

        VOID CALLBACK OnDllNotification(ULONG reason, const DllNotificationData* data, PVOID)
        {
            if (!data || !data->BaseDllName || !data->BaseDllName->Buffer) return;
            std::wstring_view name(data->BaseDllName->Buffer, data->BaseDllName->Length / sizeof(wchar_t));
            if (!IEquals(name, L"MTLX.dll")) return;
            if (reason == 1 /* loaded */) Unpatch();
            else if (reason == 2 /* unloaded */) Repatch();
        }
    }

    void Install()
    {
        using RegisterFn = NTSTATUS(NTAPI*)(ULONG, PVOID, PVOID, PVOID*);
        auto reg = (RegisterFn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrRegisterDllNotification");
        if (reg && !g_cookie) reg(0, (PVOID)OnDllNotification, nullptr, &g_cookie);
    }
}
