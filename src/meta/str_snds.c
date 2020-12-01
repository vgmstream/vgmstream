#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"


/* .str - 3DO format with CTRL/SNDS/SHDR blocks  [Icebreaker (3DO), Battle Pinball (3DO)] */
VGMSTREAM* init_vgmstream_str_snds(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, shdr_offset = -1;
    int loop_flag, channels, found_shdr = 0;
    size_t file_size, ctrl_size = -1;


    /* checks */
    /* .str: standard
     * .stream: Battle Tryst (Arcade) movies
     * .3do: Aqua World - Umimi Monogatari (3DO) movies */
    if (!check_extensions(sf, "str,stream,3do"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x4354524c &&   /* "CTRL" */
        read_u32be(0x00,sf) != 0x534e4453 &&   /* "SNDS" */
        read_u32be(0x00,sf) != 0x53484452)     /* "SHDR" */
        goto fail;

    file_size = get_streamfile_size(sf);
    start_offset = 0x00;

    /* scan chunks until we find a SNDS containing a SHDR */
    {
        off_t offset = 0;
        uint32_t size;

        while (!found_shdr && offset < file_size) {
            if (offset < 0) goto fail;

            size = read_u32be(offset + 0x04,sf);
            if (offset + size >= file_size)
                goto fail;

            switch (read_u32be(offset + 0x00,sf)) {
                case 0x4354524C: /* "CTRL" */
                    ctrl_size = read_u32be(offset + 0x04,sf);
                    break;

                case 0x534e4453: /* "SNDS" */
                    switch (read_u32be(offset + 0x10,sf)) {
                        case 0x53484452: /* SHDR */
                            found_shdr = 1;
                            shdr_offset = offset + 0x10;
                            break;
                        default:
                            break;
                    }
                    break;

                case 0x53484452: /* "SHDR" */
                    switch (read_u32be(offset + 0x7C, sf)) {
                        case 0x4354524C: /* "CTRL" */
                            /* to distinguish between styles */
                            ctrl_size = read_u32be(offset + 0x80, sf);
                            break;

                        default:
                            break;
                    }
                    break;

                default: /* ignore others */
                    break;
            }

            offset += size;
        }
    }

    if (!found_shdr)
        goto fail;

    channels = read_u32be(shdr_offset+0x20,sf);
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_STR_SNDS;
    vgmstream->sample_rate = read_u32be(shdr_offset+0x1c,sf);

    if (ctrl_size == 0x1C || ctrl_size == 0x0B || ctrl_size == -1) {
        vgmstream->num_samples = read_u32be(shdr_offset+0x2c,sf) - 1; /* sample count? */
    } 
    else {
        vgmstream->num_samples = read_u32be(shdr_offset+0x2c,sf) * 0x10; /* frame count? */
    }
    vgmstream->num_samples /= vgmstream->channels;

    switch (read_u32be(shdr_offset + 0x24,sf)) {
        case 0x53445832:    /* "SDX2" (common) */
            if (channels > 1) {
                vgmstream->coding_type = coding_SDX2_int;
                vgmstream->interleave_block_size = 0x01;
            } else {
                vgmstream->coding_type = coding_SDX2;
            }
            break;

        case 0x43424432:    /* "CBD2" (rare, Battle Tryst) */
            if (channels > 1) {
                vgmstream->coding_type = coding_CBD2_int;
                vgmstream->interleave_block_size = 0x01;
            } else {
                vgmstream->coding_type = coding_CBD2; /* assumed */
            }
            break;

        default:
            goto fail;
    }
    vgmstream->layout_type = layout_blocked_str_snds;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
