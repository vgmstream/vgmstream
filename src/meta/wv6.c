#include "meta.h"
#include "../coding/coding.h"

/* WV6 - Gorilla Systems PC games [Spy Kids: Mega Mission Zone (PC), Lilo & Stitch: Hawaiian Adventure (PC)] */
VGMSTREAM* init_vgmstream_wv6(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (read_u32le(0x00,sf) != get_streamfile_size(sf))
        return NULL;
    // 04: filename
    if (!is_id32be(0x2c,sf, "WV6 ") || !is_id32be(0x30,sf, "IMA_")) // "WV6 IMA_ADPCM COMPRESSED 16 BIT AUDIO"
        return NULL;

    if (!check_extensions(sf, "wv6"))
        return NULL;

    // 0x54/58/5c/60/6c: unknown (reject to catch possible stereo files, but don't seem to exist)
    if (read_s32le(0x54,sf) != 0x01 ||
        read_s32le(0x58,sf) != 0x01 ||
        read_s32le(0x5c,sf) != 0x10 ||
        read_s32le(0x68,sf) != 0x01 ||
        read_s32le(0x6c,sf) != 0x88)
        return NULL;
    // 0x64: PCM size (samples * channels * 2)

    channels = 1;
    loop_flag = 0;
    start_offset = 0x8c;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32le(0x60, sf);
    vgmstream->num_samples = ima_bytes_to_samples(read_u32le(0x88,sf), channels);

    vgmstream->meta_type = meta_WV6;
    vgmstream->coding_type = coding_WV6_IMA;
    vgmstream->layout_type = layout_none;

    read_string(vgmstream->stream_name, 0x1c + 1, 0x04,sf);

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
