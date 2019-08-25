#include "coding.h"

#ifdef VGM_USE_FFMPEG


/* init ATRAC3/plus while adding some fixes */
ffmpeg_codec_data * init_ffmpeg_atrac3_riff(STREAMFILE *sf, off_t offset, int* out_samples) {
    ffmpeg_codec_data *ffmpeg_data = NULL;
    int is_at3 = 0, is_at3p = 0, codec;
    size_t riff_size;
    int fact_samples, skip_samples, implicit_skip;
    off_t fact_offset = 0;
    size_t fact_size = 0;


    /* some simplified checks just in case */
    if (read_32bitBE(offset + 0x00,sf) != 0x52494646) /* "RIFF" */
        goto fail;

    riff_size = read_32bitLE(offset + 0x04,sf) + 0x08;
    codec = (uint16_t)read_16bitLE(offset + 0x14, sf);
    switch(codec) {
        case 0x0270: is_at3 = 1; break;
        case 0xFFFE: is_at3p = 1; break;
        default: goto fail;
    }


    /* init file  + apply fixes to FFmpeg decoding (with these fixes should be
     * sample-accurate vs official tools, except usual +-1 float-to-pcm conversion) */
    ffmpeg_data = init_ffmpeg_offset(sf, offset, riff_size);
    if (!ffmpeg_data) goto fail;


    /* well behaved .at3 define "fact" but official tools accept files without it */
    if (find_chunk_le(sf,0x66616374,offset + 0x0c,0, &fact_offset, &fact_size)) { /* "fact" */
        if (fact_size == 0x08) { /* early AT3 (mainly PSP games) */
            fact_samples = read_32bitLE(fact_offset + 0x00, sf);
            skip_samples = read_32bitLE(fact_offset + 0x04, sf); /* base skip samples */
        }
        else if (fact_size == 0x0c) { /* late AT3 (mainly PS3 games and few PSP games) */
            fact_samples = read_32bitLE(fact_offset + 0x00, sf);
            /* 0x04: base skip samples, ignored by decoder */
            skip_samples = read_32bitLE(fact_offset + 0x08, sf); /* skip samples with implicit skip of 184 added */
        }
        else {
            VGM_LOG("ATRAC3: unknown fact size\n");
            goto fail;
        }
    }
    else {
        fact_samples = 0; /* tools output 0 samples in this case unless loop end is defined */
        if (is_at3)
            skip_samples = 1024; /* 1 frame */
        else if (is_at3p)
            skip_samples = 2048; /* 1 frame */
        else
            skip_samples = 0;
    }

    /* implicit skip: official tools skip this even with encoder delay forced to 0. Maybe FFmpeg decodes late,
     * but when forcing tools to decode all frame samples it always ends a bit before last frame, so maybe it's
     * really an internal skip, since encoder adds extra frames so fact num_samples + encoder delay + implicit skip
     * never goes past file. Same for all bitrate/channels, not added to loops. */
    if (is_at3) {
        implicit_skip = 69;
    }
    else if (is_at3p && fact_size == 0x08) {
        implicit_skip = 184*2;
    }
    else if (is_at3p && fact_size == 0x0c) {
        implicit_skip = 184; /* first 184 is already added to delay vs field at 0x08 */
    }
    else if (is_at3p) {
        implicit_skip = 184; /* default for unknown sizes */
    }
    else {
        implicit_skip = 0;
    }

    /* FFmpeg reads this but just in case they fiddle with it in the future */
    ffmpeg_data->totalSamples = fact_samples;

    /* encoder delay: encoder introduces some garbage (not always silent) samples to skip at the beginning (at least 1 frame)
     * FFmpeg doesn't set this, and even if it ever does it's probably better to force it for the implicit skip. */
    ffmpeg_set_skip_samples(ffmpeg_data, skip_samples + implicit_skip);

    /* invert ATRAC3: waveform is inverted vs official tools (not noticeable but for accuracy) */
    if (is_at3) {
        ffmpeg_data->invert_audio_set = 1;
    }

    /* multichannel fix: LFE channel should be reordered on decode (ATRAC3Plus only, only 1/2/6/8ch exist):
     * - 6ch: FL FR FC BL BR LFE > FL FR FC LFE BL BR
     * - 8ch: FL FR FC BL BR SL SR LFE > FL FR FC LFE BL BR SL SR */
    if (is_at3p && ffmpeg_data->channels == 6) {
        /* LFE BR BL > LFE BL BR > same */
        int channel_remap[] = { 0, 1, 2, 5, 5, 5, };
        ffmpeg_set_channel_remapping(ffmpeg_data, channel_remap);
    }
    else if (is_at3p && ffmpeg_data->channels == 8) {
        /* LFE BR SL SR BL > LFE BL SL SR BR > LFE BL BR SR SL > LFE BL BR SL SR > same */
        int channel_remap[] = { 0, 1, 2, 7, 7, 7, 7, 7};
        ffmpeg_set_channel_remapping(ffmpeg_data, channel_remap);
    }


    if (out_samples)
        *out_samples = fact_samples;

    return ffmpeg_data;
fail:
    return NULL;
}



#endif
