// Stand-in for the vorbis.dll a vorbisFile.dll game ships, which the x86 loader's built-in
// vorbisfile uses when present. The tests never call these stubs.
#include <windows.h>

#define STUB(name) extern "C" int __cdecl name() { return 0; }
STUB(vorbis_synthesis_halfrate_p)
STUB(vorbis_synthesis_halfrate)
STUB(vorbis_window)
STUB(vorbis_synthesis)
STUB(vorbis_packet_blocksize)
STUB(vorbis_synthesis_read)
STUB(vorbis_synthesis_lapout)
STUB(vorbis_synthesis_pcmout)
STUB(vorbis_synthesis_blockin)
STUB(vorbis_synthesis_trackonly)
STUB(vorbis_info_init)
STUB(vorbis_synthesis_restart)
STUB(vorbis_synthesis_init)
STUB(vorbis_synthesis_headerin)
STUB(vorbis_dsp_clear)
STUB(vorbis_block_clear)
STUB(vorbis_block_init)
STUB(vorbis_comment_clear)
STUB(vorbis_comment_init)
STUB(vorbis_info_blocksize)
STUB(vorbis_info_clear)

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
