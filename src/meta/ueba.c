#include "meta.h"
#include "../coding/coding.h"


/* UEBA - from Unreal Engine 5 games [RoboCop: Rogue City (PC), Stellar Blade (PC)] */
VGMSTREAM* init_vgmstream_ueba(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset; 


    /* checks */
    if (!is_id32be(0x00,sf, "ABEU"))
        return NULL;
    /* .ueba: header id
     * .binka: class name
    */
    if (!check_extensions(sf,"ueba,binka"))
        return NULL;

    // variation of BCF1
    int version = read_u8(0x04,sf);
    int channels = read_u8(0x05,sf);
    // 06: reserved/unused
    int sample_rate = read_s32le(0x08,sf);
    int32_t num_samples = read_s32le(0x0c,sf);
    //10: max frame_size
    uint16_t flags = read_u16le(0x12,sf);
    //14: file size (doesn't seem to include SEEK chunks)

    // v1 is enforced by exes, flags is always 1 but seems ignored
    if (version != 1 || flags != 1) {
        vgm_logi("UEBA: unknown version (report)\n");
        return NULL;
    }

    // in UEBA seek entries may be 0 and use multiple 'SEEK' chunks during decode instead
    int seek_entries = read_u16le(0x18,sf);
    // 1a: frames per seek block (to calculate total samples)

    bool loop_flag = false;

    start_offset = 0x1c + seek_entries * 0x02;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UEBA;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    vgmstream->codec_data = init_binka_ueba(sample_rate, channels);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_BINKA;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
