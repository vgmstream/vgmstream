#include "meta.h"


/* .cks - Cricket Audio stream [Part Time UFO (Android), Mega Man 1-6 (Android)]  */
VGMSTREAM* init_vgmstream_cks(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, codec, sample_rate;
    int32_t num_samples, loop_start, loop_end;
    size_t block_size;


    /* checks */
    if (!is_id32be(0x00,sf, "ckmk"))
        goto fail;
    if (!check_extensions(sf, "cks"))
        goto fail;

    /* 0x04(4): platform bitflags (from LSB: iOS, Android, OS X, Windows, WP8, Linux, tvOS, undefined/ignored) */
    if (read_u32le(0x08,sf) != 0x00) /* expected file type (0x00: stream, 0x01: bank, 0x02+: unknown) */
        goto fail;
    if (read_u32le(0x0c,sf) != 0x02) /* file version (always 0x02) */
        goto fail;

    codec = read_u8(0x10,sf);
    channels = read_u8(0x11,sf);
    sample_rate = read_u16le(0x12,sf);
    num_samples = read_s32le(0x14,sf) * read_u16le(0x1a,sf); /* number_of_blocks * samples_per_frame */
    block_size = read_u16le(0x18,sf);
    /* 0x1c(2): volume */
    /* 0x1e(2): pan */
    loop_start = read_s32le(0x20,sf);
    loop_end = read_s32le(0x24,sf);
    loop_flag = read_s16le(0x28,sf) != 0; /* loop count (-1 = forever) */
    /* 0x2a(2): unused? */

    start_offset = 0x2c;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->meta_type = meta_CKS;

    switch(codec) {
        case 0x00: /* pcm16 [from tests] */
            vgmstream->coding_type = coding_PCM16LE;
            break;
        case 0x01: /* pcm8 [from tests] */
            vgmstream->coding_type = coding_PCM8;
            break;
        case 0x02: /* adpcm [Part Time UFO (Android), Mega Man 1-6 (Android)] */
            vgmstream->coding_type = coding_MSADPCM_ck;
            vgmstream->frame_size = block_size / channels; /* always 0x18 */
            break;
        default:
            goto fail;
    }
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = block_size / channels;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* .ckb - Cricket Audio bank [Fire Emblem Heroes (Android), Mega Man 1-6 (Android)]  */
VGMSTREAM* init_vgmstream_ckb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, name_offset = 0;
    int loop_flag = 0, channels = 0, codec = 0, sample_rate = 0;
    int32_t num_samples = 0, loop_start = 0, loop_end = 0;
    size_t block_size = 0, stream_size = 0;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf, "ckmk"))
        goto fail;
    if (!check_extensions(sf, "ckb"))
        goto fail;

    /* 0x04(4): platform bitflags (from LSB: iOS, Android, OS X, Windows, WP8, Linux, tvOS, undefined/ignored) */
    if (read_u32le(0x08,sf) != 0x01) /* expected file type (0x00: stream, 0x01: bank, 0x02+: unknown) */
        goto fail;
    if (read_u32le(0x0c,sf) != 0x02) /* file version (always 0x02) */
        goto fail;

    /* 0x10: bank name (size 0x1c+1) */
    /* 0x30/34: reserved? */
    total_subsongs = read_u32le(0x38,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    /* 0x3c: total_subsongs again? (ignored) */
    /* 0x40/44: unknown (ignored) */

    /* get subsong (stream offset isn't given so must calc manually) */
    {
        int i;
        off_t header_offset = 0x48;
        off_t stream_offset = 0x48 + total_subsongs*0x48;

        for (i = 0; i < total_subsongs; i++) {
            name_offset = header_offset+0x00; /* stream name (size 0x1c+1) */
            codec = read_8bit(header_offset+0x20,sf);
            channels = read_8bit(header_offset+0x21,sf);
            sample_rate = (uint16_t)read_16bitLE(header_offset+0x22,sf);
            num_samples = read_32bitLE(header_offset+0x24,sf) * read_16bitLE(header_offset+0x2a,sf); /* number_of_blocks * samples_per_frame */
            block_size = read_16bitLE(header_offset+0x28,sf);
            /* 0x2c(2): volume */
            /* 0x2e(2): pan */
            loop_start = read_32bitLE(header_offset+0x30,sf);
            loop_end = read_32bitLE(header_offset+0x34,sf);
            loop_flag = read_16bitLE(header_offset+0x38,sf) != 0; /* loop count (-1 = forever) */
            /* 0x3a(2): unused? */
            stream_size = read_32bitLE(header_offset+0x3c,sf);
            /* 0x40/44(4): unused? */

            if (target_subsong == (i+1))
                break;
            header_offset += 0x48;
            stream_offset += stream_size;
        }

        start_offset = stream_offset;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    read_string(vgmstream->stream_name,0x1c+1, name_offset,sf);

    vgmstream->meta_type = meta_CKB;

    switch(codec) {
        case 0x00: /* pcm16 [Mega Man 1-6 (Android)] */
            vgmstream->coding_type = coding_PCM16LE;
            break;
        case 0x01: /* pcm8 */
            vgmstream->coding_type = coding_PCM8;
            break;
        case 0x02: /* adpcm [Fire Emblem Heroes (Android)] */
            vgmstream->coding_type = coding_MSADPCM_ck;
            /* frame_size is always 0x18 */
            break;
        default:
            goto fail;
    }
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = block_size / channels;

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
