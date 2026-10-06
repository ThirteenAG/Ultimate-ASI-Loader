#pragma once
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
namespace cxxsnippets
{
// A memory write of a snippet: the bytes it replaced.
struct WriteRecord
{
    uintptr_t address;
    std::vector<uint8_t> original;
};
struct BuiltinOwner
{
    std::mutex mutex;
    std::recursive_mutex initializationMutex;
    std::map<size_t, bool> initialized;
    // Releases the static-local initialization locks the calling thread holds, after its initializer crashed.
    void ReleaseStaticLocks();
    std::vector<std::pair<void *, void (*)(void *)>> objects;
    std::vector<WriteRecord> writes;
    // Freed with the owner, like the module's code. Patched instructions may still be
    // jumping through these stubs after Unload().
    std::vector<std::pair<void *, void (*)(void *)>> retained;
    ~BuiltinOwner()
    {
        Release();
        for (auto i = retained.rbegin(); i != retained.rend(); ++i)
            i->second(i->first);
    }
    void Retain(void *object, void (*destroy)(void *))
    {
        std::lock_guard<std::mutex> lock(mutex);
        retained.push_back({object, destroy});
    }
    // Newest first.
    void Release()
    {
        std::vector<std::pair<void *, void (*)(void *)>> list;
        {
            std::lock_guard<std::mutex> lock(mutex);
            list.swap(objects);
        }
        for (auto i = list.rbegin(); i != list.rend(); ++i)
            i->second(i->first);
    }
    // Saves [address, address + size) before a write. Already saved ranges keep their oldest
    // bytes, which are the ones restored.
    void Remember(uintptr_t address, size_t size);
    // Restores everything Remember saved, newest first.
    void RevertWrites();
    void Own(void *object, void (*release)(void *))
    {
        std::lock_guard<std::mutex> lock(mutex);
        objects.push_back({object, release});
    }
};
std::map<std::string, std::string> HeaderSources();
void *BuiltinSymbol(const std::string &name);
} // namespace cxxsnippets
