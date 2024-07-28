#include "meta.h"
#include "../util/endianness.h"
#include "../coding/coding.h"


/* EA TMX - used for engine sounds in NFS games (2007-2011) */
VGMSTREAM* init_vgmstream_ea_tmx(STREAMFILE* sf) {
    uint32_t num_sounds, sound_type, table_offset, data_offset, entry_offset, sound_offset;
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int target_stream = sf->stream_index;
    uint32_t(*read_u32)(off_t, STREAMFILE *);


    /* checks */
    if (is_id32be(0x0c, sf, "0001")) {
        read_u32 = read_u32be;
    }
    else if (is_id32le(0x0c, sf, "0001")) {
        read_u32 = read_u32le;
    }
    else {
        return NULL;
    }

    if (!check_extensions(sf, "tmx"))
        return NULL;

    num_sounds = read_u32(0x20, sf);
    table_offset = read_u32(0x58, sf);
    data_offset = read_u32(0x5c, sf);

    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || num_sounds == 0 || target_stream > num_sounds)
        goto fail;

    entry_offset = table_offset + (target_stream - 1) * 0x24;
    sound_type = read_u32(entry_offset + 0x00, sf);
    sound_offset = read_u32(entry_offset + 0x08, sf) + data_offset;

    switch (sound_type) {
        case 0x47494E20: /* "GIN " */
            temp_sf = setup_subfile_streamfile(sf, sound_offset, get_streamfile_size(sf) - sound_offset, "gin");
            if (!temp_sf) goto fail;

            vgmstream = init_vgmstream_gin(temp_sf);
            if (!vgmstream) goto fail;
            close_streamfile(temp_sf);
            break;
        case 0x534E5220: { /* "SNR " */
            eaac_meta_t info = {0};

            info.sf_head = sf;
            info.head_offset = sound_offset;
            info.body_offset = 0x00;
            info.type = meta_EA_SNR_SNS;

            vgmstream = load_vgmstream_ea_eaac(&info);
            if (!vgmstream) goto fail;
            break;
        }
        default:
            goto fail;
    }

    vgmstream->num_streams = num_sounds;
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    return NULL;
}
