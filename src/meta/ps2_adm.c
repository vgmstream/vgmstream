#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include <string.h>

static int get_adm_loop_info(STREAMFILE *streamFile, off_t *loop_start_offset);


/* .adm - from Dragon Quest V (PS2) */
VGMSTREAM* init_vgmstream_ps2_adm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag = 0;
    off_t start_offset, loop_start_offset = 0;

    /* checks */
    if (!check_extensions(sf,"adm"))
        return NULL;

    //TODO improve/move to .txth (block behavior must be handled in vgmstream though)
    // - most blocks end with a 0x03 flag + 0x10 padding, then next blocks restarts with 0x06 flag
    // - MTXADPCM.IRX reads a 0x400 L block then R block, doesn't seem to do any block handling
    // - padded blocks don't seem consistent or depends on size/loop/etc:
    //   (but there seems to be 4 parts in looped files and 2 in non-looped)
    //   - MS_overture_lp.adm (loop offset 0x2B2000)
    //     - 0x000000~0x020000: padding (all blocks end with 0x03 flag + 0x10 padding)
    //     - 0x020000~0x2B2800: no padding (only last LR block ends with 0x03 flags)
    //     - 0x2B2800~0x2FA800: padding (all blocks)
    //     - 0x2FB000~0x442000: no padding
    //   - MS_saint.adm (loop offset 0x082000)
    //     - 0x000000~0x04E000: padding
    //     - 0x04E000~0x082800: no padding
    //     - 0x082800~0x0CF000: padding
    //     - 0x0CF000~0x40B000: no padding
    //   - MS_item.adm
    //     - 0x000000~0x016000: padding
    //     - 0x016000~0x01A000: no padding
    // - possibly automatically handled by SPU2
    //   - 0x03 flag > ends playback > immediately gets next block = no audio skips

    /* raw data, but test some .ADM blocks as they always start with PS-ADPCM flag 0x06 every 0x1000 */
    for (int i = 0; i < 10; i++) {
        if (read_u8(0x1000 * i + 0x01, sf) != 0x06)
            return NULL;
    }

    start_offset = 0x00;
    loop_flag = get_adm_loop_info(sf, &loop_start_offset);
    channels = 2;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    // TODO use info from header
    vgmstream->meta_type = meta_PS2_ADM;
    vgmstream->sample_rate = 44100;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_blocked_adm;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
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
        while (vgmstream->next_block_offset < get_streamfile_size(sf));
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
static int get_adm_loop_info(STREAMFILE* sf, off_t *loop_start_offset) {
    char file_name[PATH_LIMIT];
    char index_name[PATH_LIMIT];
    STREAMFILE *sh = NULL;
    int i, name_index = -1, loop_flag;
    off_t offset;

    sh = open_streamfile_by_filename(sf, "SLPM_655.55");
    if (!sh) goto fail;

    get_streamfile_filename(sf, file_name, PATH_LIMIT);

    /* get file index from name list (file_name == index_name = index number */
    offset = 0x23B3c0;
    for (i = 0; i < 51; i++) {
        read_string(index_name,0x20+1, offset,sh);

        if (strcmp(index_name, file_name)==0) {
            name_index = i;
            break;
        }
        offset += 0x20;
    }
    if (name_index < 0)
        goto fail;

    /* get file info using index */
    offset = 0x23BAF0 + 0x1c * name_index;
    loop_flag = (read_32bitLE(offset + 0x0c, sh) == 0); /* 1: don't loop, 0: loop */
    if (loop_flag) { /* loop flag */
        *loop_start_offset = read_32bitLE(offset + 0x00, sh);
    }
    
    // 23BA20 = adpcm volumes?
    // 23BAF0 = adpcm info
    // 00: loop start offset
    // 04: loop end (size)
    // 08: sample rate
    // 0c: loop flag (0=inf, 1=no loop)
    // 10: loop start block (in 0x800 sizes)
    // 14: loop length block (in 0x800 sizes)
    // 18: null

    close_streamfile(sh);
    return loop_flag;
fail:
    close_streamfile(sh);
    return 0;
}
