#include "meta.h"
#include "../coding/coding.h"


/* .STR - Sega Dreamcast's "Audio64 AM (AICA Manager) driver" streams */
VGMSTREAM* init_vgmstream_str_sega(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size;
    int loop_flag, channels, track_channels, tracks, sample_rate, bps, interleave, blocks;


    /* checks */
    tracks = read_u32le(0x00,sf);
    if (tracks < 1 || tracks > 16) /* official limits */
        return NULL;

    /* "Sega Stream Asset Builder Revision ..." long text with program info, after header */
    if (!is_id64be(0xD4,sf, "\0Sega St"))
        return NULL;

    if (!check_extensions(sf, "str"))
        return NULL;

    sample_rate = read_s32le(0x04,sf);
    bps = read_u32le(0x08,sf);
    interleave = read_s32le(0x0C,sf); /* multiples of 0x800 (sector size) */
    blocks = read_s32le(0x10,sf); /* interruptsTillEnd, "how may irqs until the end of that tracks data" AKA interleaved blocks */

    /* per track info (channels must be the same per track, and data_size may be inaccurate for other tracks) */
    data_size = read_u32le(0x14,sf);
    track_channels = read_s32le(0x18,sf); /* max 2 for AICA, N for PCM */
    /* 0x08: blocks / interruptsTillEnd (not including padding) */
    /* header track data reserved up to max 16 * 0x0c = 0xC0 + 0x18 = 0xD4 */
    /* after track headers there may be one phantom entry with empty data/channels but repeated interruptsTillEnd (bug?) */

    channels = track_channels * tracks;

    loop_flag = 0; /* no loop info */
    start_offset = 0x800; 

    /* all tracks/data is padded */
    if (blocks * channels * interleave != get_streamfile_size(sf) - start_offset)
        goto fail;

    /* streams start with a "leadIn" silence that defaults to 2 blocks, but it's configurable */
    //start_offset += (interleave * channels) * 2;


    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_STR_SEGA;
    vgmstream->sample_rate = sample_rate;

    switch (bps) {
        case 4: /* common [GTA2 (DC)] */
            vgmstream->coding_type = coding_AICA_int;
            vgmstream->num_samples = yamaha_bytes_to_samples(data_size, track_channels);

            for (int i = 0; i < channels; i++) {
                vgmstream->ch[i].adpcm_step_index = 0x7f;
            }
            break;

        case 8: /* not seen (encoder only) */
            vgmstream->coding_type = coding_PCM8_U;
            vgmstream->num_samples = pcm8_bytes_to_samples(data_size, track_channels);
            break;

        case 16: /* common [San Francisco Rush 2049 (DC)] */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->num_samples = pcm16_bytes_to_samples(data_size, track_channels);
            break;

        default:
            goto fail;
    }

    vgmstream->interleave_block_size = interleave;
    vgmstream->layout_type = layout_interleave;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .STR - mutant Sega Dreamcast stream [102 Dalmatians: Puppies to the Rescue (DC)] */
VGMSTREAM* init_vgmstream_str_sega_custom(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate, interleave;
    uint32_t data_size;

    /* checks */
    if (read_u32le(0x00,sf) != 0x2)
        goto fail;

    if (read_u64be(0xD4,sf) != 0x00)
        return NULL;

    if (!check_extensions(sf, "str"))
        return NULL;

    /* this looks vaguely like a STR and could be a fully new thing, but game does include Audio64.drv */

    loop_flag = 0;
    channels = 2;
    sample_rate = read_s32le(0x04,sf);
    if (read_u32le(0x08,sf) != 16)
        return NULL;
    interleave = read_s32le(0x0c,sf);
    if (read_u32le(0x10,sf) != 0x10000 ||
        read_u32le(0x14,sf) != 0x00 ||
        read_u32le(0x18,sf) != 0x00 ||
        read_u32le(0x1C,sf) != 0x1F)
        return NULL;

    start_offset = 0x800;
    data_size = get_streamfile_size(sf) - start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_STR_SEGA_custom;
    vgmstream->sample_rate =sample_rate;
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->num_samples = pcm16_bytes_to_samples(data_size, channels);
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
