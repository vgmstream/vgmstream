#include "meta.h"
#include "../coding/coding.h"
#include "../util/chunks.h"


/* CSMP - Retro Studios sample [Metroid Prime 3 (Wii)-sfx, Donkey Kong Country Returns (Wii)-sfx] */
VGMSTREAM* init_vgmstream_csmp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, first_offset = 0x08, chunk_offset;
    int loop_flag, channels;


    /* checks */
    if (!is_id32be(0x00, sf, "CSMP"))
        return NULL;
    if (!check_extensions(sf, "csmp"))
        return NULL;
    if (read_u32be(0x04, sf) != 1) // version?
        return NULL;

    // originally implemented by Antidote (see 9a03256)

    // fixed chunks: INFO > PAD > DATA
    if (!find_chunk(sf, get_id32be("DATA"), first_offset,0, &chunk_offset, NULL, 1, 0))
        return NULL;

    // contains a not quite standard DSP header
    channels = 1; // also at INFO + 0x00? (in practice uses dual stereo in separate files)
    loop_flag = read_s16be(chunk_offset + 0x0c,sf); // also at INFO + 0x01
    start_offset = chunk_offset + 0x60;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_CSMP;
    vgmstream->sample_rate = read_s32be(chunk_offset+0x08,sf);
    vgmstream->num_samples = read_s32be(chunk_offset+0x00,sf);
    vgmstream->loop_start_sample = read_s32be(chunk_offset+0x10,sf); // unlike regular DSP's nibbles
    vgmstream->loop_end_sample   = read_s32be(chunk_offset+0x14,sf) + 1;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    dsp_read_coefs_be(vgmstream, sf, chunk_offset+0x1c, 0x00);
    dsp_read_hist_be(vgmstream, sf, chunk_offset+0x40, 0x00);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
