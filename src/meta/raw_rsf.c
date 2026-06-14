#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* .rsf - from Metroid Prime (GC) */
VGMSTREAM* init_vgmstream_raw_rsf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag;
    uint32_t interleave, file_size;

    /* checks */
    if (!check_extensions(sf,"rsf"))
        return NULL;

    /* this is all we have to go on, rsf is completely headerless */
    file_size = get_streamfile_size(sf);
    interleave = (file_size + 1) / 2;

    const int max_tests = 0x20; // 0x20 is arbitrary, all files are much larger
    bool ok = g721_check_format(sf, interleave, max_tests);
    if (!ok) return NULL;

    channels = 2;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = file_size;
    vgmstream->sample_rate = 32000;

    vgmstream->coding_type = coding_G721;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_RSF;

    if (!vgmstream_open_stream(vgmstream, sf, 0x00))
        goto fail;

    /* open the file for reading by each channel */
    for (int i = 0; i < channels; i++) {
        vgmstream->ch[i].channel_start_offset= vgmstream->ch[i].offset = interleave * i;
    }

    setup_g721(vgmstream);

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
