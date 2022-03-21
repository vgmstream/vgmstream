#include "meta.h"
#include "../coding/coding.h"

typedef struct {
    int loop_flag;
    int32_t loop_start;
    int32_t loop_end;
    uint32_t file_size;
#ifdef VGM_USE_FFMPEG
    mp4_custom_t mp4;
#endif
    int type;
} ktac_header_t;


/* KTAC - Koei Tecmo custom AAC [Kin'iro no Corda 3 (Vita), Shingeki no Kyojin: Shichi kara no Dasshutsu (3DS), Dynasty Warriors (PS4)] */
VGMSTREAM* init_vgmstream_ktac(STREAMFILE* sf) {
#ifdef VGM_USE_FFMPEG
    VGMSTREAM* vgmstream = NULL;
    ktac_header_t ktac = {0};

    /* checks */
    /* .ktac: header id */
    if (!check_extensions(sf,"ktac"))
        goto fail;
    if (!is_id32be(0x00,sf, "KTAC"))
        goto fail;

    /* 0x04: version? (always 1) */
    ktac.file_size = read_u32le(0x08,sf);
    if (ktac.file_size != get_streamfile_size(sf))
        goto fail;
    ktac.mp4.stream_offset  = read_u32le(0x0c,sf);
    ktac.mp4.stream_size    = read_u32le(0x10,sf);
    ktac.type               = read_u32le(0x14,sf);
    ktac.mp4.sample_rate    = read_u32le(0x18,sf);
    ktac.mp4.num_samples    = read_u32le(0x1c,sf); /* full samples */
    ktac.mp4.channels       = read_u16le(0x20,sf);
    ktac.mp4.frame_samples  = read_u16le(0x22,sf);
    ktac.mp4.encoder_delay  = read_u16le(0x24,sf);
    ktac.mp4.end_padding    = read_u16le(0x26,sf);
    ktac.loop_start         = read_u32le(0x28,sf);
    ktac.loop_end           = read_u32le(0x2c,sf);
    /* 0x30: ? (big, related to loops) */
    /* 0x34: ? (always null) */
    ktac.mp4.table_offset   = read_u32le(0x38,sf);
    ktac.mp4.table_entries  = read_u32le(0x3c,sf);

    ktac.loop_flag = (ktac.loop_end > 0);

    /* type 1 files crash during sample_copy, wrong fake header/esds?
     * (0=AoT, KnC3 bgm, 1=KnC3 1ch voices, 2=DW4, Atelier Ryza) */
    if (ktac.type == 1)
        goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ktac.mp4.channels, ktac.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_KTAC;
    vgmstream->sample_rate = ktac.mp4.sample_rate;
    vgmstream->num_samples = ktac.mp4.num_samples - ktac.mp4.encoder_delay - ktac.mp4.end_padding;
    vgmstream->loop_start_sample = ktac.loop_start * ktac.mp4.frame_samples - ktac.mp4.encoder_delay;
    vgmstream->loop_end_sample = ktac.loop_end * ktac.mp4.frame_samples - ktac.mp4.encoder_delay;

    /* KTAC uses AAC, but not type found in .aac (that has headered frames, like mp3) but raw
     * packets + frame size table (similar to .mp4/m4a). We set config for FFmpeg's fake M4A header */
    vgmstream->codec_data = init_ffmpeg_mp4_custom_std(sf, &ktac.mp4);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
#endif
    return NULL;
}
