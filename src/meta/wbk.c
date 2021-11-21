#include "meta.h"
#include "../coding/coding.h"

/* .WBK - seen in some Treyarch games [Spider-Man 2, Ultimate Spider-Man, Call of Duty 2: Big Red One] */
VGMSTREAM* init_vgmstream_wbk(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t table_offset, entry_offset, data_offset, streams_offset, strings_offset, strings_size, coefsec_offset,
        name_offset, codec, flags, channels, sound_offset, sound_size, num_samples, sample_rate;
    int target_subsong = sf->stream_index, total_subsongs, loop_flag, has_names, i;

    /* checks */
    if (!is_id32be(0x00, sf, "WAVE") ||
        !is_id32be(0x04, sf, "BK11"))
        goto fail;

    /* checks */
    if (!check_extensions(sf, "wbk"))
        goto fail;

    /* always little endian, even on GC */
    data_offset = read_u32le(0x10, sf);
    //data_size = read_u32le(0x14, sf);
    streams_offset = read_u32le(0x18, sf);
    //streams_size = read_u32le(0x1c, sf);

    total_subsongs = read_u32le(0x40, sf);
    table_offset = read_u32le(0x44, sf);

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1)
        goto fail;

    //paramsec_size = read_u32le(0x50, sf);
    //paramsec_offset = read_u32le(0x54, sf);
    //coefsec_size = read_u32le(0x58, sf);
    coefsec_offset = read_u32le(0x5c, sf);
    strings_size = read_u32le(0x60, sf);
    strings_offset = read_u32le(0x64, sf);

    /* Ultimate Spider-Man has no names, only name hashes */
    {
        size_t len;

        /* check if the first sound points at the first or the second string */
        len = read_string(NULL, STREAM_NAME_SIZE, strings_offset, sf);
        name_offset = read_u32le(table_offset + 0x00, sf);
        has_names = (name_offset == 0x00 || name_offset == len + 0x01);
    }

    /* 0x00: name offset/name hash
     * 0x04: codec
     * 0x05: flags
     * 0x06: channel mask (can actually only be mono or stereo)
     * 0x07: padding
     * 0x08: sound size
     * 0x0c: number of samples
     * 0x10: group name offset
     * 0x14: parameters offset
     * 0x18: DSP coefs offset, usually not set (-1)
     * 0x1c: sound offset
     * 0x20: sample rate
     * 0x24: always 0?
     * 
     * struct slightly changed in Call of Duty 2 but still compatible
     */
    entry_offset = table_offset + (target_subsong - 1) * 0x28;
    name_offset = read_u32le(entry_offset + 0x00, sf);
    codec = read_u8(entry_offset + 0x04, sf);
    flags = read_u8(entry_offset + 0x05, sf);
    channels = read_u8(entry_offset + 0x06, sf) == 0x03 ? 2 : 1;
    sound_size = read_u32le(entry_offset + 0x08, sf);
    num_samples = read_u32le(entry_offset + 0x0c, sf);
    sound_offset = read_u32le(entry_offset + 0x1c, sf);
    sample_rate = read_u16le(entry_offset + 0x20, sf);

    if (!(flags & 0x02)) /* streamed sounds have absolute offset */
        sound_offset += data_offset;

    loop_flag = (flags & 0x08);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->meta_type = meta_WBK;
    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = sound_size;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample = num_samples; /* full loops only */
    vgmstream->num_streams = total_subsongs;

    switch (codec) {
        case 0x03: { /* DSP */
            uint32_t coef_offset;
            uint16_t coef_table[16] = {
                0x0216,0xfc9f,0x026c,0x04b4,0x065e,0xfdec,0x0a11,0xfd1e,
                0x0588,0xfc38,0x05ad,0x01da,0x083b,0xfdbc,0x08c3,0xff18
            };

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x8000;

            coef_offset = read_u32le(entry_offset + 0x18, sf);
            if (coef_offset == UINT32_MAX || coefsec_offset == 0x00) {
                /* hardcoded coef table */
                for (i = 0; i < vgmstream->channels; i++)
                    memcpy(vgmstream->ch[i].adpcm_coef, coef_table, sizeof(coef_table));
            } else {
                if (coefsec_offset == UINT32_MAX)
                    goto fail;

                dsp_read_coefs_be(vgmstream, sf, coefsec_offset + coef_offset, 0x28);
            }
            break;
        }
        case 0x04: /* PSX */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x800;
            break;

        case 0x05: /* XBOX */
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            break;

        case 0x07: /* IMA */
            vgmstream->coding_type = coding_IMA;
            vgmstream->layout_type = layout_none;

            /* for some reason, number of samples is off for IMA */
            vgmstream->num_samples = ima_bytes_to_samples(sound_size, channels);
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

        default:
            goto fail;
    }

    if (has_names)
        read_string(vgmstream->stream_name, STREAM_NAME_SIZE, strings_offset + name_offset, sf);
    else
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%08x", name_offset);

    if (!vgmstream_open_stream(vgmstream, sf, sound_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}