#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"

#define MAX_SEGMENTS 4

/* .WAVE - "EngineBlack" games, segmented [Shantae and the Pirate's Curse (PC/3DS), TMNT: Danger of the Ooze (PS3/3DS)] */
VGMSTREAM * init_vgmstream_wave_segmented(STREAMFILE *sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t segments_offset;
    int loop_flag = 0, channel_count, sample_rate;
    int32_t num_samples, loop_start_sample = 0, loop_end_sample = 0;

    segmented_layout_data *data = NULL;
    int segment_count, loop_start_segment = 0, loop_end_segment = 0;

    int big_endian;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;


    /* checks */
    if (!check_extensions(sf, "wave"))
        goto fail;

    if (read_32bitLE(0x00,sf) != 0x4DF72D4A &&  /* header id */
        read_32bitBE(0x00,sf) != 0x4DF72D4A)
        goto fail;
    if (read_8bit(0x04,sf) != 0x01) /* version? */
        goto fail;

    /* PS3/X360 games */
    big_endian = read_32bitBE(0x00,sf) == 0x4DF72D4A;
    if (big_endian) {
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }

    channel_count = read_8bit(0x05,sf);
    segment_count = read_16bit(0x06,sf);
    if (segment_count > MAX_SEGMENTS || segment_count <= 0) goto fail;

    loop_start_segment = read_16bit(0x08, sf);
    loop_end_segment   = read_16bit(0x0a, sf);
    segments_offset = read_32bit(0x0c, sf);

    sample_rate = read_32bit(0x10, sf);
    num_samples = read_32bit(0x14, sf);
    /* 0x18: unknown (usually 0, maybe some count) */


    /* init layout */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    /* parse segments (usually: preload + intro + loop + ending, intro/ending may be skipped)
     * Often first segment is ADPCM and rest Ogg; may only have one segment. */
    {
        off_t extradata_offset, table_offset, segment_offset;
        size_t segment_size;
        int32_t segment_samples;
        int codec;
        int i, ch;

        /* open each segment subfile */
        for (i = 0; i < segment_count; i++) {
            codec = read_8bit(segments_offset+0x10*i+0x00, sf);
            /* 0x01(1): unknown (flag? usually 0x00/0x01/0x02) */
            if (read_8bit(segments_offset+0x10*i+0x02, sf) != 0x01) goto fail; /* unknown */
            if (read_8bit(segments_offset+0x10*i+0x03, sf) != 0x00) goto fail; /* unknown */

            segment_samples  = read_32bit(segments_offset+0x10*i+0x04, sf);
            extradata_offset = read_32bit(segments_offset+0x10*i+0x08, sf);
            table_offset     = read_32bit(segments_offset+0x10*i+0x0c, sf);

            /* create a sub-VGMSTREAM per segment
             * (we'll reopen this sf as needed, so each sub-VGMSTREAM is fully independent) */
            switch(codec) {
                case 0x02: { /* "adpcm" */
                    data->segments[i] = allocate_vgmstream(channel_count, 0);
                    if (!data->segments[i]) goto fail;

                    data->segments[i]->sample_rate = sample_rate;
                    data->segments[i]->meta_type = meta_WAVE;
                    data->segments[i]->coding_type = coding_IMA_int;
                    data->segments[i]->layout_type = layout_none;
                    data->segments[i]->num_samples = segment_samples;

                    if (!vgmstream_open_stream(data->segments[i],sf,0x00))
                        goto fail;

                    /* bizarrely enough channel data isn't sequential (segment0 ch1+ may go after all other segments) */
                    for (ch = 0; ch < channel_count; ch++) {
                        segment_offset = read_32bit(table_offset + 0x04*ch, sf);
                        data->segments[i]->ch[ch].channel_start_offset =
                                data->segments[i]->ch[ch].offset = segment_offset;

                        /* ADPCM setup */
                        data->segments[i]->ch[ch].adpcm_history1_32 = read_16bit(extradata_offset+0x04*ch+0x00, sf);
                        data->segments[i]->ch[ch].adpcm_step_index  = read_8bit(extradata_offset+0x04*ch+0x02, sf);
                        /* 0x03: reserved */
                    }

                    break;
                }

                case 0x03: { /* "dsp-adpcm" */
                    data->segments[i] = allocate_vgmstream(channel_count, 0);
                    if (!data->segments[i]) goto fail;

                    data->segments[i]->sample_rate = sample_rate;
                    data->segments[i]->meta_type = meta_WAVE;
                    data->segments[i]->coding_type = coding_NGC_DSP;
                    data->segments[i]->layout_type = layout_none;
                    data->segments[i]->num_samples = segment_samples;

                    if (!vgmstream_open_stream(data->segments[i],sf,0x00))
                        goto fail;

                    /* bizarrely enough channel data isn't sequential (segment0 ch1+ may go after all other segments) */
                    for (ch = 0; ch < channel_count; ch++) {
                        segment_offset = read_32bit(table_offset + 0x04*ch, sf);
                        data->segments[i]->ch[ch].channel_start_offset =
                                data->segments[i]->ch[ch].offset = segment_offset;
                    }

                    /* ADPCM setup: 0x06 initial ps/hist1/hist2 (per channel) + 0x20 coefs (per channel) */
                    dsp_read_hist(data->segments[i], sf, extradata_offset+0x02, 0x06, big_endian);
                    dsp_read_coefs(data->segments[i], sf, extradata_offset+0x06*channel_count+0x00, 0x20, big_endian);

                    break;
                }

#ifdef VGM_USE_VORBIS
                case 0x04: { /* "vorbis" */
                    ogg_vorbis_meta_info_t ovmi = {0};

                    segment_offset = read_32bit(table_offset, sf);
                    segment_size = read_32bitBE(segment_offset, sf); /* always BE */

                    ovmi.meta_type = meta_WAVE;
                    ovmi.stream_size = segment_size;

                    data->segments[i] = init_vgmstream_ogg_vorbis_config(sf, segment_offset+0x04, &ovmi);
                    if (!data->segments[i]) goto fail;

                    if (data->segments[i]->num_samples != segment_samples) {
                        VGM_LOG("WAVE: segment %i samples != num_samples\n", i);
                        goto fail;
                    }

                    break;
                }
#endif

                default: /* others: s16be/s16le/mp3 as referenced in the exe? */
                    VGM_LOG("WAVE: unknown codec\n");
                    goto fail;
            }
        }
    }

    /* setup segmented VGMSTREAMs */
    if (!setup_layout_segmented(data))
        goto fail;


    /* parse samples */
    {
        int32_t sample_count = 0;
        int i;

        loop_flag = (loop_start_segment > 0);

        for (i = 0; i < segment_count; i++) {
            if (loop_flag && loop_start_segment == i) {
                loop_start_sample = sample_count;
            }

            sample_count += data->segments[i]->num_samples;

            if (loop_flag && loop_end_segment-1 == i) {
                loop_end_sample = sample_count;
            }
        }

        if (sample_count != num_samples) {
            VGM_LOG("WAVE: total segments samples %i != num_samples %i\n", sample_count, num_samples);
            goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;

    vgmstream->meta_type = meta_WAVE_segmented;
    vgmstream->stream_size = get_streamfile_size(sf); /* wrong kbps otherwise */

    /* .wave can mix codecs, usually first segment is a small ADPCM section) */
    vgmstream->coding_type = (segment_count == 1 ? data->segments[0]->coding_type : data->segments[1]->coding_type);
    vgmstream->layout_type = layout_segmented;
    vgmstream->layout_data = data;

    return vgmstream;

fail:
    free_layout_segmented(data);
    close_vgmstream(vgmstream);
    return NULL;
}
