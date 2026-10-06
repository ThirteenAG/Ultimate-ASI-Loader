#include "pe.hpp"
#include <algorithm>
#include <cstring>

namespace ual::pe
{
    Image::Image(HMODULE module)
    {
        if (!module) return;
        base = (uintptr_t)module;
        auto dos = (IMAGE_DOS_HEADER*)base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
        auto ntHeaders = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return;
        nt = ntHeaders;
        // use the end of the last section; some packed executables report an odd SizeOfImage
        auto sec = IMAGE_FIRST_SECTION(nt);
        uintptr_t last = base + nt->OptionalHeader.SizeOfImage;
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
            size_t size = (std::max)(sec[i].SizeOfRawData, sec[i].Misc.VirtualSize);
            last = (std::max)(last, (uintptr_t)(base + sec[i].VirtualAddress + size));
        }
        end = last;
    }

    void ForEachSection(HMODULE module, std::initializer_list<std::string_view> names,
                        const std::function<void(IMAGE_SECTION_HEADER*, uintptr_t, size_t)>& f)
    {
        Image img(module);
        if (!img.Valid()) return;
        auto sec = IMAGE_FIRST_SECTION(img.nt);
        for (WORD i = 0; i < img.nt->FileHeader.NumberOfSections; ++i, ++sec)
        {
            char name[IMAGE_SIZEOF_SHORT_NAME + 1] = {};
            memcpy(name, sec->Name, IMAGE_SIZEOF_SHORT_NAME);
            if (std::find(names.begin(), names.end(), std::string_view(name)) == names.end()) continue;
            size_t size = (sec->Misc.VirtualSize + 4095) & ~size_t(4095);
            f(sec, img.base + sec->VirtualAddress, size);
        }
    }

    void ForEachImport(const Image& img, const std::function<bool(const ImportSlot&)>& f)
    {
        if (!img.Valid()) return;
        auto& dir = img.nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dir.VirtualAddress) return;
        for (auto desc = img.At<IMAGE_IMPORT_DESCRIPTOR>(dir.VirtualAddress); img.Contains(desc, sizeof(*desc)) && desc->Name; ++desc)
        {
            auto dllName = img.At<const char>(desc->Name);
            if (!img.Contains(dllName)) continue;
            char lower[256] = {};
            for (size_t i = 0; i < sizeof(lower) - 1 && dllName[i]; ++i) lower[i] = (char)tolower((unsigned char)dllName[i]);
            std::string_view dll(lower);

            auto iat = img.At<void*>(desc->FirstThunk);
            if (desc->OriginalFirstThunk)
            {
                auto thunk = img.At<IMAGE_THUNK_DATA>(desc->OriginalFirstThunk);
                for (size_t j = 0; img.Contains(&thunk[j], sizeof(thunk[j])) && img.Contains(&iat[j], sizeof(void*)) && thunk[j].u1.AddressOfData; ++j)
                {
                    ImportSlot s{ dll, nullptr, 0, &iat[j] };
                    if (IMAGE_SNAP_BY_ORDINAL(thunk[j].u1.Ordinal)) s.ordinal = (WORD)IMAGE_ORDINAL(thunk[j].u1.Ordinal);
                    else
                    {
                        auto byName = img.At<IMAGE_IMPORT_BY_NAME>((uintptr_t)thunk[j].u1.AddressOfData);
                        if (!img.Contains(byName, sizeof(*byName))) continue;
                        s.name = byName->Name;
                    }
                    if (!f(s)) return;
                }
            }
            else if (desc->FirstThunk) // bound exe without a name table, only addresses
            {
                for (size_t j = 0; img.Contains(&iat[j], sizeof(void*)) && iat[j]; ++j)
                    if (!f(ImportSlot{ dll, nullptr, 0, &iat[j] })) return;
            }
        }
    }

    void* WritePointer(void** where, void* value)
    {
        // Keep the execute bit only where it is present: under ACG (dynamic code prohibited) asking for
        // EXECUTE_READWRITE on a data page is refused, and an import table is a data page.
        MEMORY_BASIC_INFORMATION mbi{};
        bool executable = VirtualQuery(where, &mbi, sizeof(mbi)) && (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
        DWORD old;
        if (!VirtualProtect(where, sizeof(void*), executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE, &old)) return *where;
        void* prev = *where;
        *where = value;
        VirtualProtect(where, sizeof(void*), old, &old);
        return prev;
    }
}
