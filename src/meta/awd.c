#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* AWD - Audio Wave Dictionary (RenderWare) */
VGMSTREAM* init_vgmstream_awd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    char header_name[STREAM_NAME_SIZE], stream_name[STREAM_NAME_SIZE];
    int /*bit_depth = 0,*/ channels = 0, sample_rate = 0, stream_codec = -1, total_subsongs = 0, target_subsong = sf->stream_index;
    int interleave, loop_flag;
    off_t data_offset, header_name_offset, misc_data_offset, linked_list_offset, wavedict_offset;
    off_t entry_info_offset, entry_name_offset, /*entry_uuid_offset,*/ next_entry_offset, prev_entry_offset, stream_offset = 0;
    read_u32_t read_u32;
    size_t /*data_size,*/ header_size, /*misc_data_size,*/ stream_size = 0;

    /* checks */
    if (read_u32le(0x00, sf) != 0x809 && read_u32be(0x00, sf) != 0x809)
        goto fail;

    /* .awd: standard (Burnout series, Black, Call of Duty: Finest Hour)
     * .hwd/lwd: high/low vehicle engine sounds (Burnout series)
     * (Burnout 3: Takedown, Burnout Revenge, Burnout Dominator) */
    if (!check_extensions(sf, "awd,hwd,lwd"))
        goto fail;

    read_u32 = read_u8(0x04, sf) ? read_u32be : read_u32le;
    //read_u32 = guess_endian32(0x00, sf) ? read_u32be : read_u32le;

    data_offset = read_u32(0x08, sf);
    wavedict_offset = read_u32(0x0C, sf);
    //data_size = read_u32(0x14, sf);
    /* Platform UUIDs in big endian
     * {FD9D32D3-E179-426A-8424-14720AC7F648}: GameCube
     * {ACC9EAAA-38FC-1749-AE81-64EADBC79353}: PlayStation 2
     * {042D3A45-5FE4-C84B-81F0-DF758B01F273}: Xbox */
    //platf_uuid_1 = read_u64be(0x18, sf);
    //platf_uuid_2 = read_u64be(0x20, sf);
    header_size = read_u32(0x28, sf);

    if (data_offset != header_size)
        goto fail;

    header_name_offset = read_u32(wavedict_offset + 0x04, sf);

    if (header_name_offset) /* not used in Black */
        read_string(header_name, STREAM_NAME_SIZE, header_name_offset, sf);

    if (!target_subsong)
        target_subsong = 1;

    /* Linked lists have no total subsong count; instead iterating
     * through all of them until it returns to the 1st entry again */
    linked_list_offset = wavedict_offset + 0x0C;

    prev_entry_offset = read_u32(linked_list_offset + 0x00, sf);
    next_entry_offset = read_u32(linked_list_offset + 0x04, sf);

    while (next_entry_offset != linked_list_offset) {
        total_subsongs++;

        entry_info_offset = read_u32(next_entry_offset + 0x08, sf);

        if (total_subsongs > 1024 || /* in case it gets stuck in an infinite loop */
            entry_info_offset < wavedict_offset || entry_info_offset > header_size ||
            prev_entry_offset < wavedict_offset || prev_entry_offset > header_size ||
            next_entry_offset < wavedict_offset || next_entry_offset > header_size)
            goto fail;

        prev_entry_offset = read_u32(next_entry_offset + 0x00, sf);
        next_entry_offset = read_u32(next_entry_offset + 0x04, sf);

        /* is at the correct target song index */
        if (total_subsongs == target_subsong) {
            //entry_uuid_offset = read_u32(entry_info_offset + 0x00, sf); /* only used in Burnout games */
            entry_name_offset = read_u32(entry_info_offset + 0x04, sf);

            sample_rate = read_u32(entry_info_offset + 0x10, sf);
            stream_codec = read_u32(entry_info_offset + 0x14, sf);
            stream_size = read_u32(entry_info_offset + 0x18, sf);
            //bit_depth = read_u8(entry_info_offset + 0x1C, sf);
            channels = read_u8(entry_info_offset + 0x1D, sf); /* always 1, don't think stereo entries exist */
            if (channels != 1)
                goto fail;

            /* stores a "00: GCN ADPCM Header" chunk, otherwise empty */
            misc_data_offset = read_u32(entry_info_offset + 0x20, sf);
            //misc_data_size = read_u32(entry_info_offset + 0x24, sf);

            /* entry_info_offset + 0x2C to +0x44 has the target format information,
             * which in most cases would probably be identical to the input format
             * variables (from sample_rate to misc_data_size) */

            stream_offset = read_u32(entry_info_offset + 0x4C, sf) + data_offset;

            read_string(stream_name, STREAM_NAME_SIZE, entry_name_offset, sf);
        }
    }

    if (total_subsongs < 1 || target_subsong > total_subsongs)
        goto fail;

    interleave = 0;
    loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream)
        goto fail;

    vgmstream->meta_type = meta_AWD;
    vgmstream->layout_type = layout_none;
    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = stream_size;
    vgmstream->num_streams = total_subsongs;
    vgmstream->interleave_block_size = interleave;

    if (header_name_offset)
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s/%s", header_name, stream_name);
    else
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", stream_name);

    switch (stream_codec) {
        case 0x00: /* PS2 (Burnout series, Black, Call of Duty: Finest Hour) */
            vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);
            vgmstream->coding_type = coding_PSX;
            break;

        case 0x01: /* Xbox (Burnout series, Black) */
            vgmstream->num_samples = pcm16_bytes_to_samples(stream_size, channels);
            vgmstream->coding_type = coding_PCM16LE;
            break;

        case 0x03: /* GCN (Call of Duty: Finest Hour) */
            vgmstream->num_samples = dsp_bytes_to_samples(stream_size, channels);
            dsp_read_coefs_be(vgmstream, sf, misc_data_offset + 0x1C, 0);
            dsp_read_hist_be(vgmstream, sf, misc_data_offset + 0x40, 0);
            vgmstream->coding_type = coding_NGC_DSP;
            break;

        case 0x04: /* Xbox (Black, Call of Duty: Finest Hour) */
            vgmstream->num_samples = xbox_ima_bytes_to_samples(stream_size, channels);
            vgmstream->coding_type = coding_XBOX_IMA;
            break;

        default:
            VGM_LOG("AWD: unknown codec type %d\n", stream_codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, stream_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
