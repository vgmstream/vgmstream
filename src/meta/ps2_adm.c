#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include <string.h>

static int get_adm_loop_info(STREAMFILE *streamFile, off_t *loop_start_offset);

/* .adm - from Dragon Quest V (PS2) */
VGMSTREAM * init_vgmstream_ps2_adm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int channel_count, loop_flag = 0;
    off_t start_offset, loop_start_offset = 0;

    /* checks */
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
    loop_flag = get_adm_loop_info(streamFile, &loop_start_offset);
    channel_count = 2;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_ADM;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_adm;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    /* calc num_samples as playable data size varies between files/blocks */
    {
        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset,vgmstream);
            if (loop_flag && vgmstream->current_block_offset == loop_start_offset)
                vgmstream->loop_start_sample = vgmstream->num_samples;
            vgmstream->num_samples += ps_bytes_to_samples(vgmstream->current_block_size, 1);
        }
        while (vgmstream->next_block_offset < get_streamfile_size(streamFile));
        block_update(start_offset,vgmstream);

        if (loop_flag)
            vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* loops are not in the .ADM or .DAT bigfile containing them but in the exe; manually get them (a bit meh but whatevs) */
static int get_adm_loop_info(STREAMFILE *streamFile, off_t *loop_start_offset) {
    char file_name[PATH_LIMIT];
    char index_name[PATH_LIMIT];
    STREAMFILE *streamExe = NULL;
    int i, name_index = -1, loop_flag;
    off_t offset;

    streamExe = open_streamfile_by_filename(streamFile, "SLPM_655.55");
    if (!streamExe) goto fail;

    get_streamfile_filename(streamFile, file_name, PATH_LIMIT);

    /* get file index from name list (file_name == index_name = index number */
    offset = 0x23B3c0;
    for (i = 0; i < 51; i++) {
        read_string(index_name,0x20+1, offset,streamExe);

        if (strcmp(index_name, file_name)==0) {
            name_index = i;
            break;
        }
        offset += 0x20;
    }
    if (name_index < 0)
        goto fail;

    /* get file info using index */
    offset = 0x23BAEC + 0x1c*name_index;
    loop_flag = (read_32bitLE(offset + 0x10, streamExe) == 0); /* 1: don't loop, 0: loop */
    if (loop_flag) { /* loop flag */
        *loop_start_offset = read_32bitLE(offset + 0x04, streamExe);
    }
    /* 0x08: num_samples/loop_end, 0x0c: sample rate (always 44100), 0x14/18: some size? */

    close_streamfile(streamExe);
    return loop_flag;
fail:
    close_streamfile(streamExe);
    return 0;
}
