#include "meta.h"

/* RIFF IMA - from Final Fantasy Tactics A2 (NDS) */
VGMSTREAM* init_vgmstream_riff_ima(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks*/
    if (!is_id32be(0x00,sf, "RIFF"))
        return NULL;
    // 04: full filesize
    if (!is_id32be(0x08,sf, "IMA "))
        return NULL;

    /* .bin: actual extension
     * .strm: folder */
    if (!check_extensions(sf,"bin,lbin,strm"))
        return NULL;

    loop_flag = (read_s32le(0x20,sf) !=0);
    channels = read_s32le(0x24,sf);
    start_offset = 0x2C;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->channels = channels;
    vgmstream->sample_rate = read_s32le(0x0C,sf);
    vgmstream->num_samples = (read_s32le(0x04,sf)-start_offset);
    vgmstream->loop_start_sample = read_s32le(0x20,sf);
    vgmstream->loop_end_sample = read_s32le(0x28,sf);

    vgmstream->meta_type = meta_RIFF_IMA;
    vgmstream->coding_type = coding_SQEX_IMA;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x80;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
