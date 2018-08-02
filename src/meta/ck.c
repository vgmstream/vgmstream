#include "meta.h"


/* .cks - Cricket Audio stream [Part Time UFO (Android), Mega Man 1-6 (Android)]  */
VGMSTREAM * init_vgmstream_cks(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, codec, sample_rate;
    int32_t num_samples, loop_start, loop_end;
    size_t block_size;


    /* checks */
    if (!check_extensions(streamFile, "cks"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x636B6D6B) /* "ckmk" */
        goto fail;
    /* 0x04(4): platform bitflags (from LSB: iOS, Android, OS X, Windows, WP8, Linux, tvOS, undefined/ignored) */
    if (read_32bitLE(0x08,streamFile) != 0x00) /* expected file type (0x00: stream, 0x01: bank, 0x02+: unknown) */
        goto fail;
    if (read_32bitLE(0x0c,streamFile) != 0x02) /* file version (always 0x02) */
        goto fail;

    codec = read_8bit(0x10,streamFile);
    channel_count = read_8bit(0x11,streamFile);
    sample_rate = (uint16_t)read_16bitLE(0x12,streamFile);
    num_samples = read_32bitLE(0x14,streamFile) * read_16bitLE(0x1a,streamFile); /* number_of_blocks * samples_per_frame */
    block_size = read_16bitLE(0x18,streamFile);
    /* 0x1c(2): volume */
    /* 0x1e(2): pan */
    loop_start = read_32bitLE(0x20,streamFile);
    loop_end = read_32bitLE(0x24,streamFile);
    loop_flag = read_16bitLE(0x28,streamFile) != 0; /* loop count (-1 = forever) */
    /* 0x2a(2): unused? */

    start_offset = 0x2c;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
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
            /* frame_size is always 0x18 */
            break;
        default:
            goto fail;
    }
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = block_size / channel_count;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* .ckb - Cricket Audio bank [Fire Emblem Heroes (Android), Mega Man 1-6 (Android)]  */
VGMSTREAM * init_vgmstream_ckb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, name_offset = 0;
    int loop_flag, channel_count, codec, sample_rate;
    int32_t num_samples, loop_start, loop_end;
    size_t block_size, stream_size;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "ckb"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x636B6D6B) /* "ckmk" */
        goto fail;
    /* 0x04(4): platform bitflags (from LSB: iOS, Android, OS X, Windows, WP8, Linux, tvOS, undefined/ignored) */
    if (read_32bitLE(0x08,streamFile) != 0x01) /* expected file type (0x00: stream, 0x01: bank, 0x02+: unknown) */
        goto fail;
    if (read_32bitLE(0x0c,streamFile) != 0x02) /* file version (always 0x02) */
        goto fail;

    /* 0x10: bank name (size 0x1c+1) */
    /* 0x30/34: reserved? */
    total_subsongs = read_32bitLE(0x38,streamFile);
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
            codec = read_8bit(header_offset+0x20,streamFile);
            channel_count = read_8bit(header_offset+0x21,streamFile);
            sample_rate = (uint16_t)read_16bitLE(header_offset+0x22,streamFile);
            num_samples = read_32bitLE(header_offset+0x24,streamFile) * read_16bitLE(header_offset+0x2a,streamFile); /* number_of_blocks * samples_per_frame */
            block_size = read_16bitLE(header_offset+0x28,streamFile);
            /* 0x2c(2): volume */
            /* 0x2e(2): pan */
            loop_start = read_32bitLE(header_offset+0x30,streamFile);
            loop_end = read_32bitLE(header_offset+0x34,streamFile);
            loop_flag = read_16bitLE(header_offset+0x38,streamFile) != 0; /* loop count (-1 = forever) */
            /* 0x3a(2): unused? */
            stream_size = read_32bitLE(header_offset+0x3c,streamFile);
            /* 0x40/44(4): unused? */

            if (target_subsong == (i+1))
                break;
            header_offset += 0x48;
            stream_offset += stream_size;
        }

        start_offset = stream_offset;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    read_string(vgmstream->stream_name,0x1c+1, name_offset,streamFile);

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
    vgmstream->interleave_block_size = block_size / channel_count;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
