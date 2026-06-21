#include "meta.h"
#include "../coding/coding.h"

/* MSF - TamaSoft games [Abandoner: The Severed Dreams (PC)] */
VGMSTREAM * init_vgmstream_msf_tamasoft(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate, codec, loop_start = 0, loop_end = 0;
    size_t data_size;


    /* checks */
    if (!is_id32be(0x00,sf, "MSF "))
        return NULL;
    /* .msf: extension referenced in .EXE (bigfiles don't have filenames) */
    if (!check_extensions(sf, "msf"))
        return NULL;

    if (read_u32be(0x08,sf) != 0x00) // just in case since there are other .msf
        return NULL;

    /* header from 0x10 to 0x30 is encrypted (though value at 0x10 doubles as key...) */
    uint16_t xor16 = (uint16_t)((read_u32le(0x04,sf) * 0x65u) + 0x30Au); // from the exe
    uint32_t xor32 = (uint32_t)(((uint32_t)xor16 << 16u) | (uint32_t)xor16);

    // 0x10: null */
    loop_flag       = read_u32le(0x14,sf) ^ xor32;
    data_size       = read_u32le(0x18,sf) ^ xor32;
    codec           = read_u16le(0x1c,sf) ^ xor16;
    channel_count   = read_u16le(0x1e,sf) ^ xor16;
    sample_rate     = read_u32le(0x20,sf) ^ xor32;
    // 0x24: average bitrate?
    // 0x28: block size
    // 0x2a: bps
    // 0x2c: unknown (fixed)

    if (loop_flag) {
        loop_start      = read_s32le(0x30,sf);
        loop_end        = read_s32le(0x34,sf);
        // 0x38/3c: null
        start_offset = 0x40;
    }
    else {
        start_offset = 0x30;
    }

    if (codec != 0x01)
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MSF_TAMASOFT;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm_bytes_to_samples(data_size, channel_count, 16);
    vgmstream->loop_start_sample = pcm_bytes_to_samples(loop_start, channel_count, 16);
    vgmstream->loop_end_sample   = pcm_bytes_to_samples(loop_end, channel_count, 16);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
