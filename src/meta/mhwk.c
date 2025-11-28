#include "meta.h"
#include "../coding/coding.h"

/* Header size of the main file wrapper (MHWK + Size + WAVE) */
#define MHWK_FILE_HEADER_SIZE 0x0C
/* The 'Data' chunk starts with a 20-byte parameter header before audio begins */
#define MHWK_DATA_HEADER_SIZE 0x14

/* MHWK - Broderbund's Mohawk engine (.mhk) */
VGMSTREAM* init_vgmstream_mhwk(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset = 0;
    int loop_flag = 0, channels = 0, sample_rate = 0, format = 0;
    int32_t num_samples = 0, loop_start = 0, loop_end = 0;
    off_t chunk_offset = 0;
    off_t data_offset = 0;
    uint32_t chunk_id;
    uint32_t chunk_size;

    /* Check for MHWK magic word */
    if (!is_id32be(0x00, sf, "MHWK"))
        return NULL;

    if (!check_extensions(sf, "mhk"))
        return NULL;

    /* Check for WAVE magic word, which follows the MHWK header */
    if (!is_id32be(0x08, sf, "WAVE"))
        goto fail;

    chunk_offset = MHWK_FILE_HEADER_SIZE;

    while (chunk_offset < get_streamfile_size(sf)) {
        chunk_id = read_u32be(chunk_offset, sf);
        chunk_size = read_u32be(chunk_offset + 0x04, sf);
        chunk_offset += 0x08;

        if (chunk_id == get_id32be("Data")) {
            data_offset = chunk_offset;
            break;
        }
        else if (chunk_id == get_id32be("Cue#") || chunk_id == get_id32be("ADPC")) {
            chunk_offset += chunk_size;
            continue;
        }
        else {
            goto fail;
        }
    }

    if (!data_offset)
        goto fail;

    /* Data chunk header */

    /* Header size is consistently 20 bytes from analysis */
    sample_rate = read_u16be(data_offset + 0x00, sf);
    num_samples = read_u32be(data_offset + 0x02, sf);
    /* 0x06: sample width (1 byte), 0x07: channels (1 byte) */
    channels = read_u8(data_offset + 0x07, sf);
    format = read_u16be(data_offset + 0x08, sf);

    if (read_u16be(data_offset + 0x0A, sf) == 0xFFFF) {
        loop_flag = 1;
        loop_start = read_u32be(data_offset + 0x0C, sf);
        loop_end = read_u32be(data_offset + 0x10, sf);
    }

    /* The actual audio data starts after the header's 20 bytes not 28 bytes */
    start_offset = data_offset + MHWK_DATA_HEADER_SIZE;

    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream)
        goto fail;

    vgmstream->meta_type = meta_MHWK;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    /* Determine coding type based on format ID */
    switch (format) {

        //Myst 1993
        case 0x0000: /* 8-bit unsigned PCM. Default */
            vgmstream->coding_type = coding_PCM8_U;
            vgmstream->layout_type = layout_interleave; // Usually mono.
            vgmstream->interleave_block_size = 0x01;
            break;

        //Carmen Sandiego: Word Detective
        case 0x0001: /* Intel DVI ADPCM */
            vgmstream->coding_type = coding_DVI_IMA;
            vgmstream->layout_type = layout_none;
            break;
        //Riven DVD
        case 0x0002: /* MPEG-2 Layer II LSF */
            vgmstream->layout_type = layout_none;
#if defined(VGM_USE_MPEG)
            vgmstream->codec_data = init_mpeg(sf, start_offset, &vgmstream->coding_type, vgmstream->channels);  // Uses MPEG-2 LSF variant.
#elif defined(VGM_USE_FFMPEG)
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->codec_data = init_ffmpeg_offset(sf, start_offset, chunk_size - MHWK_DATA_HEADER_SIZE);
#else
            goto fail;
#endif
            if (!vgmstream->codec_data)
                goto fail;
            break;

        default: /* Unknown format */
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
