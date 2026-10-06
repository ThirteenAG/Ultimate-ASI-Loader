// ual_host_launcher.exe: imports nothing but its game DLL, like the .NET and UWP
// launchers whose game code is in DLLs (GTA SA UWP, Cuphead UWP). No C runtime.
extern "C" __declspec(dllimport) int __stdcall GameMain();

extern "C" int LauncherMain()
{
    return GameMain();
}
