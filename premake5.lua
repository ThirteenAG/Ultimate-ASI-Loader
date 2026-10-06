-- Ultimate ASI Loader build script
--
--   premake5 vs2026 [--with-version=X.Y.Z]
--   msbuild build\Ultimate-ASI-Loader.slnx -p:Configuration=Release -p:Platform=Win32   (or x64)
--
-- Output goes to bin\<Win32|x64>\<config>\: dinput8.dll, scripts\*.asi, VirtualFileServer.exe
-- and tests\. Packaging is tools\package.ps1.

newoption {
   trigger     = "with-version",
   value       = "STRING",
   description = "Current UAL version",
   default     = "8.0.0",
}

-- version resource ---------------------------------------------------------

local function parse_version(s)
   local t = {}
   for i in s:gmatch("([^.]+)") do
      t[#t + 1] = tonumber((i:gsub("%D+", ""))) or 0
   end
   while #t < 4 do t[#t + 1] = 0 end
   return math.min(t[1], 255), math.min(t[2], 255), math.min(t[3], 65535), math.min(t[4], 65535)
end

local major, minor, build, revision = parse_version(_OPTIONS["with-version"])

local githash = ""
local f = io.popen("git rev-parse --short HEAD")
if f then
   githash = f:read("*a"):gsub("%s+", "")
   f:close()
end

local fileVersion = major .. "." .. minor .. "." .. build
local productVersion = fileVersion .. "." .. revision .. (githash ~= "" and ("-" .. githash) or "")

-- helpers -------------------------------------------------------------------

-- settings shared by every shipped project
local function shipped_project(kindName)
   kind (kindName)
   language "C++"
   characterset "Unicode"
   files { "source/shared/Versioninfo.rc" }
   includedirs { "source/shared" }
   vpaths { ["shared/*"] = "source/shared/**" }
end

-- Single-platform project. It keeps both platforms so Visual Studio doesn't show it as
-- "unloaded", and builds nothing on the other one.
function only_platform(platform)
   filter ("platforms:not " .. platform)
      kind "None"
      buildcommands { "echo %{prj.name}: " .. platform .. " only, nothing to build" } -- msbuild warns without a command
      rebuildcommands { "echo %{prj.name}: " .. platform .. " only, nothing to build" }
      cleancommands { "echo %{prj.name}: " .. platform .. " only, nothing to clean" }
   filter {}
end

-- workspace -----------------------------------------------------------------

workspace "Ultimate-ASI-Loader"
   configurations { "Release", "Debug" }
   platforms { "Win32", "x64" }
   location "build"
   startproject "Ultimate-ASI-Loader"
   cppdialect "C++latest"
   exceptionhandling "SEH"
   buildoptions { "/utf-8" } -- including string literals
   staticruntime "On"
   targetdir "bin/%{cfg.platform}/%{cfg.buildcfg}"

   defines { "rsc_CompanyName=\"ThirteenAG\"" }
   -- no ';' here, MSBuild separates preprocessor definitions with it
   defines { "rsc_LegalCopyright=\"Copyright (C) ThirteenAG. MIT License\"" }
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{cfg.buildtarget.name}\"" }
   defines { "rsc_FileDescription=\"Ultimate ASI Loader\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/Ultimate-ASI-Loader\"" }
   defines { "rsc_FileVersion_MAJOR=" .. major, "rsc_FileVersion_MINOR=" .. minor, "rsc_FileVersion_BUILD=" .. build, "rsc_FileVersion_REVISION=" .. revision }
   defines { "rsc_FileVersion=\"" .. fileVersion .. "\"", "rsc_ProductVersion=\"" .. productVersion .. "\"" }
   defines { "rsc_GitSHA1=\"" .. githash .. "\"", "rsc_GitSHA1W=L\"" .. githash .. "\"" }

   filter "platforms:Win32"
      architecture "x86"
   filter "platforms:x64"
      architecture "x86_64" -- code checks the compiler's _WIN64
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
      symbols "On" -- the release zips embed the PDB and crash reports resolve symbols from it
   filter {}

-- loader --------------------------------------------------------------------

group "loader"

-- Loader sources without entry point, export tables and resources, shared with the unit tests.
-- p is the repo root prefix, v the Visual Studio folder ("" for the project root).
function ual_loader_sources(p, v)
   v = v or ""
   -- source\loader\core\x.cpp shows as core\x.cpp in Solution Explorer. Patterns must not overlap.
   vpaths { [v .. "*"] = p .. "source/loader/**" }
   vpaths { [v .. "compat/*"] = p .. "source/compat/**" }
   vpaths { [v .. "external/*"] = p .. "external/**" }
   defines { "_CRT_SECURE_NO_WARNINGS" }
   defines { "ISOLATION_AWARE_ENABLED=1" } -- task dialogs use the loader's comctl32 v6 manifest
   includedirs { p .. "source/loader", p .. "external" }
   files { p .. "source/loader/core/*.hpp", p .. "source/loader/core/*.cpp" }          -- shared helpers, settings
   files { p .. "source/loader/crash/*.hpp", p .. "source/loader/crash/*.cpp" }        -- crash reports
   files { p .. "source/loader/plugins/*.hpp", p .. "source/loader/plugins/*.cpp" }    -- ASI plugin loading
   files { p .. "source/loader/proxy/*.hpp", p .. "source/loader/proxy/*.cpp", p .. "source/loader/proxy/*.inl" } -- forwarding to the original DLL
   files { p .. "source/loader/startup/*.hpp", p .. "source/loader/startup/*.cpp" }    -- loading at the game's entry point
   files { p .. "source/loader/ui/*.hpp", p .. "source/loader/ui/*.cpp" }              -- dialogs
   files { p .. "source/loader/vfs/*.hpp", p .. "source/loader/vfs/*.cpp" }            -- update folders, zip packages, virtual files
   files { p .. "source/loader/cxx/*.hpp", p .. "source/loader/cxx/*.cpp" }            -- .cxx snippets, engine in project cxxsnippets
   links { "cxxsnippets", "psapi", "user32" }

   includedirs { p .. "external/injector/minhook/include", p .. "external/miniz", p .. "source/shared" }
   files { p .. "external/injector/minhook/include/*.h", p .. "external/injector/minhook/src/**.h", p .. "external/injector/minhook/src/**.c" }
   files { p .. "external/miniz/miniz*.c", p .. "external/miniz/miniz*.h" } -- miniz_export.h: source/shared

   filter "platforms:Win32"
      -- backwards compatibility (Win32 games)
      files { p .. "source/compat/wndmode/*.hpp", p .. "source/compat/wndmode/*.cpp" }  -- window mode, wndmode.ini
      files { p .. "source/compat/d3d8/*.hpp", p .. "source/compat/d3d8/*.cpp" }        -- Direct3D8DisableMaximizedWindowedModeShim
      files { p .. "source/compat/vorbisfile/*.hpp", p .. "source/compat/vorbisfile/*.h", p .. "source/compat/vorbisfile/*.cpp", p .. "source/compat/vorbisfile/*.c" } -- built-in vorbisFile.dll
      files { p .. "external/d3d8to9/source/*.hpp", p .. "external/d3d8to9/source/*.cpp" }
      -- official libogg and libvorbis for the built-in vorbisFile.dll
      includedirs { p .. "external/ogg/include", p .. "external/vorbis/include", p .. "external/vorbis/lib" }
      files { p .. "external/ogg/src/bitwise.c", p .. "external/ogg/src/framing.c" }
      for _, n in ipairs { "analysis", "bitrate", "block", "codebook", "envelope", "floor0", "floor1", "info", "lookup", "lpc", "lsp",
                           "mapping0", "mdct", "psy", "registry", "res0", "sharedbook", "smallft", "synthesis", "window" } do
         files { p .. "external/vorbis/lib/" .. n .. ".c" }
      end
      links { "d3d9", "delayimp" }
      linkoptions { "/DELAYLOAD:d3d9.dll" } -- only d3d8to9 uses it
   filter { "platforms:Win32", "files:**.c" } -- third-party C: libogg, libvorbis, vorbisfile.c, minhook, miniz
      disablewarnings { "4244", "4267", "4305", "4996", "4018", "4100", "4189", "4554", "4706" }
   filter { "platforms:Win32", "configurations:Release" }
      defines { "D3D8TO9NOLOG" }
   filter {}

end

-- .cxx snippet engine: compiles C++ to machine code in the game's process, with the hooking
-- libraries the snippets bind to
project "cxxsnippets"
   kind "StaticLib"
   language "C++"
   targetdir "build/lib/%{cfg.platform}/%{cfg.buildcfg}"
   defines { "INJECTOR_GVM_DUMMY", "INJECTOR_GVM_OWN_DETECT" } -- the sources define NOMINMAX and WIN32_LEAN_AND_MEAN themselves
   files { "source/loader/cxx/cxxsnippets/*.hpp", "source/loader/cxx/cxxsnippets/*.cpp" }
   vpaths { ["*"] = "source/loader/cxx/cxxsnippets/**" }
   vpaths { ["external/*"] = "external/**" }
   files { "external/Hooking.Patterns/Hooking.Patterns.cpp", "external/injector/safetyhook/src/**.cpp", "external/injector/zydis/Zydis.c" }
   removefiles { "external/injector/safetyhook/src/os.linux.cpp" }
   includedirs { "external/Hooking.Patterns", "external/injector/include", "external/injector/safetyhook/include", "external/injector/zydis" }
   filter "files:**.c"
      disablewarnings { "4244", "4267" }
   filter {}

project "Ultimate-ASI-Loader"
   shipped_project "SharedLib"
   targetname "dinput8"
   targetextension ".dll"
   ual_loader_sources ""
   files { "source/loader/main.cpp" }
   filter "platforms:Win32"
      files { "source/loader/x86.def", "source/loader/resources/UALx86.rc" }
      files { "source/compat/xlive/xliveless.h", "source/compat/xlive/xliveless.cpp", "source/compat/xlive/xliveless.rc" } -- xlive.dll, Games for Windows Live replacement
   filter "platforms:x64"
      files { "source/loader/x64.def", "source/loader/resources/UALx64.rc" }
   filter {}

project "VirtualFileServer"
   shipped_project "ConsoleApp"
   only_platform "x64" -- helper process for the x86 loader
   files { "source/VirtualFileServer/VirtualFileServer.cpp", "source/loader/resources/UALx64.rc" }
   vpaths { ["*"] = "source/VirtualFileServer/**" }
   vpaths { ["resources/*"] = "source/loader/resources/**" }

-- demo plugins ----------------------------------------------------------------
-- source/plugins/<name>/<name>.cpp -> bin\<platform>\<config>\scripts\<name>.asi and .ini

local function demo_plugin(name, opts)
   opts = opts or {}
   project (name)
      shipped_project "SharedLib"
      targetdir "bin/%{cfg.platform}/%{cfg.buildcfg}/scripts"
      targetextension ".asi"
      files { "source/plugins/" .. name .. "/*.cpp", "source/plugins/" .. name .. "/*.ini" }
      vpaths { ["*"] = "source/plugins/" .. name .. "/**" }
      vpaths { ["external/*"] = "external/**" }
      if opts.only then only_platform(opts.only) end
      if opts.hooking then -- safetyhook and Hooking.Patterns compiled in
         files { "external/Hooking.Patterns/Hooking.Patterns.cpp", "external/injector/safetyhook/src/**.cpp", "external/injector/zydis/Zydis.c" }
         removefiles { "external/injector/safetyhook/src/os.linux.cpp" }
         includedirs { "external/Hooking.Patterns", "external/injector/safetyhook/include", "external/injector/zydis" }
         filter "files:**.c"
            disablewarnings { "4244", "4267" }
         filter {}
      end
      if os.isfile("source/plugins/" .. name .. "/" .. name .. ".ini") then
         postbuildcommands { '{COPYFILE} "%{wks.location}/../source/plugins/' .. name .. '/' .. name .. '.ini" "%{cfg.targetdir}"' }
      end
end

group "plugins"

demo_plugin "MessageBox"
   filter "platforms:x64"
      targetname "MessageBox_x64"
   filter {}

demo_plugin("ExeUnprotect", { only = "Win32" })
demo_plugin("PluginTemplate", { hooking = true })
demo_plugin "VirtualFiles"
demo_plugin("FrameLimiter", { hooking = true })

-- tests -------------------------------------------------------------------------

group "tests"
include "tests"

group ""
