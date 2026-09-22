#include "meta.h"
#include "../coding/coding.h"


/* KA Sound - Knowledge Adventure [(ca. 199X)/Dr. Brain Thinking Games: Puzzle Madness / JumpStart 199X] */
VGMSTREAM* init_vgmstream_ka(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t table_offset = 0x36, start_offset;
    uint16_t table_size;
    int version, compression, channels, sample_rate;
    size_t file_size = get_streamfile_size(sf);
    int32_t num_samples;

    if (!is_id64be(0x00, sf, "KA Sound"))
        return NULL;
    if (!check_extensions(sf, "snd"))
        return NULL;
    if (file_size < table_offset)
        return NULL;

    version = read_u8(0x0e, sf);
    compression = read_u8(0x0f, sf);
    channels = read_u8(0x10, sf);
    if ((version != 1 && version != 2) || compression != 2 || channels != 1)
        return NULL;
    if (read_u8(0x12, sf) != 8 || read_u8(0x13, sf) != 16)
        return NULL;

    switch (read_u8(0x14, sf)) {
        case 1:
            sample_rate = 11025;
            break;
        case 2:
            sample_rate = 22050;
            break;
        case 3:
            sample_rate = 44100;
            break;
        default:
            return NULL;
    }
    num_samples = read_s32le(0x20, sf);
    if (num_samples <= 0)
        return NULL;

    table_size = read_u16le(0x34, sf);
    start_offset = table_offset + table_size;
    if (file_size < start_offset)
        return NULL;

    vgmstream = allocate_vgmstream(channels, false);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_KA_SOUND;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->stream_size = file_size - start_offset;
    vgmstream->coding_type = coding_KA_ADPCM;
    vgmstream->layout_type = layout_none;

    vgmstream->codec_data = init_ka(sf, table_offset, num_samples);
    if (!vgmstream->codec_data) goto fail;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
