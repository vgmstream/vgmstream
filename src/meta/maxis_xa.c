#include "meta.h"
#include "../util.h"

/* Maxis XA - found in Sim City 3000 (PC), The Sims 2 (PC) */
VGMSTREAM* init_vgmstream_maxis_xa(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    
    /* checks */
    if ((read_u32be(0x00,sf) & 0xFFFF0000) != get_id32be("XA\0\0")) // 02: "I"/"J"/null (version?)
        return NULL;

    uint32_t pcm_size = read_u32le(0x04,sf);
    if (read_u16le(0x08,sf) != 0x0001)
        return NULL;

    if (!check_extensions(sf,"xa"))
        return NULL;

    int channels = read_s16le(0x0A,sf);
    int sample_rate = read_s32le(0x0C,sf);
    int avg_byte_rate = read_s32le(0x10,sf);
    int sample_align = read_u16le(0x14,sf);
    int resolution = read_u16le(0x16,sf);

    /* check alignment */
    if (sample_align != (resolution / 8) * channels)
        return NULL;

    /* check average byte rate */
    if (avg_byte_rate != sample_rate * sample_align)
        return NULL;

    off_t start_offset = 0x18;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, false);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm_size / sizeof(short) / channels;

    vgmstream->meta_type = meta_MAXIS_XA;
    vgmstream->coding_type = coding_MAXIS_XA;
    vgmstream->layout_type = layout_none;

    /* open streams */
    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
