#include "meta.h"
#include "../coding/coding.h"
#include "../util/chunks.h"

/* .SDD - from Piglet's Big Game (PS2/GC) */
VGMSTREAM* init_vgmstream_sdd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t header_size, data_size, sample_rate, sound_offset, sound_size;
    uint8_t codec, channels;
    off_t table_offset, data_offset, entry_offset, name_offset;
    size_t name_size;
    int target_subsong = sf->stream_index, total_subsongs, loop_flag;

    if (!is_id32be(0x00, sf, "DSBH"))
        goto fail;

    if (!check_extensions(sf, "sdd"))
        goto fail;

    /* always little endian, even on GC */
    header_size = read_u32le(0x04, sf);
    table_offset = 0x20;

    /* haven't seen any filenames larger than 16 bytes so should be safe */
    total_subsongs = (header_size - table_offset) / 0x20;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1)
        goto fail;

    /* get name buffer size */
    name_offset = table_offset + (target_subsong - 1) * 0x20;
    name_size = read_string(NULL, STREAM_NAME_SIZE, name_offset, sf) + 1;

    entry_offset = name_offset + name_size;
    codec = read_u8(entry_offset + 0x00, sf);
    //bps = read_u8(entry_offset + 0x01, sf);
    channels = read_u8(entry_offset + 0x02, sf);
    sample_rate = read_u32le(entry_offset + 0x03, sf);
    sound_offset = read_u32le(entry_offset + 0x07, sf);
    sound_size = read_u32le(entry_offset + 0x0b, sf);

    /* no stereo samples seen */
    if (channels > 1)
        goto fail;

    data_offset = header_size;
    if (!is_id32be(data_offset, sf, "DSBD"))
        goto fail;

    data_size = read_u32le(data_offset + 0x04, sf);
    if (data_offset + data_size > get_streamfile_size(sf))
        goto fail;

    sound_offset += data_offset + 0x20;
    loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SDD;
    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = sound_size;
    vgmstream->num_streams = total_subsongs;
    read_string(vgmstream->stream_name, STREAM_NAME_SIZE, name_offset, sf);

    switch (codec) {
        case 0x01: /* DSP */
            /* starts with incomplete DSP header (nibble count not set) */
            if (sound_size < 0x60)
                goto fail;

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = read_u32be(sound_offset + 0x00, sf);

            /* set coefs and initial history */
            dsp_read_coefs_be(vgmstream, sf, sound_offset + 0x1c, 0x00);
            vgmstream->ch[0].adpcm_history1_16 = read_u16be(sound_offset + 0x40, sf);
            vgmstream->ch[0].adpcm_history2_16 = read_u16be(sound_offset + 0x42, sf);

            sound_offset += 0x60;
            vgmstream->stream_size -= 0x60;
            break;
        case 0x02: { /* PCM */
            off_t chunk_offset;
            size_t chunk_size;

            /* stored as RIFF */
            sound_offset += 0x0c;
            sound_size -= 0x0c;

            /* find "data" chunk */
            if (!find_chunk_riff_le(sf, 0x64617461, sound_offset, sound_size, &chunk_offset, &chunk_size))
                goto fail;

            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = pcm16_bytes_to_samples(chunk_size, channels);

            sound_offset = chunk_offset;
            vgmstream->stream_size = chunk_size;
            break;
        }
        case 0x03: /* PSX */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = ps_bytes_to_samples(sound_size, channels);
            break;
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, sound_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
