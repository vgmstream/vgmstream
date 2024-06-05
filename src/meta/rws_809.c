#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* RWS - RenderWare Stream (with the tag 0x809) */
VGMSTREAM* init_vgmstream_rws_809(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    bool big_endian;
    char file_name[STREAM_NAME_SIZE], header_name[STREAM_NAME_SIZE], stream_name[STREAM_NAME_SIZE];
    int channels = 0, idx, interleave, loop_flag, sample_rate = 0, total_subsongs, target_subsong = sf->stream_index;
    read_u32_t read_u32;
    off_t chunk_offset, header_offset, misc_data_offset = 0, stream_name_offset, stream_offset = 0;
    size_t chunk_size, header_size, misc_data_size, stream_size = 0;
    uint32_t codec_uuid = 0;


    /* checks */
    if (read_u32le(0x00, sf) != 0x809) /* File ID */
        goto fail;

    /* Burnout 2: Point of Impact (PS2, GCN, Xbox), Neighbours from Hell (GCN, Xbox):
     * Predecessor to the common 0x80D-0x80F tag RWS with some parts reworked into AWD? */
    if (!check_extensions(sf, "rws"))
        goto fail;

    /* Uses the following chunk IDs across the file:
     * 0x00000809: File ID
     * 0x0000080A: File header ID
     * 0x0000080C: File data ID
     *
     * 0x00000802: Stream ID
     * 0x00000803: Stream header ID
     * 0x00000804: Stream data ID
     */

    chunk_offset = 0x0C;
    if (read_u32le(chunk_offset, sf) != 0x80A) /* File header ID */
        goto fail;
    chunk_size = read_u32le(chunk_offset + 0x04, sf); /* usually 0x44 */

    read_string(header_name, STREAM_NAME_SIZE, chunk_offset + 0x40, sf);

    chunk_offset += chunk_size + 0x0C;
    if (read_u32le(chunk_offset, sf) != 0x80C) /* File data ID */
        goto fail;

    big_endian = guess_endian32(chunk_offset + 0x0C, sf);
    read_u32 = big_endian ? read_u32be : read_u32le;

    total_subsongs = read_u32(chunk_offset + 0x0C, sf);

    if (!target_subsong)
        target_subsong = 1;

    chunk_offset += 0x10;
    for (idx = 1; idx <= total_subsongs; idx++) {
        if (read_u32le(chunk_offset, sf) != 0x802) /* Stream ID */
            goto fail;
        chunk_size = read_u32le(chunk_offset + 0x04, sf);

        if (idx == target_subsong) {
            header_offset = chunk_offset + 0x0C;
            if (read_u32le(header_offset, sf) != 0x803) /* Stream header ID */
                goto fail;
            header_size = read_u32le(header_offset + 0x04, sf);

            sample_rate = read_u32(header_offset + 0x10, sf);
            stream_size = read_u32(header_offset + 0x18, sf);
            //bit_depth = read_u8(header_offset + 0x1C, sf);
            channels = read_u8(header_offset + 0x1D, sf); /* always 1? */
            if (channels != 1)
                goto fail;

            //misc_data_offset = read_u32(header_offset + 0x20, sf); /* assumed, but wrong? */
            misc_data_offset = header_offset + 0x3C;
            misc_data_size = read_u32(header_offset + 0x24, sf); /* 0x60 in GCN if DSP-ADPCM, otherwise 0 */

            codec_uuid = read_u32(header_offset + 0x2C, sf);

            /* (header_offset + 0x3C + misc_data_size) + 0x00 to +0x18 has the target format
             * info which in most cases would probably be identical to the input format info */

            /* (misc_data_size * 2) should be 0xC0 if it exists */
            stream_name_offset = header_offset + 0x7C + (misc_data_size * 2);
            read_string(stream_name, STREAM_NAME_SIZE, stream_name_offset, sf);

            stream_offset = header_offset + header_size + 0x0C;
            if (read_u32le(stream_offset, sf) != 0x804) /* Stream data ID */
                goto fail;
            if (read_u32le(stream_offset + 0x04, sf) != stream_size)
                goto fail;
        }
        chunk_offset += chunk_size + 0x0C;
    }

    if (total_subsongs < 1 || target_subsong > total_subsongs)
        goto fail;

    interleave = 0;
    loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream)
        goto fail;

    vgmstream->meta_type = meta_RWS;
    vgmstream->layout_type = layout_none;
    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = stream_size;
    vgmstream->num_streams = total_subsongs;
    vgmstream->interleave_block_size = interleave;

    /* should be the same as in rws_80d, maybe merge the two switches into one function? */
    /* the full list of all the valid codec UUIDs can also be found listed in rws_80d */
    switch (codec_uuid) {
        case 0xD01BD217: /* {D01BD217-3587-4EED-B9D9-B8E86EA9B995}: PCM Signed 16-bit */
            vgmstream->num_samples = pcm16_bytes_to_samples(stream_size, channels);
            vgmstream->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
            break;

        case 0xD9EA9798: /* {D9EA9798-BBBC-447B-96B2-654759102E16}: PSX-ADPCM */
            vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);
            vgmstream->coding_type = coding_PSX;
            break;

        case 0xF86215B0: /* {F86215B0-31D5-4C29-BD37-CDBF9BD10C53}: DSP-ADPCM */
            vgmstream->num_samples = dsp_bytes_to_samples(stream_size, channels);
            dsp_read_coefs_be(vgmstream, sf, misc_data_offset + 0x1C, 0);
            dsp_read_hist_be(vgmstream, sf, misc_data_offset + 0x40, 0);
            vgmstream->coding_type = coding_NGC_DSP;
            break;

        default:
            VGM_LOG("RWS: unknown codec 0x%08x\n", codec_uuid);
            goto fail;
    }

    get_streamfile_basename(sf, file_name, STREAM_NAME_SIZE);
    if (strcmp(file_name, header_name) == 0)
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s", stream_name);
    else
        snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s/%s", header_name, stream_name);

    if (!vgmstream_open_stream(vgmstream, sf, stream_offset + 0x0C))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
