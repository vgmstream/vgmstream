#include "meta.h"
#include "../coding/coding.h"

/* XA30 - from Reflections games [Driver: Parallel Lines (PC/PS2), Driver 3 (PC)] */
VGMSTREAM* init_vgmstream_xa_xa30(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    /* checks */
    if (!is_id32be(0x00,sf, "XA30") &&  /* [Driver: Parallel Lines (PC/PS2)] */
        !is_id32be(0x00,sf, "e4x\x92")) /* [Driver 3 (PC)] */
        return NULL;

    /* .XA: actual extension
     * .xa30/e4x: header ID */
    if (!check_extensions(sf,"xa,xa30,e4x"))
        return NULL;


    off_t start_offset;
    int loop_flag, channels, interleave, sample_rate;
    uint32_t codec, stream_size, file_size;
    int total_subsongs, target_subsong = sf->stream_index;

    if (read_u32le(0x04,sf) > 2) {
        /* PS2 */
        sample_rate = read_s32le(0x04,sf);
        interleave = read_u16le(0x08, sf); // assumed, always 0x8000
        channels = read_u16le(0x0a, sf); // assumed, always 1
        start_offset = read_u32le(0x0C,sf);
        // 10: some samples value? (smaller than real samples)
        stream_size = read_u32le(0x14,sf); // always off by 0x800
        // 18: fixed values
        // 1c: null
        // rest of the header: garbage/repeated values or config (includes stream name)

        if (channels != 1)
            return NULL;

        codec = 0xFF; //fake codec to simplify
        total_subsongs = 0;

        file_size = get_streamfile_size(sf);
        if (stream_size - 0x0800 != file_size)
            return NULL;
        stream_size = file_size - start_offset;
    }
    else {
        /* PC */
        total_subsongs = read_u32le(0x14,sf) != 0 ? 2 : 1; /* second stream offset (only in Driver 3) */
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

        channels = read_s32le(0x04,sf); // assumed, always 2
        sample_rate = read_s32le(0x08,sf);
        codec = read_u32le(0x0c,sf);
        start_offset = read_u32le(0x10 + 0x04 * (target_subsong - 1),sf);
        stream_size  = read_u32le(0x18 + 0x04 * (target_subsong - 1),sf);
        //20: fixed: IMA=00016000, PCM=00056000
        interleave = read_u32le(0x24, sf);
        // rest of the header is null

        if (channels != 2)
            return NULL;
    }

    loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XA_XA30;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    switch(codec) {
        case 0x00:   /* Driver 3 (PC)-rare */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave / 2;
            vgmstream->num_samples = pcm16_bytes_to_samples(stream_size, channels);
            break;

        case 0x01:
            vgmstream->coding_type = coding_REF_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = interleave;
            vgmstream->num_samples = ms_ima_bytes_to_samples(stream_size, interleave, channels);
            break;

        case 0xFF:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = interleave;
            vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);
            break;

        default:
           goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
