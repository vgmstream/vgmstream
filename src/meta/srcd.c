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
    uint32_t fourcc = 0;
    const char* extension = NULL;
    VGMSTREAM* (*init_vgmstream_function)(STREAMFILE*) = NULL;

    if (!is_id32be(0x00, sf, "srcd"))
        return NULL;

    if (!check_extensions(sf, "srcd,asrc,14,21,26,31,de,en,fr,ja,ko"))
        return NULL;

    container_type = read_u32be(0x0C, sf);

    if (container_type == get_id32be("wav ")) {
        extension = "wav";
        fourcc = get_id32be("RIFF");
        init_vgmstream_function = init_vgmstream_riff;
    } else if (container_type == get_id32be("ogg ")) {
        extension = "ogg";
        fourcc = get_id32be("OggS");
        init_vgmstream_function = init_vgmstream_ogg_vorbis;
    } else {
        VGM_LOG("SRCD: Codec not recognized\n");
        return NULL;
    }

    /* Versions
       Version 14 - CAS
       Version 21 - CAS2
       Version 26 - GTPD
       Version 31 - AJAAT, GTPD (Android)
     */

    enum versions { VERSION_31, VERSION_26, VERSION_21, VERSION_14, VERSION_UNKNOWN };
    enum versions ver = VERSION_UNKNOWN;

    /* Static Check:
       SRCD header contains ptr to the start of the data.
       The ptr value equals its own offset + 8.
    */

    typedef struct {
        off_t offset;
        enum versions version;
    } static_entry_t;

    const static_entry_t static_entries[] = {
        { 0x46, VERSION_26 },
        { 0x41, VERSION_21 },
        { 0x3A, VERSION_14 }
    };

    for (int i = 0; i < sizeof(static_entries) / sizeof(static_entry_t); i++) {
        off_t pos = static_entries[i].offset;
        uint32_t val = read_u32le(pos, sf);

        if (val == pos + 0x08) {
            if (read_u32be(val, sf) == fourcc) {
                start_offset = val;
                ver = static_entries[i].version;
                break;
            }
        }
    }

    /* Dynamic Check (Version 31):
       Base offset: 0x4E
       Table Count: 0x3D (Byte)
       Entry Size:  0x08 bytes
       Pos = 0x4E + (Count * 0x08)
     */

    uint8_t table_count = read_u8(0x3D, sf);
    off_t pos = 0x4E + (table_count * 0x08);
    uint32_t val = read_u32le(pos, sf);

    if (val == pos + 0x08) {
        if (read_u32be(val, sf) == fourcc) {
            start_offset = val;
            ver = VERSION_31;
        }
    }

    if (start_offset == 0)
        return NULL;

    switch (ver) {
        case VERSION_31:
            loop_flag = read_u8(0x34, sf);
            loop_start_sample = read_u32le(0x35, sf);
            loop_end_sample = read_u32le(0x39, sf);
            break;

        case VERSION_26:
        case VERSION_21:
            loop_flag = read_u8(0x2C, sf);
            loop_start_sample = read_u32le(0x2D, sf);
            loop_end_sample = read_u32le(0x31, sf);
            break;

        case VERSION_14:
            loop_flag = read_u8(0x28, sf);
            loop_start_sample = read_u32le(0x29, sf);
            loop_end_sample = read_u32le(0x2D, sf);
            break;

        default:
            VGM_LOG("SRCD: Unknown header layout, disabling loop\n");
            loop_flag = 0;
            break;
    }


    subfile = setup_subfile_streamfile(sf, start_offset, get_streamfile_size(sf) - start_offset, extension);
    if (!subfile) return NULL;

    vgmstream = init_vgmstream_function(subfile);
    if (!vgmstream) {
        close_streamfile(subfile);
        return NULL;
    }

    vgmstream->meta_type = meta_SRCD;

    vgmstream_force_loop(vgmstream, loop_flag, loop_start_sample, loop_end_sample);

    close_streamfile(subfile);
    return vgmstream;
}
