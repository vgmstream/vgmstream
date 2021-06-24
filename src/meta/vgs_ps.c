#include "meta.h"
#include "../coding/coding.h"

/* VGS - from Princess Soft games [Gin no Eclipse (PS2), Metal Wolf REV (PS2)] */
VGMSTREAM* init_vgmstream_vgs_ps(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t data_size, channel_size, interleave, sample_rate;
    int loop_flag, channels;
    int32_t loop_start = 0, loop_end = 0;


    /* check */
    if (!check_extensions(sf,"vgs"))
        goto fail;
    if (!is_id32be(0x00,sf, "VGS\0")) /* 'VAG stereo', presumably (simple VAG clone) */
        goto fail;

    start_offset = 0x30;
    data_size = get_streamfile_size(sf) - start_offset;

    /* test PS-ADPCM null frame for 2nd channel to detect interleave */
    if (read_u32be(0x20000 + start_offset,sf) == 0) {
        interleave = 0x20000; /* common */
    }
    else if (read_u32be(0x8000 + start_offset,sf) == 0) {
        interleave = 0x8000; /* Ishikura Noboru no Igo Kouza: Chuukyuuhen (PS2) */
    }
    else {
        goto fail;
    }

    channels = 2;
    channel_size = read_u32be(0x0c,sf);
    sample_rate = read_s32be(0x10,sf);
    loop_flag = 0; /* all files have loop flags but simply fade out normally and repeat */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VGS_PS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(channel_size, 1);
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (data_size % (vgmstream->interleave_block_size * channels)) / channels;
    read_string(vgmstream->stream_name,0x10+1, 0x20,sf); /* always, can be null */

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
