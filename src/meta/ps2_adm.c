#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* .adm - from Dragon Quest V (PS2) */
VGMSTREAM * init_vgmstream_ps2_adm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int channel_count, loop_flag = 0;
    off_t start_offset;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"adm"))
        goto fail;

    /* raw data, but test some .ADM blocks as they always start with PS-ADPCM flag 0x06 every 0x1000 */
    {
        int i;
        for (i = 0; i < 10; i++) {
            if (read_8bit(0x1000*i + 0x01, streamFile) != 0x06)
                goto fail;
        }
    }

    start_offset = 0x00;
    loop_flag = 0; /* files loop, but apparently no info in the .adm or in the .dat bigfile containing them */
    channel_count = 2;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 44100;
    vgmstream->meta_type = meta_PS2_ADM;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_ps2_adm_blocked;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    /* calc num_samples as playable data size varies between files/blocks */
    vgmstream->num_samples = 0; //ps_bytes_to_samples(get_streamfile_size(streamFile), channel_count);
    ps2_adm_block_update(start_offset,vgmstream);
    do {
        vgmstream->num_samples += ps_bytes_to_samples(vgmstream->current_block_size * channel_count, channel_count);
        ps2_adm_block_update(vgmstream->next_block_offset,vgmstream);
    } while (vgmstream->next_block_offset < get_streamfile_size(streamFile));

    ps2_adm_block_update(start_offset,vgmstream);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
