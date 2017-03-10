-- x86
workspace "Ultimate-ASI-Loader-x86"
   configurations { "Release", "Debug" }
   architecture "x86"
   location "build"
     
project "Ultimate-ASI-Loader-x86"
   kind "SharedLib"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"
   targetname "dinput8"
   targetextension ".dll"
   
   includedirs { "source/src" }
   includedirs { "external" }
   includedirs { "source/xlive" }
   
   files { "source/src/*.h", "source/src/*.cpp" }
   files { "source/def/*.def" }
   files { "source/res/*.rc" }
   files { "source/verinfo/*.rc" }
   files { "external/d3d8to9/source/*.h", "external/d3d8to9/source/*.cpp" }
   files { "external/MemoryModule/*.h", "external/MemoryModule/*.c" }

   local dxsdk = os.getenv "DXSDK_DIR" or "Error: DXSDK_DIR not set!"
   includedirs { dxsdk .. "/include" }
   libdirs { dxsdk .. "/lib/x86" }
   
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      targetdir "data"
      defines { "NDEBUG", "D3D8TO9NOLOG" }
      optimize "On"
	  flags { "StaticRuntime" }
	  characterset ("MBCS")
	  
project "TestPlugin-x86"
   kind "SharedLib"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}/examples/scripts"
   targetextension ".asi"
   
   files { "source/test/*.h", "source/test/*.cpp" }
   files { "source/test/*.rc" }
   files { "source/verinfo/*.rc" }
   
   includedirs { "source/test" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      targetdir "data/examples/scripts"
      defines { "NDEBUG" }
      optimize "On"
	  flags { "StaticRuntime" }
	  characterset ("MBCS")
	  
	  
-- x64
workspace "Ultimate-ASI-Loader-x64"
   configurations { "Release", "Debug" }
   architecture "x86_64"
   location "build"
     
project "Ultimate-ASI-Loader-x64"
   kind "SharedLib"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}/x64/"
   targetname "dinput8"
   targetextension ".dll"
   
   includedirs { "source/x64loader" }
   
   files { "source/x64loader/*.h", "source/x64loader/*.cpp" }
   files { "source/x64loader/*.def" }
   files { "source/verinfo/*.rc" }
   
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      targetdir "data/x64/"
      defines { "NDEBUG" }
      optimize "On"
	  flags { "StaticRuntime" }
	  characterset ("MBCS")
	  
project "TestPlugin-x64"
   kind "SharedLib"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}/x64/examples/scripts"
   targetname "RE7Demo.InfiniteAmmo"
   targetextension ".asi"
   
   files { "source/x64test/*.h", "source/x64test/*.cpp" }
   files { "source/verinfo/*.rc" }
   
   includedirs { "source/x64test" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      targetdir "data/x64/examples/scripts"
      defines { "NDEBUG" }
      optimize "On"
	  flags { "StaticRuntime" }
	  characterset ("MBCS")