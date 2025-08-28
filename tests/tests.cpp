#include <windows.h>
#include <mutex>
#include <stacktrace>
#include <fstream>
#include <filesystem>

HMODULE hUAL = NULL;

std::filesystem::path GetModulePath(HMODULE hModule)
{
    static constexpr auto INITIAL_BUFFER_SIZE = MAX_PATH;
    static constexpr auto MAX_ITERATIONS = 7;

    std::u16string ret;
    std::filesystem::path pathret;
    auto bufferSize = INITIAL_BUFFER_SIZE;
    for (size_t iterations = 0; iterations < MAX_ITERATIONS; ++iterations)
    {
        ret.resize(bufferSize);
        size_t charsReturned = 0;
        charsReturned = GetModuleFileNameW(hModule, (LPWSTR)&ret[0], bufferSize);
        if (charsReturned < ret.length())
        {
            ret.resize(charsReturned);
            pathret = ret;
            return pathret;
        }
        else
        {
            bufferSize *= 2;
        }
    }

    return {};
}

void Init()
{
    // ASI Loading
    {
        auto mp = GetModulePath(NULL);
        auto filename = mp.filename();
        filename.replace_extension(L".txt");
        std::ofstream outfile(filename, std::ios::out | std::ios::binary);
        outfile.flush();
        outfile.close();
    }

    // Overload (folder)
    {
        std::ifstream ifs;
        ifs.open(L"input.bin", std::ifstream::in);
        std::string line;
        std::getline(ifs, line);
        std::ofstream outfile(line, std::ios::out | std::ios::binary);
        outfile.flush();
        outfile.close();
    }

    // Overload (virtual file)
    {
        bool (WINAPI* GetOverloadPathW)(wchar_t* out, size_t out_size) = nullptr;
        bool (WINAPI* AddVirtualFileForOverloadW)(const wchar_t* virtualPath, const uint8_t* data, size_t size, int priority) = nullptr;

        GetOverloadPathW = (decltype(GetOverloadPathW))GetProcAddress(hUAL, "GetOverloadPathW");
        AddVirtualFileForOverloadW = (decltype(AddVirtualFileForOverloadW))GetProcAddress(hUAL, "AddVirtualFileForOverloadW");

        unsigned char hexData[28] = { // virtual_file_test_passed.txt
            0x76, 0x69, 0x72, 0x74, 0x75, 0x61, 0x6C, 0x5F, 0x66, 0x69, 0x6C, 0x65, 0x5F, 0x74, 0x65, 0x73,
            0x74, 0x5F, 0x70, 0x61, 0x73, 0x73, 0x65, 0x64, 0x2E, 0x74, 0x78, 0x74,
        };

        // calling AddVirtualFile for the same path should append the data
        AddVirtualFileForOverloadW(L"input2.bin", &hexData[0], 14, 1000);
        AddVirtualFileForOverloadW(L"input2.bin", &hexData[14], 14, 1000);

        std::ifstream ifs;
        ifs.open(L"input2.bin", std::ifstream::in);
        std::string line;
        std::getline(ifs, line);
        std::ofstream outfile(line, std::ios::out | std::ios::binary);
        outfile.flush();
        outfile.close();

        AddVirtualFileForOverloadW(L"input3.bin", &hexData[14], 14, 1000);
    }

    // Overload (zip file)
    {
        std::ifstream ifs;
        ifs.open(L"input3.bin", std::ifstream::in);
        std::string line;
        std::getline(ifs, line);
        std::ofstream outfile(line, std::ios::out | std::ios::binary);
        outfile.flush();
        outfile.close();
    }

    // Overload (virtual path)
    {
        bool (WINAPI* GetOverloadPathW)(wchar_t* out, size_t out_size) = nullptr;
        bool (WINAPI* AddVirtualPathForOverloadW)(const wchar_t* originalPath, const wchar_t* virtualPath, int priority) = nullptr;
        void (WINAPI* RemoveVirtualPathFromOverloadW)(const wchar_t* originalPath) = nullptr;

        GetOverloadPathW = (decltype(GetOverloadPathW))GetProcAddress(hUAL, "GetOverloadPathW");
        AddVirtualPathForOverloadW = (decltype(AddVirtualPathForOverloadW))GetProcAddress(hUAL, "AddVirtualPathForOverloadW");
        RemoveVirtualPathFromOverloadW = (decltype(RemoveVirtualPathFromOverloadW))GetProcAddress(hUAL, "RemoveVirtualPathFromOverloadW");

        AddVirtualPathForOverloadW(L"input3.bin", L"storage/input3.bin", 1001); // higher priority than input3.bin above to work
        AddVirtualPathForOverloadW(L"input4.bin", L"storage/input4.bin", 1001);
        RemoveVirtualPathFromOverloadW(L"input4.bin");

        std::ifstream ifs;
        ifs.open(L"input3.bin", std::ifstream::in);
        std::string line;
        std::getline(ifs, line);

        if (line == "virtual_path_test_passed.txt")
        {
            std::ifstream ifs2;
            ifs2.open(L"input4.bin", std::ifstream::in);
            std::string line2;
            std::getline(ifs2, line2);
            std::ofstream outfile2(line2, std::ios::out | std::ios::binary);
            outfile2.flush();
            outfile2.close();
        }
    }

    ExitProcess(0);
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        Init();
    });
}

bool IsModuleUAL(HMODULE mod)
{
    if (GetProcAddress(mod, "IsUltimateASILoader") != NULL || (GetProcAddress(mod, "DirectInput8Create") != NULL && GetProcAddress(mod, "DirectSoundCreate8") != NULL && GetProcAddress(mod, "InternetOpenA") != NULL))
        return true;
    return false;
}

bool IsUALPresent()
{
    for (const auto& entry : std::stacktrace::current())
    {
        HMODULE hModule = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)entry.native_handle(), &hModule))
        {
            if (IsModuleUAL(hModule))
            {
                hUAL = hModule;
                return true;
            }
        }
    }
    return false;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (!IsUALPresent())
        {
            InitializeASI();
        }
    }
    return TRUE;
}
