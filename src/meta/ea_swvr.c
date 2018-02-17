#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* SWVR - from EA games [Future Cop L.A.P.D. (PS/PC), Freekstyle (PS2/GC), EA Sports Supercross (PS)] */
VGMSTREAM * init_vgmstream_ea_swvr(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count;
    int big_endian;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;


    /* check extension */
    if (!check_extensions(streamFile,"str"))
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) == 0x53575652) { /* "SWVR" (GC) */
        big_endian = 1;
        read_32bit = read_32bitBE;
    }
    else if (read_32bitBE(0x00,streamFile) == 0x52565753) { /* "RVWS" (PS/PS2) */
        big_endian = 0;
        read_32bit = read_32bitLE;
    }
    else {
        goto fail;
    }


    start_offset = read_32bit(0x04,streamFile);
    loop_flag = 1;
    channel_count = 2;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 16000;
    vgmstream->codec_endian = big_endian;

    vgmstream->meta_type = meta_EA_SWVR;
    vgmstream->layout_type = layout_blocked_ea_swvr;

    vgmstream->coding_type = coding_PSX;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;


    /* calculate samples */
    {
        off_t current_chunk = start_offset;

        vgmstream->num_samples = 0;
        while ((current_chunk + start_offset) < (get_streamfile_size(streamFile))) {
            uint32_t block_id = (read_32bit(current_chunk,streamFile));
            if (block_id == 0x5641474D) { /* "VAGM" */
                block_update_ea_swvr(start_offset,vgmstream);
                vgmstream->num_samples += vgmstream->current_block_size/16*28;
                current_chunk += vgmstream->current_block_size + 0x1C;
            }
            current_chunk += 0x10;
        }
    }

    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
