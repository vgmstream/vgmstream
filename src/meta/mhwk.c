#include "meta.h"
#include "../coding/coding.h"

/* MHWK - Broderbund's Mohawk engine (.mhk) */
VGMSTREAM* init_vgmstream_mhwk(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset = 0;
    int loop_flag = 0, channels = 0, sample_rate = 0, format = 0;
    int32_t num_samples = 0, loop_start = 0, loop_end = 0;
    off_t current_offset;
    uint32_t chunk_id;
    uint32_t chunk_size;

    if (!check_extensions(sf, "mhk"))
        goto fail;

    /* Check for MHWK magic word */
    if (read_u32be(0x00, sf) != 0x4D48574B) /* "MHWK" */
        goto fail;

    /* Check for WAVE magic word, which follows the MHWK header */
    if (read_u32be(0x08, sf) != 0x57415645) /* "WAVE" */
        goto fail;

    current_offset = 0x0C;

    while (current_offset < get_streamfile_size(sf)) {
        chunk_id = read_u32be(current_offset, sf);
        chunk_size = read_u32be(current_offset + 0x04, sf);
        current_offset += 0x08;

        if (chunk_id == 0x43756523) { /* "Cue#" */
            current_offset += chunk_size;
            continue;
        }
        else if (chunk_id == 0x41445043) { /* "ADPC" */
            current_offset += chunk_size;
            continue;
        }
        else if (chunk_id == 0x44617461) { /* "Data" */
            break;
        }
        else {
            goto fail;
        }
    }

    /* Data chunk header */

    /* Header size is consistently 20 bytes from analysis */
    sample_rate = read_u16be(current_offset + 0x00, sf);
    num_samples = read_u32be(current_offset + 0x02, sf);
    /* 0x06: sample width (1 byte), 0x07: channels (1 byte) */
    channels = read_u8(current_offset + 0x07, sf);
    format = read_u16be(current_offset + 0x08, sf);

    if (read_u16be(current_offset + 0x0A, sf) == 0xFFFF) {
        loop_flag = 1;
        loop_start = read_u32be(current_offset + 0x0C, sf);
        loop_end = read_u32be(current_offset + 0x10, sf);
    }

    /* The actual audio data starts after the header's 20 bytes not 28 bytes */
    start_offset = current_offset + 0x14;

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
        case 0x0001: /* IMA ADPCM */
            vgmstream->coding_type = coding_IMA;
            vgmstream->layout_type = layout_none;
            break;

        default:
            vgmstream->coding_type = coding_PCM8_U;
            vgmstream->layout_type = layout_none;
            break;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

