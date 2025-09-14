#include "meta.h"
#include "../coding/coding.h"


/* BCF1 - RAD Game Tools format [Minecraft (PS3/Vita/PS4), Scribblenauts Unmasked (PC)] */
VGMSTREAM* init_vgmstream_bcf1(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset; 


    /* checks */
    if (!is_id32be(0x00,sf, "1FCB")) //'bcf' from debug strings, 1 may be version?
        return NULL;
    if (!check_extensions(sf,"binka"))
        return NULL;

    // always LE even on PS3 machines
    int version = read_u8(0x04,sf);
    int channels = read_u8(0x05,sf);
    int sample_rate = read_u16le(0x06,sf);
    int32_t num_samples = read_s32le(0x08,sf);
    //0c: max frame_size
    uint32_t file_size = read_s32le(0x10,sf);

    if (file_size != get_streamfile_size(sf)) {
        vgm_logi("BCF1: expected size %x (re-rip?)\n", file_size);
        //return NULL;
    }

    // seek table may be 0 (disabled when encoding)
    int seek_entries;
    if (version == 1) { // Civilization V (PC), rare
        seek_entries = read_s32le(0x14,sf);
    }
    else if (version == 2) {
        seek_entries = read_u16le(0x14,sf);
        //16: frames per seek block (to calculate total samples)
        // frames per block is usually 1 but seen 1, 2, 4, 8, ... 64 depending on file size
        // (encoder auto-adjusts so seek table doesn't grow too big)
    } else {
        // decoder checks <= 2, not seen v0 files
        vgm_logi("BCF1: unknown version (report)\n");
        return NULL;
    }

    bool loop_flag = false;

    start_offset = 0x18 + seek_entries * 0x02;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_BCF1;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    vgmstream->codec_data = init_binka_bcf1(sample_rate, channels);
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
