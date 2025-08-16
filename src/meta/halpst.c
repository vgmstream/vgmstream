#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


/* HALPST - HAL engine(?) format [Kirby Air Ride (GC), Giftpia (GC), Killer7 (GC)] */
VGMSTREAM* init_vgmstream_halpst(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    /* checks */
    if (!is_id32be(0x00, sf, " HAL") && !is_id32be(0x04, sf, "PST\0"))
        return NULL;
    if (!check_extensions(sf,"hps"))
        return NULL;

    int channels, sample_rate, loop_flag = false;

    sample_rate = read_s32be(0x08,sf);
    channels    = read_s32be(0x0c,sf);

    // per DSP channel:
    // 00 max block size?
    // 04 type 2?
    // 08 nibbles
    // 0c type 2?
    // 10: coefs
    int32_t num_samples = dsp_nibbles_to_samples(read_u32be(0x18,sf)) + 1;

    uint32_t start_offset = 0x80;
    // TODO: needed? (only 1/2ch files ad both start at the same offset)
    if (channels > 2) {
        // align the header length needed for the extra channels
        start_offset = 0x10 + 0x38* channels;
        start_offset = (start_offset + 0x1f) / 0x20 * 0x20;
    }


    // looping info is implicit in the "next block" field of the final block
    int32_t loop_start = 0;
    {
        off_t last_offset = 0;
        off_t offset = start_offset;

        /* determine if there is a loop */
        while (offset > last_offset) {
            last_offset = offset;
            offset = read_u32be(offset + 0x08,sf);
        }

        if (offset >= 0) {
            loop_flag = true;

            /* one more pass to determine start sample */
            int32_t start_nibble = 0;
            off_t loop_offset = offset;
            offset = start_offset;
            while (offset != loop_offset) {
                start_nibble += read_s32be(offset + 0x04,sf) + 1;
                offset = read_u32be(offset + 0x08,sf);
            }

            loop_start = dsp_nibbles_to_samples(start_nibble);
        }
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = num_samples;
    vgmstream->sample_rate = sample_rate;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_blocked_halpst;
    vgmstream->meta_type = meta_HALPST;

    dsp_read_coefs_be(vgmstream, sf, 0x10 + 0x10, 0x38);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    //block_update(header_length, vgmstream);
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
