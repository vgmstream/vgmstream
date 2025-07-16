#include "meta.h"
#include "../coding/coding.h"

/* srcd - Capcom RE Engine */
VGMSTREAM* init_vgmstream_srcd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* subfile = NULL;
    off_t start_offset = 0;
    int loop_flag = 0;
    int32_t loop_start_sample = 0, loop_end_sample = 0;
    uint32_t container_type;
    const char* extension = NULL;
    VGMSTREAM* (*init_vgmstream_function)(STREAMFILE*) = NULL;

    if (!is_id32be(0x00, sf, "srcd"))
        return NULL;

    if (!check_extensions(sf, "srcd,asrc,14,21,26,31"))
        return NULL;

    {
        enum versions { VERSION_31, VERSION_21_26, VERSION_14, VERSION_UNKNOWN };
        enum versions ver = VERSION_UNKNOWN;

        //v31 - AJ_AAT
        if (read_u32le(0x18, sf) > 0x02) {
            ver = VERSION_31;
        }
        //v21 - CAS2
        else if (read_u32le(0x41, sf) == 0x49) {
            ver = VERSION_21_26;
        }
        //v26 - GTPD
        else if (read_u32le(0x46, sf) == 0x4E) {
            ver = VERSION_21_26;
        }
        //v14 - CAS
        else if (read_u8(0x3A, sf) == 0x42) {
            ver = VERSION_14;
        }

        switch (ver) {
            case VERSION_31:
                loop_flag         = read_u8(0x34, sf);
                loop_start_sample = read_u32le(0x35, sf);
                loop_end_sample   = read_u32le(0x39, sf);
                break;

            case VERSION_21_26:
                loop_flag         = read_u8(0x2C, sf);
                loop_start_sample = read_u32le(0x2D, sf);
                loop_end_sample   = read_u32le(0x31, sf);
                break;

            case VERSION_14:
                loop_flag         = read_u8(0x28, sf);
                loop_start_sample = read_u32le(0x29, sf);
                loop_end_sample   = read_u32le(0x2D, sf);
                break;

            default:
                VGM_LOG("SRCD: Unknown version, disabling loop\n");
                loop_flag = 0;
                break;
        }
    }

    container_type = read_u32be(0x0C, sf);
    {
        const off_t scan_start = 0x40;
        const size_t scan_size = 0x100; //Should be small
        off_t current_offset;
        uint32_t magic_to_find = 0;

        if (container_type == get_id32be("wav ")) {
            magic_to_find = get_id32be("RIFF");
        } else if (container_type == get_id32be("ogg ")) {
            magic_to_find = get_id32be("OggS");
        }

        if (magic_to_find) {
            for (current_offset = scan_start; current_offset < scan_start + scan_size; current_offset++) {
                if (read_u32be(current_offset, sf) == magic_to_find) {
                    start_offset = current_offset;
                    break;
                }
            }
        }
    }

    if (start_offset == 0)
        goto fail;


    /* Select the appropriate init function and extension based on container type */
    if (container_type == get_id32be("wav ")) {
        extension = "wav";
        init_vgmstream_function = init_vgmstream_riff;
    } else if (container_type == get_id32be("ogg ")) {
        extension = "ogg";
        init_vgmstream_function = init_vgmstream_ogg_vorbis;
    } else {
        VGM_LOG("SRCD: Codec not recognized");
        goto fail;
    }

    subfile = setup_subfile_streamfile(sf, start_offset, get_streamfile_size(sf) - start_offset, extension);
    if (!subfile) goto fail;

    vgmstream = init_vgmstream_function(subfile);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SRCD;

    vgmstream_force_loop(vgmstream, loop_flag, loop_start_sample, loop_end_sample);

    close_streamfile(subfile);
    return vgmstream;

fail:
    close_streamfile(subfile);
    close_vgmstream(vgmstream);
    return NULL;
}
