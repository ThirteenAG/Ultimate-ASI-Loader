#include "vorbisfile.hpp"
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <cstring>

extern "C" const float* vorbis_window(vorbis_dsp_state* v, int W); // libvorbis, not in codec.h

namespace ual::compat::vorbisfile
{
    namespace
    {
        // libvorbis functions vorbisfile.c calls through dispatch.h. X(name, ret, params, args)
#define UAL_VORBIS_FUNCTIONS(X)                                                                              \
    X(vorbis_block_clear, int, (vorbis_block * vb), (vb))                                                    \
    X(vorbis_block_init, int, (vorbis_dsp_state * v, vorbis_block * vb), (v, vb))                            \
    X(vorbis_comment_clear, void, (vorbis_comment * vc), (vc))                                               \
    X(vorbis_comment_init, void, (vorbis_comment * vc), (vc))                                                \
    X(vorbis_dsp_clear, void, (vorbis_dsp_state * v), (v))                                                   \
    X(vorbis_info_blocksize, int, (vorbis_info * vi, int zo), (vi, zo))                                      \
    X(vorbis_info_clear, void, (vorbis_info * vi), (vi))                                                     \
    X(vorbis_info_init, void, (vorbis_info * vi), (vi))                                                      \
    X(vorbis_packet_blocksize, long, (vorbis_info * vi, ogg_packet * op), (vi, op))                          \
    X(vorbis_synthesis, int, (vorbis_block * vb, ogg_packet * op), (vb, op))                                 \
    X(vorbis_synthesis_blockin, int, (vorbis_dsp_state * v, vorbis_block * vb), (v, vb))                     \
    X(vorbis_synthesis_halfrate, int, (vorbis_info * v, int flag), (v, flag))                                \
    X(vorbis_synthesis_halfrate_p, int, (vorbis_info * v), (v))                                              \
    X(vorbis_synthesis_headerin, int, (vorbis_info * vi, vorbis_comment * vc, ogg_packet * op), (vi, vc, op)) \
    X(vorbis_synthesis_init, int, (vorbis_dsp_state * v, vorbis_info * vi), (v, vi))                         \
    X(vorbis_synthesis_lapout, int, (vorbis_dsp_state * v, float*** pcm), (v, pcm))                          \
    X(vorbis_synthesis_pcmout, int, (vorbis_dsp_state * v, float*** pcm), (v, pcm))                          \
    X(vorbis_synthesis_read, int, (vorbis_dsp_state * v, int samples), (v, samples))                         \
    X(vorbis_synthesis_restart, int, (vorbis_dsp_state * v), (v))                                            \
    X(vorbis_synthesis_trackonly, int, (vorbis_block * vb, ogg_packet * op), (vb, op))                       \
    X(vorbis_window, const float*, (vorbis_dsp_state * v, int W), (v, W))

        // Either the game's vorbis.dll or the linked libvorbis.
        struct Decoder
        {
#define UAL_DECL(name, ret, params, args) ret(__cdecl* name) params = ::name;
            UAL_VORBIS_FUNCTIONS(UAL_DECL)
#undef UAL_DECL
            bool game = false;
        };

        Decoder Choose()
        {
            Decoder linked;
            HMODULE dll = LoadLibraryW(L"vorbis.dll"); // same search as the original vorbisFile.dll
            if (!dll) return linked;
            Decoder fromGame;
            bool complete = true;
#define UAL_RESOLVE(name, ret, params, args)     if (auto p = GetProcAddress(dll, #name)) fromGame.name = reinterpret_cast<decltype(fromGame.name)>(p); else complete = false;
            UAL_VORBIS_FUNCTIONS(UAL_RESOLVE)
#undef UAL_RESOLVE
            if (!complete) // all or nothing, the two decoders' state must not mix
            {
                FreeLibrary(dll);
                return linked;
            }
            fromGame.game = true;
            return fromGame;
        }

        Decoder& Get()
        {
            static Decoder d = Choose();
            return d;
        }
    }

    bool UsesGameDecoder()
    {
        return Get().game;
    }

    FARPROC BuiltinExport(const char* name)
    {
        Get(); // choose the decoder before the first call
        static const struct
        {
            const char* name;
            FARPROC proc;
        } exports[] = {
#define E(n) { #n, reinterpret_cast<FARPROC>(&n) }
            E(ov_bitrate),       E(ov_bitrate_instant), E(ov_clear),        E(ov_comment),          E(ov_crosslap),
            E(ov_fopen),         E(ov_halfrate),        E(ov_halfrate_p),   E(ov_info),             E(ov_open),
            E(ov_open_callbacks), E(ov_pcm_seek),       E(ov_pcm_seek_lap), E(ov_pcm_seek_page),    E(ov_pcm_seek_page_lap),
            E(ov_pcm_tell),      E(ov_pcm_total),       E(ov_raw_seek),     E(ov_raw_seek_lap),     E(ov_raw_tell),
            E(ov_raw_total),     E(ov_read),            E(ov_read_float),   E(ov_read_filter),      E(ov_seekable),
            E(ov_serialnumber),  E(ov_streams),         E(ov_test),         E(ov_test_callbacks),   E(ov_test_open),
            E(ov_time_seek),     E(ov_time_seek_lap),   E(ov_time_seek_page), E(ov_time_seek_page_lap), E(ov_time_tell),
            E(ov_time_total),
#undef E
        };
        for (const auto& e : exports)
            if (name && !strcmp(e.name, name)) return e.proc;
        return nullptr;
    }
}

// Targets of the dispatch.h renames.
extern "C"
{
#define UAL_FORWARD(name, ret, params, args) \
    ret ual_vf_##name params { return ual::compat::vorbisfile::Get().name args; }
    UAL_VORBIS_FUNCTIONS(UAL_FORWARD)
#undef UAL_FORWARD
}
