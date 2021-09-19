#include "meta.h"
#include "../util.h"

/* STRM - common Nintendo NDS streaming format */
VGMSTREAM* init_vgmstream_nds_strm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int channels, loop_flag, codec, sample_rate;


    /* checks */
    if (!is_id32be(0x00,sf, "STRM"))
        goto fail;

    if (!check_extensions(sf, "strm"))
        goto fail;

    /* BOM check? */
    if (read_u32be(0x04,sf) != 0xFFFE0001 &&
        read_u32be(0x04,sf) != 0xFEFF0001) /* newer games? */
        goto fail;

    if (!is_id32be(0x10,sf, "HEAD") &&
        read_u32le(0x14,sf) != 0x50)
        goto fail;

    codec = read_u8(0x18,sf);
    loop_flag = read_u8(0x19,sf);
    sample_rate = read_u16le(0x1c,sf);
    channels = read_u8(0x1a,sf);
    if (channels > 2) goto fail;

    start_offset = read_u32le(0x28,sf);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = read_32bitLE(0x24,sf);
    vgmstream->loop_start_sample = read_32bitLE(0x20,sf);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_STRM;

    switch (codec) {
        case 0x00: /* [Bleach - Dark Souls (DS)] */
            vgmstream->coding_type = coding_PCM8;
            break;
        case 0x01:
            vgmstream->coding_type = coding_PCM16LE;
            break;
        case 0x02: /* [SaGa 2 (DS)] */
            vgmstream->coding_type = coding_NDS_IMA;
            break;
        default:
            goto fail;
    }
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_32bitLE(0x30,sf);
    vgmstream->interleave_last_block_size = read_32bitLE(0x38,sf);


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
