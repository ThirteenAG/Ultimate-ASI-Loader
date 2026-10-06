-- Test projects, included by the root premake5.lua. Outputs go to bin\<Win32|x64>\<config>\tests:
--   probe.asi                instrumented plugin (tests/probe/probe.cpp)
--   probe_missingdep.asi     plugin importing a DLL that is never deployed
--   ual_missing_dependency.dll
--   ual_fake_original.dll    stand-in for an original DLL (<name>Hooked.dll tests)
--   stubs\vorbis.dll         the game's vorbis.dll, used by the x86 built-in vorbisfile
--   ual_host.exe             test "game" that loads proxies at run time
--   ual_host_<proxy>.exe     test "game" that statically imports <proxy>.dll
--   ual_unit_tests.exe       white-box unit tests, compiled with the loader sources
--   ual_tests.exe            integration runner, x64 only, tests both architectures
--   ual_wndmode_tests.exe    window mode tests (Win32)

local root = _MAIN_SCRIPT_DIR
local testsdir = "bin/%{cfg.platform}/%{cfg.buildcfg}/tests"

-- proxy name -> SDK import library for ual_host_<proxy>.exe
local static_hosts = {
   { name = "dinput8",     lib = "dinput8" },
   { name = "d3d9",        lib = "d3d9" },
   { name = "d3d10",       lib = "d3d10" },
   { name = "d3d11",       lib = "d3d11" },
   { name = "d3d12",       lib = "d3d12" },
   { name = "dxgi",        lib = "dxgi" },
   { name = "dsound",      lib = "dsound" },
   { name = "winmm",       lib = "winmm" },
   { name = "version",     lib = "version" },
   { name = "wininet",     lib = "wininet" },
   { name = "winhttp",     lib = "winhttp" },
   { name = "xinput1_4",   lib = "xinput" },
   { name = "xinput9_1_0", lib = "xinput9_1_0" },
   { name = "xinputuap",   lib = "xinputuap" },
   { name = "ddraw",       lib = "ddraw",   x86only = true },
   { name = "msacm32",     lib = "msacm32", x86only = true },
   { name = "msvfw32",     lib = "vfw32",   x86only = true },
}

local function test_project(name, kindName)
   project (name)
      kind (kindName)
      language "C++"
      characterset "Unicode"
      targetdir (root .. "/" .. testsdir)
      defines { "_CRT_SECURE_NO_WARNINGS" }
      vpaths { ["*"] = root .. "/tests/**" } -- tests\host\x.cpp shows as host\x.cpp
      filter "configurations:Release"
         symbols "On"
      filter {}
end

test_project("ual_probe", "SharedLib")
   targetname "probe"
   targetextension ".asi"
   files { root .. "/tests/probe/probe.cpp", root .. "/tests/common/report.hpp" }

test_project("ual_missing_dependency", "SharedLib")
   defines { "UAL_MISSING_DEPENDENCY_DLL" }
   files { root .. "/tests/probe/missing_dependency.cpp" }

test_project("ual_probe_missingdep", "SharedLib")
   targetname "probe_missingdep"
   targetextension ".asi"
   files { root .. "/tests/probe/missing_dependency.cpp" }
   links { "ual_missing_dependency" }

test_project("ual_fake_original", "SharedLib")
   files { root .. "/tests/probe/fake_original.cpp", root .. "/tests/probe/fake_original.def" }

-- stand-in for the game's vorbis.dll, used by the x86 built-in vorbisfile
test_project("ual_stub_vorbis", "SharedLib")
   only_platform "Win32"
   targetname "vorbis"
   targetdir (root .. "/" .. testsdir .. "/stubs")
   files { root .. "/tests/probe/stub_vorbis.cpp", root .. "/tests/probe/stub_vorbis.def" }

test_project("ual_host_core", "StaticLib")
   targetdir (root .. "/build/lib/%{cfg.platform}/%{cfg.buildcfg}")
   files { root .. "/tests/host/host.hpp", root .. "/tests/host/host_main.cpp", root .. "/tests/host/scenarios_*.cpp", root .. "/tests/common/*.hpp" }

local function host_project(name, define, lib)
   test_project(name, "ConsoleApp")
      entrypoint "wmainCRTStartup"
      files { root .. "/tests/host/host_imports.cpp" }
      links { "ual_host_core", "ole32", lib }
      linkoptions { "/WHOLEARCHIVE:ual_host_core.lib" } -- keep the self-registering scenarios
      if define then defines { define } end
end

host_project("ual_host")

-- protection stub layouts, entry point and import directory in a section after .text
--   ual_host_stub.exe          Origin style: loads the loader at run time
--   ual_host_stub_dinput8.exe  Denuvo style: imports the loader (dinput8.dll)
for _, v in ipairs { { "ual_host_stub" }, { "ual_host_stub_dinput8", "UAL_HOST_PROXY_dinput8", "dinput8" } } do
   host_project(v[1], v[2], v[3])
      files { root .. "/tests/host/stub_entry.cpp" }
      entrypoint "StubEntry"
      linkoptions { "/SECTION:.rdata,ER" }
end

-- old exe with no executable section that relies on DEP being off (Max Payne)
host_project("ual_host_noexec_dinput8", "UAL_HOST_PROXY_dinput8", "dinput8")
   linkoptions { "/SECTION:.text,!E", "/NXCOMPAT:NO" }

-- launcher importing only its game DLL, like .NET/UWP launchers
test_project("ual_launcher_game", "SharedLib")
   files { root .. "/tests/host/launcher_game.cpp", root .. "/tests/common/report.hpp" }
test_project("ual_host_launcher", "ConsoleApp")
   files { root .. "/tests/host/launcher_main.cpp" }
   entrypoint "LauncherMain"

   buffersecuritycheck "Off"
   linkoptions { "/NODEFAULTLIB" }
   links { "ual_launcher_game" }
for _, h in ipairs(static_hosts) do
   host_project("ual_host_" .. h.name, "UAL_HOST_PROXY_" .. h.name, h.lib)
   if h.x86only then only_platform "Win32" end
end

test_project("ual_unit_tests", "ConsoleApp")
   entrypoint "wmainCRTStartup"
   ual_loader_sources(root .. "/", "loader/")
   files { root .. "/tests/unit/*.cpp", root .. "/tests/common/testlib.hpp" }

test_project("ual_tests", "ConsoleApp")
   only_platform "x64"
   entrypoint "wmainCRTStartup"
   includedirs { root .. "/external/miniz", root .. "/source/shared" }
   files { root .. "/tests/runner/*.cpp", root .. "/tests/runner/*.hpp", root .. "/tests/common/*.hpp", root .. "/external/miniz/miniz*.c" }
   vpaths { ["external/*"] = root .. "/external/**" }
   links { "ole32", "oleaut32" }

-- source/compat/wndmode, Win32 like the feature
test_project("ual_wndmode_tests", "ConsoleApp")
   only_platform "Win32"
   entrypoint "wmainCRTStartup"
   defines { "NOMINMAX" }
   includedirs { root .. "/source/compat/wndmode", root .. "/external/injector/minhook/include" }
   files { root .. "/tests/wndmode/*.cpp", root .. "/tests/common/testlib.hpp" }
   files { root .. "/source/compat/wndmode/*.cpp", root .. "/external/injector/minhook/src/**.c" }
   vpaths { ["source/*"] = root .. "/source/**" }
   vpaths { ["external/*"] = root .. "/external/**" }

-- .cxx snippet engine tests. case-<name>.dll builds the same snippets with MSVC for comparison.
test_project("ual_cxxsnippets_tests", "ConsoleApp")
   defines { "NOMINMAX", "WIN32_LEAN_AND_MEAN" }
   defines { 'CXXSNIPPETS_TEST_ROOT="' .. (root .. "/tests/cxxsnippets"):gsub("\\", "/") .. '"' }
   includedirs { root .. "/source/loader/cxx/cxxsnippets" }
   files { root .. "/tests/cxxsnippets/tests.cpp", root .. "/tests/cxxsnippets/usecases.cpp", root .. "/tests/cxxsnippets/differential.cpp",
           root .. "/tests/cxxsnippets/corpus.cpp", root .. "/tests/cxxsnippets/exports.def" }
   links { "cxxsnippets", "psapi", "user32" }

-- examples/snippets/*.cxx must stay valid for MSVC with the real headers. Compiled only.
test_project("snippet-examples-native", "StaticLib")
   defines { "NOMINMAX", "WIN32_LEAN_AND_MEAN" }
   files { root .. "/examples/snippets/*.cxx" }
   vpaths { ["examples/*"] = root .. "/examples/snippets/**" }
   linkoptions { "/IGNORE:4006" } -- every example defines Init()
   includedirs { root .. "/external/injector/include", root .. "/external/injector/safetyhook/include", root .. "/external/Hooking.Patterns" }

for _, name in ipairs { "minimal", "multiple", "threadsafe", "midhook", "vmthook", "dll", "pattern" } do
   test_project("case-" .. name, "SharedLib")
      defines { "NOMINMAX", "WIN32_LEAN_AND_MEAN", "INJECTOR_GVM_DUMMY", "INJECTOR_GVM_OWN_DETECT" }
      files { root .. "/tests/cxxsnippets/cases/" .. name .. ".cxx", root .. "/tests/cxxsnippets/native-shim.cpp", root .. "/tests/cxxsnippets/native-shim.def" }
      includedirs { root .. "/external/injector/include", root .. "/external/injector/safetyhook/include", root .. "/external/Hooking.Patterns" }
      links { "cxxsnippets", "psapi", "user32" }
end
