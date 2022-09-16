#include "meta.h"
#include "../coding/coding.h"


static int check_subinterleave(STREAMFILE* sf, uint32_t start_offset);

/* GCub - found in Black Box/Shaba games (engine?) [Sega Soccer Slam (GC), Shrek the Third (CC), Shrek Superslam (GC)] */
VGMSTREAM* init_vgmstream_gcub(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size;
    int channels, loop_flag, sample_rate;
    int is_subinterleave;


    /* checks */
    if (!is_id32be(0x00,sf, "GCub"))
        goto fail;

    /* .wav: extension found in bigfile
     * .gcub: header id */
    if (!check_extensions(sf, "wav,lwav,gcub"))
        goto fail;

    loop_flag = 0;
    channels = read_u32be(0x04,sf);
    sample_rate = read_u32be(0x08,sf);
    data_size = read_u32be(0x0c,sf);

    if (is_id32be(0x60,sf, "GCxx")) /* seen in sfx */
        start_offset = 0x88;
    else
        start_offset = 0x60;

    /* Shaba/Shrek games use it but no header diffs, detect by inspecting data 
     * (could try checking initial PS but not enough for files that start with 0s) */
    is_subinterleave = 0;
    if (channels > 1)
        is_subinterleave = check_subinterleave(sf, start_offset);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_GCUB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = dsp_bytes_to_samples(data_size, channels);
    vgmstream->coding_type = is_subinterleave ? coding_NGC_DSP_subint : coding_NGC_DSP;
    vgmstream->layout_type = is_subinterleave ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = is_subinterleave ? 0x01 : 0x8000;

    dsp_read_coefs_be(vgmstream, sf, 0x10, 0x20);
    /* 0x50: initial ps for ch1/2 (16b) */
    /* 0x54: hist? (always blank) */

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static int check_subinterleave(STREAMFILE* sf, uint32_t offset) {
    int test_frames = 50;
    uint32_t size = get_streamfile_size(sf);

    /* DSP frame headers can't go past certain values, while nibbles can, so check that supposed headers are so */
    while (test_frames >= 0 && offset < size) {
        uint8_t h2 = read_u8(offset + 0x01,sf); /* only need to test header for 2nd channel */

        /* ignore blank frames */
        if (h2 != 0x00) {
            uint8_t factor = (h2 >> 0) & 0xF; /* not sure about actual max but 0x0b is max usually */
            uint8_t index  = (h2 >> 4) & 0xF;

            if (factor > 0x0c || index > 8)
                return 0;
            test_frames--;
        }

        offset += 0x10;
    }

    /* too many blanks */
    if (test_frames > 0)
        return 0;

    return 1;
}