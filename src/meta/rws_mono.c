#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* RWS - RenderWare Stream (with the tag 0x809) */
VGMSTREAM* init_vgmstream_rws_mono(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    bool big_endian;
    char header_name[STREAM_NAME_SIZE], stream_name[STREAM_NAME_SIZE];
    int bit_depth = 0, channels = 0, idx, interleave, loop_flag, sample_rate = 0, total_subsongs, target_subsong = sf->stream_index;
    read_u32_t read_u32;
    off_t chunk_offset, header_offset, stream_offset = 0;
    size_t chunk_size, header_size, stream_size = 0;


    /* checks */
    if (read_u32le(0x00, sf) != 0x809) /* File ID */
        goto fail;
    if (read_u32le(0x04, sf) + 0x0C != get_streamfile_size(sf))
        goto fail;
    /* Burnout 2: Point of Impact (PS2, GCN, Xbox):
     * Predecessor to the common 0x80D-0x80F tag rws.c (which is also used in B2)
     * with some parts of it later reworked into awd.c seemingly */
    if (!check_extensions(sf, "rws"))
        goto fail;

    /* Uses various chunk IDs across the file:
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
            header_size = read_u32le(header_offset + 0x04, sf); /* usually 0xA0 */

            sample_rate = read_u32(header_offset + 0x10, sf);
            stream_size = read_u32(header_offset + 0x18, sf);
            bit_depth = read_u8(header_offset + 0x1C, sf);
            channels = read_u8(header_offset + 0x1D, sf); /* always 1? */
            if (channels != 1)
                goto fail;

            /* Assumed misc data offs/size at header_offset + 0x20 to +0x24 
             * which is always empty since GCN uses PCM S16BE encoding here */

            read_string(stream_name, STREAM_NAME_SIZE, header_offset + 0x7C, sf);

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

    /* Likely unreliable, but currently only can be tested with Burnout 2 */
    switch (bit_depth) {
        case 4: /* PS-ADPCM, normally DSP-ADPCM would be 4 too (as is in awd.c) but GCN uses PCM */
            vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);
            vgmstream->coding_type = coding_PSX;
            break;

        case 16: /* PCM Signed 16-bit */
            vgmstream->num_samples = pcm16_bytes_to_samples(stream_size, channels);
            vgmstream->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
            break;

        default:
            goto fail;
    }

    snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "%s/%s", header_name, stream_name);

    if (!vgmstream_open_stream(vgmstream, sf, stream_offset + 0x0C))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}