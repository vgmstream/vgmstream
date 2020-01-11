#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


/* .str - 3DO format with CTRL/SNDS/SHDR blocks  [Icebreaker (3DO), Battle Pinball (3DO)] */
VGMSTREAM * init_vgmstream_str_snds(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, shdr_offset = -1;
    int loop_flag, channel_count, found_shdr = 0;
    size_t file_size, ctrl_size = -1;


    /* checks */
    if (!check_extensions(streamFile, "str"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x4354524c &&   /* "CTRL" */
        read_32bitBE(0x00,streamFile) != 0x534e4453 &&   /* "SNDS" */
        read_32bitBE(0x00,streamFile) != 0x53484452)     /* "SHDR" */
        goto fail;

    file_size = get_streamfile_size(streamFile);
    start_offset = 0x00;

    /* scan chunks until we find a SNDS containing a SHDR */
    {
        off_t current_chunk = 0;

        while (!found_shdr && current_chunk < file_size) {
            if (current_chunk < 0) goto fail;

            if (current_chunk+read_32bitBE(current_chunk+0x04,streamFile) >= file_size)
                goto fail;

            switch (read_32bitBE(current_chunk,streamFile)) {
                case 0x4354524C: /* "CTRL" */
                    ctrl_size = read_32bitBE(current_chunk+4,streamFile);
                    break;

                case 0x534e4453: /* "SNDS" */
                    switch (read_32bitBE(current_chunk+16,streamFile)) {
                        case 0x53484452: /* SHDR */
                            found_shdr = 1;
                            shdr_offset = current_chunk+16;
                            break;
                        default:
                            break;
                    }
                    break;

                case 0x53484452: /* "SHDR" */
                    switch (read_32bitBE(current_chunk+0x7C, streamFile)) {
                        case 0x4354524C: /* "CTRL" */
                            /* to distinguish between styles */
                            ctrl_size = read_32bitBE(current_chunk + 0x80, streamFile);
                            break;

                        default:
                            break;
                    }
                    break;

                default:
                    /* ignore others for now */
                    break;
            }

            current_chunk += read_32bitBE(current_chunk+0x04,streamFile);
        }
    }

    if (!found_shdr) goto fail;

    channel_count = read_32bitBE(shdr_offset+0x20,streamFile);
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_STR_SNDS;
    vgmstream->sample_rate = read_32bitBE(shdr_offset+0x1c,streamFile);

    if (ctrl_size == 0x1C || ctrl_size == 0x0B || ctrl_size == -1) {
        vgmstream->num_samples = read_32bitBE(shdr_offset+0x2c,streamFile) - 1; /* sample count? */
    } 
    else {
        vgmstream->num_samples = read_32bitBE(shdr_offset+0x2c,streamFile) * 0x10; /* frame count? */
    }
    vgmstream->num_samples /= vgmstream->channels;

    switch (read_32bitBE(shdr_offset+0x24,streamFile)) {
        case 0x53445832:    /* "SDX2" */
            if (channel_count > 1) {
                vgmstream->coding_type = coding_SDX2_int;
                vgmstream->interleave_block_size = 1;
            } else
                vgmstream->coding_type = coding_SDX2;
            break;
        default:
            goto fail;
    }
    vgmstream->layout_type = layout_blocked_str_snds;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
