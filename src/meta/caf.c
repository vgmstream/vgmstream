#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* CAF - from tri-Crescendo games [Baten Kaitos 1/2 (GC), Fragile (Wii)] */
VGMSTREAM* init_vgmstream_caf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int channels, loop_flag;
    int32_t num_samples = 0;


    /* checks */
    if (!is_id32be(0x00,sf, "CAF "))
        return NULL;

    /* .caf: header id
     * (extensionless): files on disc don't have any extensions
     * .cfn: fake extension */
    if (!check_extensions(sf,"caf,cfn,"))
        return NULL;

    /* get total samples from blocks + find loop */ //TODO reuse function calls
    uint32_t loop_start = -1;
    off_t offset = 0x00;
    off_t file_size = get_streamfile_size(sf);
    while (offset < file_size) {
        // see blocked layout for block info
        off_t next_block = read_u32be(offset+0x04,sf);
        off_t channel_bytes = read_u32be(offset+0x14,sf);
        int channel_samples = dsp_bytes_to_samples(channel_bytes, 1);

        if (read_u32be(offset+0x08,sf) == read_u32be(offset+0x20,sf) && loop_start < 0) {
            loop_start = num_samples;
        }

        num_samples += channel_samples;
        offset += next_block;
    }

    start_offset = 0x00;
    channels = 2; /* always stereo */
    loop_flag = (loop_start != -1);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 32000;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = num_samples;

    vgmstream->meta_type = meta_CAF;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_blocked_caf;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
