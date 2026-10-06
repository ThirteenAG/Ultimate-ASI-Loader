// Built-in vorbisFile.dll for older games (GTA San Andreas, Scarface), used when the loader is
// named vorbisFile.dll and no vorbisFileHooked.dll or vorbisHooked.dll sits next to it.
// vorbisfile.c and libogg are built from external/vorbis and external/ogg. Decoding uses the game's
// vorbis.dll if it exports everything vorbisfile.c needs, otherwise the linked libvorbis.
// vorbis_synthesis_idheader always comes from the linked libvorbis since the games' 1.0.x builds lack it.
#pragma once
#include <windows.h>

namespace ual::compat::vorbisfile
{
    // Built-in ov_* export by name, or null.
    FARPROC BuiltinExport(const char* name);

    // True when decoding through the game's vorbis.dll.
    bool UsesGameDecoder();
}
