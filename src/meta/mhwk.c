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

    /* Check for MHWK magic word */
    if (!is_id32be(0x00, sf, "MHWK"))
        return NULL;

    if (!check_extensions(sf, "mhk"))
        return NULL;

    /* Check for WAVE magic word, which follows the MHWK header */
    if (!is_id32be(0x08, sf, "WAVE"))
        goto fail;

    current_offset = 0x0C;

    while (current_offset < get_streamfile_size(sf)) {
        chunk_id = read_u32be(current_offset, sf);
        chunk_size = read_u32be(current_offset + 0x04, sf);
        current_offset += 0x08;

        if (chunk_id == get_id32be("Data")) {
            break;
        }
        else if (chunk_id == get_id32be("Cue#") || chunk_id == get_id32be("ADPC")) {
            current_offset += chunk_size;
            continue;
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

    /* Discontinuity fix: (8-bit unsigned PCM only) */
    if (format == 0x0000 && num_samples >= 4) {
        const int PCM8_U_SILENCE = 0x80;
        const int SQUELCH = 32; //Threshold of discontinuity before removing. Lower values = increased sensitivity.
        off_t window_offset = start_offset + num_samples - 4;
        uint8_t s[4];
        int is_safe = 0;

        s[0] = read_u8(window_offset + 0, sf);
        s[1] = read_u8(window_offset + 1, sf);
        s[2] = read_u8(window_offset + 2, sf);
        s[3] = read_u8(window_offset + 3, sf);

        /* Path 1: Check for sustained quietness. If all samples in the window
         * are very close to silence, the sound is considered stable and safe. */
        int is_stable_and_quiet = 1;
        for (int i = 0; i < 4; i++) {
            if (abs(s[i] - PCM8_U_SILENCE) > SQUELCH) {
                is_stable_and_quiet = 0;
                break;
            }
        }
        if (is_stable_and_quiet) {
            is_safe = 1;
        }

        /* Path 2: If not stable/quiet, check for a consistent fade-out trend. */
        if (!is_safe) {
            int dist_last = abs(s[3] - PCM8_U_SILENCE);
            int dist_prev = abs(s[2] - PCM8_U_SILENCE);
            int dist_ante = abs(s[1] - PCM8_U_SILENCE);

            if (dist_last < dist_prev && dist_prev < dist_ante) {
                is_safe = 1;
            }
        }

        /* If the file's ending is neither stable nor fading, apply the fix. */
        if (!is_safe) {
            VGM_LOG("MHWK: Discontinuity detected at sample %i (offset 0x%08x). Final samples: 0x%02x 0x%02x 0x%02x 0x%02x. Truncating one sample.\n",
                    num_samples, (uint32_t)window_offset, s[0], s[1], s[2], s[3]);

            num_samples--;
            if (loop_flag && loop_end > num_samples) {
                loop_end = num_samples;
            }
        }
    }

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
        case 0x0001: /* IMA ADPCM */
            vgmstream->coding_type = coding_IMA;
            vgmstream->layout_type = layout_none;
            break;
        //Riven DVD
        case 0x0002: /* MPEG Layer II */
#if defined(VGM_USE_MPEG)
            vgmstream->coding_type = coding_MPEG_layer2;
#elif defined(VGM_USE_FFMPEG)
            vgmstream->coding_type = coding_FFmpeg;
#else
            goto fail;
#endif
            vgmstream->layout_type = layout_none;
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
