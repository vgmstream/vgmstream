#include <ctype.h>
#include "info.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "mixing.h"
#include "../util/channel_mappings.h"
#include "../util/sf_utils.h"

#define TEMPSIZE (256+32)

/*******************************************************************************/
/* TEXT                                                                        */
/*******************************************************************************/

static void describe_get_time(int32_t samples, int sample_rate, double* p_time_mm, double* p_time_ss) {
    double seconds = (double)samples / sample_rate;
    *p_time_mm = (int)(seconds / 60.0);
    *p_time_ss = seconds - *p_time_mm * 60.0;
    if (*p_time_ss >= 59.999) /* avoid round up to 60.0 when printing to %06.3f */
        *p_time_ss = 59.999;
}

/* Write a description of the stream into array pointed by desc, which must be length bytes long.
 * Will always be null-terminated if length > 0 */
void describe_vgmstream(VGMSTREAM* vgmstream, char* desc, int length) {
    char temp[TEMPSIZE];
    double time_mm, time_ss;

    desc[0] = '\0';

    if (!vgmstream) {
        snprintf(temp,TEMPSIZE, "NULL VGMSTREAM");
        concatn(length,desc,temp);
        return;
    }

    snprintf(temp,TEMPSIZE, "sample rate: %d Hz\n", vgmstream->sample_rate);
    concatn(length,desc,temp);

    snprintf(temp,TEMPSIZE, "channels: %d\n", vgmstream->channels);
    concatn(length,desc,temp);

    {
        int output_channels = 0;
        mixing_info(vgmstream, NULL, &output_channels);

        if (output_channels != vgmstream->channels) {
            snprintf(temp,TEMPSIZE, "input channels: %d\n", vgmstream->channels); /* repeated but mainly for plugins */
            concatn(length,desc,temp);
            snprintf(temp,TEMPSIZE, "output channels: %d\n", output_channels);
            concatn(length,desc,temp);
        }
    }

    if (vgmstream->channel_layout) {
        uint32_t cl = vgmstream->channel_layout;

        /* not "channel layout: " to avoid mixups with "layout: " */
        snprintf(temp,TEMPSIZE, "channel mask: 0x%x /", vgmstream->channel_layout);
        concatn(length,desc,temp);
        if (cl & speaker_FL)    concatn(length,desc," FL");
        if (cl & speaker_FR)    concatn(length,desc," FR");
        if (cl & speaker_FC)    concatn(length,desc," FC");
        if (cl & speaker_LFE)   concatn(length,desc," LFE");
        if (cl & speaker_BL)    concatn(length,desc," BL");
        if (cl & speaker_BR)    concatn(length,desc," BR");
        if (cl & speaker_FLC)   concatn(length,desc," FLC"); //FCL is also common
        if (cl & speaker_FRC)   concatn(length,desc," FRC"); //FCR is also common
        if (cl & speaker_BC)    concatn(length,desc," BC");
        if (cl & speaker_SL)    concatn(length,desc," SL");
        if (cl & speaker_SR)    concatn(length,desc," SR");
        if (cl & speaker_TC)    concatn(length,desc," TC");
        if (cl & speaker_TFL)   concatn(length,desc," TFL");
        if (cl & speaker_TFC)   concatn(length,desc," TFC");
        if (cl & speaker_TFR)   concatn(length,desc," TFR");
        if (cl & speaker_TBL)   concatn(length,desc," TBL");
        if (cl & speaker_TBC)   concatn(length,desc," TBC");
        if (cl & speaker_TBR)   concatn(length,desc," TBR");
        concatn(length,desc,"\n");
    }

    /* times mod sounds avoid round up to 60.0 */
    if (vgmstream->loop_start_sample >= 0 && vgmstream->loop_end_sample > vgmstream->loop_start_sample) {
        if (!vgmstream->loop_flag) {
            concatn(length,desc,"looping: disabled\n");
        }

        describe_get_time(vgmstream->loop_start_sample, vgmstream->sample_rate, &time_mm, &time_ss);
        snprintf(temp,TEMPSIZE, "loop start: %d samples (%1.0f:%06.3f seconds)\n", vgmstream->loop_start_sample, time_mm, time_ss);
        concatn(length,desc,temp);

        describe_get_time(vgmstream->loop_end_sample, vgmstream->sample_rate, &time_mm, &time_ss);
        snprintf(temp,TEMPSIZE, "loop end: %d samples (%1.0f:%06.3f seconds)\n", vgmstream->loop_end_sample, time_mm, time_ss);
        concatn(length,desc,temp);
    }

    describe_get_time(vgmstream->num_samples, vgmstream->sample_rate, &time_mm, &time_ss);
    snprintf(temp,TEMPSIZE, "stream total samples: %d (%1.0f:%06.3f seconds)\n", vgmstream->num_samples, time_mm, time_ss);
    concatn(length,desc,temp);

    snprintf(temp,TEMPSIZE, "encoding: ");
    concatn(length,desc,temp);
    get_vgmstream_coding_description(vgmstream, temp, TEMPSIZE);
    concatn(length,desc,temp);
    concatn(length,desc,"\n");

    snprintf(temp,TEMPSIZE, "layout: ");
    concatn(length,desc,temp);
    get_vgmstream_layout_description(vgmstream, temp, TEMPSIZE);
    concatn(length, desc, temp);
    concatn(length,desc,"\n");

    if (vgmstream->layout_type == layout_interleave && vgmstream->channels > 1) {
        snprintf(temp,TEMPSIZE, "interleave: %#x bytes\n", (int32_t)vgmstream->interleave_block_size);
        concatn(length,desc,temp);

        if (vgmstream->interleave_first_block_size && vgmstream->interleave_first_block_size != vgmstream->interleave_block_size) {
            snprintf(temp,TEMPSIZE, "interleave first block: %#x bytes\n", (int32_t)vgmstream->interleave_first_block_size);
            concatn(length,desc,temp);
        }

        if (vgmstream->interleave_last_block_size && vgmstream->interleave_last_block_size != vgmstream->interleave_block_size) {
            snprintf(temp,TEMPSIZE, "interleave last block: %#x bytes\n", (int32_t)vgmstream->interleave_last_block_size);
            concatn(length,desc,temp);
        }
    }

    /* codecs with configurable frame size */
    if (vgmstream->frame_size > 0 || vgmstream->interleave_block_size > 0) {
        int32_t frame_size = vgmstream->frame_size > 0 ? vgmstream->frame_size : vgmstream->interleave_block_size;
        switch (vgmstream->coding_type) {
            case coding_MSADPCM:
            case coding_MSADPCM_mono:
            case coding_MSADPCM_ck:
            case coding_MS_IMA:
            case coding_MS_IMA_mono:
            case coding_MPC3:
            case coding_WWISE_IMA:
            case coding_REF_IMA:
            case coding_PSX_cfg:
                snprintf(temp,TEMPSIZE, "frame size: %#x bytes\n", frame_size);
                concatn(length,desc,temp);
                break;
            default:
                break;
        }
    }

    snprintf(temp,TEMPSIZE, "metadata from: ");
    concatn(length,desc,temp);
    get_vgmstream_meta_description(vgmstream, temp, TEMPSIZE);
    concatn(length,desc,temp);
    concatn(length,desc,"\n");

    snprintf(temp,TEMPSIZE, "bitrate: %d kbps\n", get_vgmstream_average_bitrate(vgmstream) / 1000);
    concatn(length,desc,temp);

    /* only interesting if more than one */
    if (vgmstream->num_streams > 1) {
        snprintf(temp,TEMPSIZE, "stream count: %d\n", vgmstream->num_streams);
        concatn(length,desc,temp);
    }

    if (vgmstream->num_streams > 1) {
        snprintf(temp,TEMPSIZE, "stream index: %d\n", vgmstream->stream_index == 0 ? 1 : vgmstream->stream_index);
        concatn(length,desc,temp);
    }

    if (vgmstream->stream_name[0] != '\0') {
        snprintf(temp,TEMPSIZE, "stream name: %s\n", vgmstream->stream_name);
        concatn(length,desc,temp);
    }

    sfmt_t sfmt = mixing_get_input_sample_type(vgmstream);
    if (sfmt != SFMT_S16) {
        const char* sfmt_desc;
        switch(sfmt) {
            case SFMT_FLT: sfmt_desc = "float"; break;
            case SFMT_F16: sfmt_desc = "float16"; break;
            case SFMT_S16: sfmt_desc = "pcm16"; break;
            case SFMT_S24: sfmt_desc = "pcm24"; break;
            case SFMT_S32: sfmt_desc = "pcm32"; break;
            case SFMT_O24: sfmt_desc = "pcm24"; break;
            default: sfmt_desc = "???";
        }

        snprintf(temp,TEMPSIZE, "sample type: %s\n", sfmt_desc);
        concatn(length,desc,temp);
    }


    if (vgmstream->config_enabled) {
        int32_t samples = vgmstream->pstate.play_duration;

        describe_get_time(samples, vgmstream->sample_rate, &time_mm, &time_ss);
        snprintf(temp,TEMPSIZE, "play duration: %d samples (%1.0f:%06.3f seconds)\n", samples, time_mm, time_ss);
        concatn(length,desc,temp);
    }

}


/*******************************************************************************/
/* BITRATE                                                                     */
/*******************************************************************************/

#define BITRATE_FILES_MAX 128 /* arbitrary max, but +100 segments have been observed */
typedef struct {
    uint32_t hash[BITRATE_FILES_MAX]; /* already used streamfiles */
    int subsong[BITRATE_FILES_MAX]; /* subsongs of those streamfiles (could be incorporated to the hash?) */
    int count;
    int count_max;
} bitrate_info_t;

static uint32_t hash_sf(STREAMFILE* sf) {
    int i;
    char path[PATH_LIMIT];
    uint32_t hash = 2166136261;

    get_streamfile_name(sf, path, sizeof(path));

    /* our favorite garbo hash a.k.a FNV-1 32b */
    i = 0;
    while (path[i] != '\0') {
        char c = tolower(path[i]);
        hash = (hash * 16777619) ^ (uint8_t)c;
        i++;
    }

    return hash;
}

/* average bitrate helper to get STREAMFILE for a channel, since some codecs may use their own */
static STREAMFILE* get_vgmstream_average_bitrate_channel_streamfile(VGMSTREAM* vgmstream, int channel) {

    if (vgmstream->coding_type == coding_NWA) {
        return nwa_get_streamfile(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_ACM) {
        return acm_get_streamfile(vgmstream->codec_data);
    }

    if (vgmstream->coding_type == coding_COMPRESSWAVE) {
        return compresswave_get_streamfile(vgmstream);
    }

#ifdef VGM_USE_VORBIS
    if (vgmstream->coding_type == coding_OGG_VORBIS) {
        return ogg_vorbis_get_streamfile(vgmstream->codec_data);
    }
#endif
    if (vgmstream->coding_type == coding_CRI_HCA) {
        return hca_get_streamfile(vgmstream->codec_data);
    }
#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type == coding_FFmpeg) {
        return ffmpeg_get_streamfile(vgmstream->codec_data);
    }
#endif
#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)
    if (vgmstream->coding_type == coding_MP4_AAC) {
        return mp4_aac_get_streamfile(vgmstream->codec_data);
    }
#endif

    return vgmstream->ch[channel].streamfile;
}

static int get_vgmstream_file_bitrate_from_size(size_t size, int sample_rate, int32_t length_samples) {
    if (sample_rate == 0 || length_samples == 0) return 0;
    if (length_samples < 100) return 0; /* ignore stupid bitrates caused by some segments */
    return (int)((int64_t)size * 8 * sample_rate / length_samples);
}
static int get_vgmstream_file_bitrate_from_streamfile(STREAMFILE* sf, int sample_rate, int32_t length_samples) {
    if (sf == NULL) return 0;
    return get_vgmstream_file_bitrate_from_size(get_streamfile_size(sf), sample_rate, length_samples);
}

static int get_vgmstream_file_bitrate_main(VGMSTREAM* vgmstream, bitrate_info_t* br, int* p_uniques) {
    int i, ch;
    int bitrate = 0;

    /* Recursively get bitrate and fill the list of streamfiles if needed (to filter),
     * since layouts can include further vgmstreams that may also share streamfiles.
     *
     * Because of how data, layers and segments can be combined it's possible to
     * fool this in various ways; metas should report stream_size in complex cases
     * to get accurate bitrates (particularly for subsongs). An edge case is when
     * segments use only a few samples from a full file (like Wwise transitions), bitrates
     * become a bit high since its hard to detect only part of the file is needed. */

    if (vgmstream->stream_size != 0) {
        /* format may report full size for custom layouts that otherwise get odd values */
        bitrate += get_vgmstream_file_bitrate_from_size(vgmstream->stream_size, vgmstream->sample_rate, vgmstream->num_samples);
        if (p_uniques)
            (*p_uniques)++;
    }
    else if (vgmstream->layout_type == layout_segmented) {
        int uniques = 0;
        segmented_layout_data *data = (segmented_layout_data *) vgmstream->layout_data;
        for (i = 0; i < data->segment_count; i++) {
            bitrate += get_vgmstream_file_bitrate_main(data->segments[i], br, &uniques);
        }
        if (uniques)
            bitrate /= uniques; /* average */
    }
    else if (vgmstream->layout_type == layout_layered) {
        layered_layout_data *data = vgmstream->layout_data;
        for (i = 0; i < data->layer_count; i++) {
            bitrate += get_vgmstream_file_bitrate_main(data->layers[i], br, NULL);
        }
    }
    else {
        /* Add channel bitrate if streamfile hasn't been used before, so bitrate doesn't count repeats
         * (like same STREAMFILE reopened per channel, also considering SFs may be wrapped). */
        for (ch = 0; ch < vgmstream->channels; ch++) {
            uint32_t hash_cur;
            int subsong_cur;
            STREAMFILE* sf_cur;
            int is_unique = 1; /* default to "no other SFs exist" */

            /* compares paths (hashes for faster compares) + subsongs (same file + different subsong = "different" file) */
            sf_cur = get_vgmstream_average_bitrate_channel_streamfile(vgmstream, ch);
            if (!sf_cur) continue;

            hash_cur = hash_sf(sf_cur);
            subsong_cur = vgmstream->stream_index;

            for (i = 0; i < br->count; i++) {
                uint32_t hash_cmp = br->hash[i];
                int subsong_cmp = br->subsong[i];

                if (hash_cur == hash_cmp && subsong_cur == subsong_cmp) {
                    is_unique = 0;
                    break;
                }
            }

            if (is_unique) {
                size_t file_bitrate;

                if (br->count >= br->count_max) goto fail;
                
                if (vgmstream->stream_size) {
                    /* stream_size applies to both channels but should add once and detect repeats (for current subsong) */
                    file_bitrate = get_vgmstream_file_bitrate_from_size(vgmstream->stream_size, vgmstream->sample_rate, vgmstream->num_samples);
                }
                else {
                    file_bitrate = get_vgmstream_file_bitrate_from_streamfile(sf_cur, vgmstream->sample_rate, vgmstream->num_samples);
                }

                /* possible in cases like using silence codec */
                if (!file_bitrate)
                    break;

                br->hash[br->count] = hash_cur;
                br->subsong[br->count] = subsong_cur;

                br->count++;
                if (p_uniques)
                    (*p_uniques)++;

                bitrate += file_bitrate;

                break;
            }
        }
    }

    return bitrate;
fail:
    return 0;
}

/* Return the average bitrate in bps of all unique data contained within this stream.
 * This is the bitrate of the *file*, as opposed to the bitrate of the *codec*, meaning
 * it counts extra data like block headers and padding. While this can be surprising
 * sometimes (as it's often higher than common codec bitrates) it isn't wrong per se. */
int get_vgmstream_average_bitrate(VGMSTREAM* vgmstream) {
    bitrate_info_t br = {0};
    br.count_max = BITRATE_FILES_MAX;

    if (vgmstream->coding_type == coding_SILENCE)
        return 0;

    return get_vgmstream_file_bitrate_main(vgmstream, &br, NULL);
}
