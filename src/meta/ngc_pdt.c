#include "meta.h"
#include "../coding/coding.h"

/* PDT - custom fake header for split (PDTExt) .ptd [Mario Party (GC)] */
VGMSTREAM * init_vgmstream_ngc_pdt_split(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "pdt"))
        goto fail;

    /* 0x10 fake header + chunks of the original header / data pasted together */
    if (read_32bitBE(0x00,streamFile) != 0x50445420 && /* "PDT " */
        read_32bitBE(0x04,streamFile) != 0x44535020 && /* "DSP " */
        read_32bitBE(0x08,streamFile) != 0x48454144 && /* "HEAD " */
        read_16bitBE(0x0C,streamFile) != 0x4552)       /* "ER " */
        goto fail;

    start_offset = 0x800;
    channel_count = (uint16_t)(read_16bitLE(0x0E,streamFile));
    loop_flag = (read_32bitBE(0x1C,streamFile) != 2);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x14,streamFile);

    if (channel_count == 1) {
        vgmstream->num_samples = read_32bitBE(0x18,streamFile)*14/8/channel_count/2;
        if (loop_flag) {
            vgmstream->loop_start_sample = read_32bitBE(0x1C,streamFile)*14/8/channel_count/2;
            vgmstream->loop_end_sample = read_32bitBE(0x18,streamFile)*14/8/channel_count/2;
        }
    }
    else if (channel_count == 2) {
        vgmstream->num_samples = read_32bitBE(0x18,streamFile)*14/8/channel_count;
        if (loop_flag) {
            vgmstream->loop_start_sample = read_32bitBE(0x1C,streamFile)*14/8/channel_count;
            vgmstream->loop_end_sample = read_32bitBE(0x18,streamFile)*14/8/channel_count;
        }
    }
    else {
        goto fail;
    }

    vgmstream->meta_type = meta_NGC_PDT;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    dsp_read_coefs_be(vgmstream, streamFile, 0x50, 0x20);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    if (channel_count == 2) {
        vgmstream->ch[1].channel_start_offset =
                vgmstream->ch[1].offset = ((get_streamfile_size(streamFile)+start_offset) / channel_count);
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
