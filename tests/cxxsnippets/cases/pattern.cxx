#include <injector/injector.hpp>
#include <Hooking.Patterns.h>
void Init()
{
    injector::WriteMemory<uint8_t>(hook::get_pattern("74 10 53 53 6A 1B"), 0xEB, true);
}
void Shutdown() {}
