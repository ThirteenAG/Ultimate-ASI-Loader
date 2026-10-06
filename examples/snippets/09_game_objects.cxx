// 09 - Game data: structs, pointer chains, and calling game functions
//
// Use for: reading or changing game state (player, camera, settings) and
// calling functions of the game yourself.
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <Hooking.Patterns.h>

// Describe only the fields you need. Fill the gaps with byte arrays so that
// every field lands on the offset you found in the disassembler.
struct Vector3
{
    float x, y, z;
};

struct Camera
{
    uint8_t unknown0[0x10];
    Vector3 position;      // +0x10
    uint8_t unknown1[0x8]; // +0x1C
    float fov;             // +0x24
    float aspectRatio;     // +0x28
};

struct Player
{
    void **vtable;   // +0x00 (classes with virtual methods start with it)
    int health;      // +0x04
    int maxHealth;   // +0x08
    Camera *camera;  // +0x0C (x86 layout: pointers are 8 bytes on x64)
};

// Check the layout at compile time.
static_assert(offsetof(Camera, fov) == 0x24, "Camera::fov");

// Calling game functions: a function pointer type with the right calling convention.
typedef void (*Log_t)(char const *text);                    // __cdecl
typedef int(__stdcall *GetSetting_t)(char const *name);     // __stdcall
#ifdef _WIN64
typedef void (*SetHealth_t)(Player *self, int health);      // a method (x64)
#else
typedef void(__fastcall *SetHealth_t)(Player *self, void *edx, int health); // __thiscall method (x86)
#endif

Log_t gameLog = nullptr;
SetHealth_t setHealth = nullptr;
Player **localPlayer = nullptr;

// A pointer chain like [[game.exe+0x123456]+0x0C]+0x24, with checks at every step:
// the game may not have created the objects yet.
float *FindFov()
{
    if (!localPlayer || !*localPlayer)
        return nullptr;
    Camera *camera = (*localPlayer)->camera;
    return camera ? &camera->fov : nullptr;
}

// Structs passed or returned BY VALUE are not supported as parameters of your
// functions. Use the pieces instead. On x86 a 12-byte struct argument is
// pushed like 3 separate 4-byte arguments, so
//     void __cdecl Teleport(Vector3 where)
// can be called (and hooked) as
//     void Teleport(float x, float y, float z)
// On x64 a struct bigger than 8 bytes is passed by pointer, so declare it as Vector3*.
#ifdef _WIN64
typedef void (*Teleport_t)(Vector3 *where);
#else
typedef void (*Teleport_t)(float x, float y, float z);
#endif

void Init()
{
    hook::pattern log("55 8B EC 81 EC 00 04 00 00 8D 45 0C");
    if (!log.empty())
        gameLog = (Log_t)log.get_first();

    hook::pattern health("8B 44 24 04 89 41 04 C2 04 00");
    if (!health.empty())
        setHealth = (SetHealth_t)health.get_first();

    hook::pattern player("8B 0D ? ? ? ? 85 C9 74 ? 8B 41 04");
    if (!player.empty())
        localPlayer = *(Player ***)player.get_first(2);

    if (gameLog)
        gameLog("snippet loaded");

    float *fov = FindFov();
    if (fov)
        *fov = 90.0f;

    if (setHealth && localPlayer && *localPlayer)
    {
#ifdef _WIN64
        setHealth(*localPlayer, (*localPlayer)->maxHealth);
#else
        setHealth(*localPlayer, nullptr, (*localPlayer)->maxHealth);
#endif
    }
}
