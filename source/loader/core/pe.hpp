#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include <string_view>

namespace ual::pe
{
    struct Image
    {
        uintptr_t base = 0;
        IMAGE_NT_HEADERS* nt = nullptr;
        uintptr_t end = 0; // bounds check for anything read from the image

        explicit Image(HMODULE module);
        bool Valid() const { return nt != nullptr; }
        bool Contains(const void* p, size_t size = 1) const { return (uintptr_t)p >= base && (uintptr_t)p + size <= end; }
        template<class T>
        T* At(uintptr_t rva) const { return reinterpret_cast<T*>(base + rva); }
    };

    // size is rounded up to whole pages.
    void ForEachSection(HMODULE module, std::initializer_list<std::string_view> names,
                        const std::function<void(IMAGE_SECTION_HEADER*, uintptr_t start, size_t size)>& f);

    struct ImportSlot
    {
        std::string_view dll;   // lower case; "" for the section scan fallback
        const char* name;       // null if by ordinal or unknown
        WORD ordinal;           // 0 if by name
        void** slot;            // IAT entry
    };

    // Bound or stripped exes without a name table give only the slot. f returns false to stop.
    void ForEachImport(const Image& image, const std::function<bool(const ImportSlot&)>& f);

    // Works on read-only memory. Returns the old value.
    void* WritePointer(void** where, void* value);
}
