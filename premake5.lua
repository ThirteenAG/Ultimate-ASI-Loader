-- x86
workspace "Ultimate-ASI-Loader-x86"
   configurations { "Release", "Debug" }
   architecture "x86"
   location "build"
   
   defines { "rsc_CompanyName=\"ThirteenAG\"" }
   defines { "rsc_LegalCopyright=\"MIT License\""} 
   defines { "rsc_FileVersion=\"1.0.0.0\"", "rsc_ProductVersion=\"1.0.0.0\"" }
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{prj.name}.dll\"" }
   defines { "rsc_FileDescription=\"Ultimate ASI Loader\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/Ultimate-ASI-Loader\"" }
     
project "Ultimate-ASI-Loader-x86"
   kind "SharedLib"
   language "C++"
   targetdir "bin/x86/%{cfg.buildcfg}"
   targetname "dinput8"
   targetextension ".dll"
   
   includedirs { "source/x86" }
   includedirs { "external" }
   
   files { "source/x86/*.h", "source/x86/*.cpp" }
   files { "source/x86/x86.def" }
   files { "source/dllmain.cpp" }
   files { "source/resources/*.rc" }
   files { "external/d3d8to9/source/*.h", "external/d3d8to9/source/*.cpp" }
   files { "external/MemoryModule/*.h", "external/MemoryModule/*.c" }

   local dxsdk = os.getenv "DXSDK_DIR" or "Error: DXSDK_DIR not set!"
   includedirs { dxsdk .. "/include" }
   libdirs { dxsdk .. "/lib/x86" }
   
   characterset ("MBCS")
   
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG", "D3D8TO9NOLOG" }
      optimize "On"
      flags { "StaticRuntime" }
      
project "MessageBox"
   kind "SharedLib"
   language "C++"
   targetdir "bin/x86/%{cfg.buildcfg}/scripts"
   targetextension ".asi"
   
   files { "demo_plugins/x86/*.cpp" }
   files { "source/resources/Versioninfo.rc" }
   includedirs { "demo_plugins/x86" }

   characterset ("MBCS")
   
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
      flags { "StaticRuntime" }
      
      
-- x64
workspace "Ultimate-ASI-Loader-x64"
   configurations { "Release", "Debug" }
   architecture "x86_64"
   location "build"
   
   defines { "rsc_CompanyName=\"ThirteenAG\"" }
   defines { "rsc_LegalCopyright=\"MIT License\""} 
   defines { "rsc_FileVersion=\"1.0.0.0\"", "rsc_ProductVersion=\"1.0.0.0\"" }
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{prj.name}.dll\"" }
   defines { "rsc_FileDescription=\"Ultimate ASI Loader\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/Ultimate-ASI-Loader\"" }
   
   defines { "X64" }
     
project "Ultimate-ASI-Loader-x64"
   kind "SharedLib"
   language "C++"
   targetdir "bin/x64/%{cfg.buildcfg}"
   targetname "dinput8"
   targetextension ".dll"
   
   includedirs { "source/x64" }
   includedirs { "external" }
   
   files { "source/x64/*.h", "source/x64/*.cpp" }
   files { "source/x64/x64.def" }
   files { "source/dllmain.cpp" }
   files { "source/resources/Versioninfo.rc" }
   
   characterset ("MBCS")
   
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
      flags { "StaticRuntime" }
      
project "RE7Demo.InfiniteAmmo"
   kind "SharedLib"
   language "C++"
   targetdir "bin/x64/%{cfg.buildcfg}/scripts"
   targetextension ".asi"
   
   files { "demo_plugins/x64/*.cpp" }
   files { "source/resources/Versioninfo.rc" }
   includedirs { "demo_plugins/x64" }

   characterset ("MBCS")
   
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
      flags { "StaticRuntime" }