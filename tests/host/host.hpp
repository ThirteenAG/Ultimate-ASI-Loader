// The "game" for integration tests. ual_host_<proxy>.exe statically imports <proxy>.dll,
// ual_host.exe has no proxy import and uses load:<dll>. Each "verb:arg" action appends a
// report record. Actions are listed in host_main.cpp, scenarios live in scenarios_*.cpp.
#pragma once

#include <windows.h>
#include <cstdint>
#include <type_traits>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "../common/report.hpp"

namespace host
{
    // Exit code when a scenario watchdog detects a deadlock
    constexpr UINT kDeadlockExitCode = 0xDEAD10CC;

    struct UalApi
    {
        HMODULE module = nullptr;
        bool(WINAPI* IsUltimateASILoader)() = nullptr;
        bool(WINAPI* GetOverloadPathA)(char* out, size_t out_size) = nullptr;
        bool(WINAPI* GetOverloadPathW)(wchar_t* out, size_t out_size) = nullptr;
        bool(WINAPI* GetOverloadedFilePathA)(const char* path, char* out, size_t out_size) = nullptr;
        bool(WINAPI* GetOverloadedFilePathW)(const wchar_t* path, wchar_t* out, size_t out_size) = nullptr;
        bool(WINAPI* AddVirtualFileForOverloadA)(const char* path, const uint8_t* data, size_t size, int priority) = nullptr;
        bool(WINAPI* AddVirtualFileForOverloadW)(const wchar_t* path, const uint8_t* data, size_t size, int priority) = nullptr;
        void(WINAPI* RemoveVirtualFileFromOverloadA)(const char* path) = nullptr;
        void(WINAPI* RemoveVirtualFileFromOverloadW)(const wchar_t* path) = nullptr;
        bool(WINAPI* AddVirtualPathForOverloadA)(const char* original, const char* virt, int priority) = nullptr;
        bool(WINAPI* AddVirtualPathForOverloadW)(const wchar_t* original, const wchar_t* virt, int priority) = nullptr;
        void(WINAPI* RemoveVirtualPathFromOverloadA)(const char* original) = nullptr;
        void(WINAPI* RemoveVirtualPathFromOverloadW)(const wchar_t* original) = nullptr;

        bool Resolve();          // first loaded module exporting IsUltimateASILoader
        bool Complete() const;   // all exports found
    };

    class Host
    {
    public:
        std::wstring report;
        UalApi ual;
        int failedChecks = 0;
        int totalChecks = 0;
        std::string currentScenario;

        void Emit(ualtest::Record r);
        // Records ev=check and returns ok so callers can bail out
        bool Check(bool ok, const std::string& name, const std::string& detail = {});
        template<class A, class B>
        bool CheckEq(const A& a, const B& b, const std::string& name)
        {
            bool ok = (a == b);
            return Check(ok, name, ok ? std::string() : "got " + Str(a) + ", expected " + Str(b));
        }
        bool RequireUal();
        // Runner marks the test skipped, e.g. a legacy system DLL is missing
        void Skip(const std::string& reason)
        {
            ualtest::Record r;
            r["ev"] = "skip";
            r["reason"] = reason;
            Emit(r);
        }

        static std::string Str(const std::string& s) { return "\"" + s + "\""; }
        static std::string Str(const std::wstring& s) { return "\"" + ualtest::utf8(s) + "\""; }
        static std::string Str(const char* s) { return s ? "\"" + std::string(s) + "\"" : "null"; }
        static std::string Str(bool v) { return v ? "true" : "false"; }
        template<class T> static std::string Str(const T& v)
        {
            if constexpr (std::is_pointer_v<T>) { char b[32]; sprintf_s(b, "%p", (const void*)v); return b; }
            else return std::to_string(v);
        }
    };

    using ScenarioFn = void (*)(Host&, const std::wstring& args);
    std::map<std::string, ScenarioFn>& Scenarios();

    struct ScenarioReg
    {
        ScenarioReg(const char* name, ScenarioFn fn) { Scenarios()[name] = fn; }
    };

#define HOST_SCENARIO(name)                                                     \
    static void host_scenario_##name(::host::Host& host, const std::wstring& args); \
    static ::host::ScenarioReg host_scenario_reg_##name(#name, &host_scenario_##name); \
    static void host_scenario_##name([[maybe_unused]] ::host::Host& host, [[maybe_unused]] const std::wstring& args)

    std::wstring ExeDir();

    // Calls Sleep(0) from the stub section. Null outside ual_host_stub.exe.
    extern void (*g_stubCall)();
    std::wstring SystemDir();
    std::string Acp(const std::wstring& s);
    std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep);
    bool ReadAll(HANDLE h, std::string& out, DWORD chunk = 4096);
    std::string ReadFileW(const std::wstring& path, DWORD* err = nullptr);
    bool WriteFileW(const std::wstring& path, const std::string& data);
    HMODULE FindModuleByName(const wchar_t* fileName);
    bool AddressInModule(const void* addr, HMODULE mod);
}

// host_imports.cpp
extern "C" const char* ual_host_static_proxy();
extern "C" const void* ual_host_static_anchor();
