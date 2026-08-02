#include <ctype.h>
#include "info.h"
#include "mixing.h"
#include "play_state.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/channel_mappings.h"
#include "../util/sf_utils.h"
#include "../util/string_utils.h"

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
void describe_vgmstream(VGMSTREAM* vgmstream, char* dst, size_t dst_size) {
    char tmp[TEMPSIZE];
    size_t tmp_size = sizeof(tmp);
    double time_mm, time_ss;

    dst[0] = '\0';

    if (!vgmstream) {
        strcat_v(dst, dst_size, "NULL VGMSTREAM");
        return;
    }

    int input_sample_rate = vgmstream->sample_rate;
    int output_sample_rate = mixing_get_output_sample_rate(vgmstream);
    if (output_sample_rate == 0)
        output_sample_rate = input_sample_rate;

    //TODO: improve to avoid strcat recalculating dst_len

    snprintf(tmp, tmp_size, "sample rate: %d Hz\n", output_sample_rate);
    strcat_v(dst, dst_size, tmp);

    snprintf(tmp, tmp_size, "channels: %d\n", vgmstream->channels);
    strcat_v(dst, dst_size, tmp);

    {
        int output_channels = 0;
        mixing_info(vgmstream, NULL, &output_channels);

        if (output_channels != vgmstream->channels) {
            snprintf(tmp, tmp_size, "input channels: %d\n", vgmstream->channels); /* repeated but mainly for plugins */
            strcat_v(dst, dst_size, tmp);
            snprintf(tmp, tmp_size, "output channels: %d\n", output_channels);
            strcat_v(dst, dst_size, tmp);
        }
    }

    if (vgmstream->channel_layout) {
        uint32_t cl = vgmstream->channel_layout;

        /* not "channel layout: " to avoid mixups with "layout: " */
        snprintf(tmp, tmp_size, "channel mask: 0x%x /", vgmstream->channel_layout);
        strcat_v(dst, dst_size, tmp);
        if (cl & speaker_FL)    strcat_v(dst, dst_size," FL");
        if (cl & speaker_FR)    strcat_v(dst, dst_size," FR");
        if (cl & speaker_FC)    strcat_v(dst, dst_size," FC");
        if (cl & speaker_LFE)   strcat_v(dst, dst_size," LFE");
        if (cl & speaker_BL)    strcat_v(dst, dst_size," BL");
        if (cl & speaker_BR)    strcat_v(dst, dst_size," BR");
        if (cl & speaker_FLC)   strcat_v(dst, dst_size," FLC"); //FCL is also common
        if (cl & speaker_FRC)   strcat_v(dst, dst_size," FRC"); //FCR is also common
        if (cl & speaker_BC)    strcat_v(dst, dst_size," BC");
        if (cl & speaker_SL)    strcat_v(dst, dst_size," SL");
        if (cl & speaker_SR)    strcat_v(dst, dst_size," SR");
        if (cl & speaker_TC)    strcat_v(dst, dst_size," TC");
        if (cl & speaker_TFL)   strcat_v(dst, dst_size," TFL");
        if (cl & speaker_TFC)   strcat_v(dst, dst_size," TFC");
        if (cl & speaker_TFR)   strcat_v(dst, dst_size," TFR");
        if (cl & speaker_TBL)   strcat_v(dst, dst_size," TBL");
        if (cl & speaker_TBC)   strcat_v(dst, dst_size," TBC");
        if (cl & speaker_TBR)   strcat_v(dst, dst_size," TBR");
        strcat_v(dst, dst_size,"\n");
    }

    /* times mod sounds avoid round up to 60.0 */
    if (vgmstream->loop_start_sample >= 0 && vgmstream->loop_end_sample > vgmstream->loop_start_sample) {
        if (!vgmstream->loop_flag) {
            strcat_v(dst, dst_size,"looping: disabled\n");
        }

        describe_get_time(vgmstream->loop_start_sample, output_sample_rate, &time_mm, &time_ss);
        snprintf(tmp, tmp_size, "loop start: %d samples (%1.0f:%06.3f seconds)\n", vgmstream->loop_start_sample, time_mm, time_ss);
        strcat_v(dst, dst_size, tmp);

        describe_get_time(vgmstream->loop_end_sample, output_sample_rate, &time_mm, &time_ss);
        snprintf(tmp, tmp_size, "loop end: %d samples (%1.0f:%06.3f seconds)\n", vgmstream->loop_end_sample, time_mm, time_ss);
        strcat_v(dst, dst_size, tmp);
    }

    describe_get_time(vgmstream->num_samples, input_sample_rate, &time_mm, &time_ss);
    snprintf(tmp, tmp_size, "stream total samples: %d (%1.0f:%06.3f seconds)\n", vgmstream->num_samples, time_mm, time_ss);
    strcat_v(dst, dst_size, tmp);

    strcat_v(dst, dst_size, "encoding: ");
    get_vgmstream_coding_description(vgmstream, tmp, tmp_size);
    strcat_v(dst, dst_size, tmp);
    strcat_v(dst, dst_size, "\n");

    strcat_v(dst, dst_size, "layout: ");
    get_vgmstream_layout_description(vgmstream, tmp, tmp_size);
    strcat_v(dst, dst_size, tmp);
    strcat_v(dst, dst_size,"\n");

    if (vgmstream->layout_type == layout_interleave && vgmstream->channels > 1) {
        snprintf(tmp, tmp_size, "interleave: %#x bytes\n", (int32_t)vgmstream->interleave_block_size);
        strcat_v(dst, dst_size, tmp);

        if (vgmstream->interleave_first_block_size && vgmstream->interleave_first_block_size != vgmstream->interleave_block_size) {
            snprintf(tmp, tmp_size, "interleave first block: %#x bytes\n", (int32_t)vgmstream->interleave_first_block_size);
            strcat_v(dst, dst_size, tmp);
        }

        if (vgmstream->interleave_last_block_size && vgmstream->interleave_last_block_size != vgmstream->interleave_block_size) {
            snprintf(tmp, tmp_size, "interleave last block: %#x bytes\n", (int32_t)vgmstream->interleave_last_block_size);
            strcat_v(dst, dst_size, tmp);
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
                snprintf(tmp, tmp_size, "frame size: %#x bytes\n", frame_size);
                strcat_v(dst, dst_size, tmp);
                break;
            default:
                break;
        }
    }

    strcat_v(dst, dst_size, "metadata from: ");
    get_vgmstream_meta_description(vgmstream, tmp, tmp_size);
    strcat_v(dst, dst_size, tmp);
    strcat_v(dst, dst_size,"\n");

    snprintf(tmp, tmp_size, "bitrate: %d kbps\n", get_vgmstream_average_bitrate(vgmstream) / 1000);
    strcat_v(dst, dst_size, tmp);

    /* only interesting if more than one */
    if (vgmstream->num_streams > 1) {
        snprintf(tmp, tmp_size, "stream count: %d\n", vgmstream->num_streams);
        strcat_v(dst, dst_size, tmp);
    }

    if (vgmstream->num_streams > 1) {
        snprintf(tmp, tmp_size, "stream index: %d\n", vgmstream->stream_index == 0 ? 1 : vgmstream->stream_index);
        strcat_v(dst, dst_size, tmp);
    }

    if (vgmstream->stream_name[0] != '\0') {
        snprintf(tmp, tmp_size, "stream name: %s\n", vgmstream->stream_name);
        strcat_v(dst, dst_size, tmp);
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

        snprintf(tmp, tmp_size, "sample type: %s\n", sfmt_desc);
        strcat_v(dst, dst_size, tmp);
    }


    if (vgmstream->config_enabled) {
        int32_t samples = vgmstream_get_samples(vgmstream);

        describe_get_time(samples, output_sample_rate, &time_mm, &time_ss);
        snprintf(tmp, tmp_size, "play duration: %d samples (%1.0f:%06.3f seconds)\n", samples, time_mm, time_ss);
        strcat_v(dst, dst_size, tmp);
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
    char path[PATH_LIMIT];

    get_streamfile_name(sf, path, sizeof(path));

    /* our favorite garbo hash a.k.a FNV-1 32b */
    uint32_t hash = 2166136261;
    int i = 0;
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
        for (int i = 0; i < data->segment_count; i++) {
            bitrate += get_vgmstream_file_bitrate_main(data->segments[i], br, &uniques);
        }
        if (uniques)
            bitrate /= uniques; /* average */
    }
    else if (vgmstream->layout_type == layout_layered) {
        layered_layout_data *data = vgmstream->layout_data;
        for (int i = 0; i < data->layer_count; i++) {
            bitrate += get_vgmstream_file_bitrate_main(data->layers[i], br, NULL);
        }
    }
    else {
        /* Add channel bitrate if streamfile hasn't been used before, so bitrate doesn't count repeats
         * (like same STREAMFILE reopened per channel, also considering SFs may be wrapped). */
        for (int ch = 0; ch < vgmstream->channels; ch++) {
            uint32_t hash_cur;
            int subsong_cur;
            STREAMFILE* sf_cur;
            int is_unique = 1; /* default to "no other SFs exist" */

            /* compares paths (hashes for faster compares) + subsongs (same file + different subsong = "different" file) */
            sf_cur = get_vgmstream_average_bitrate_channel_streamfile(vgmstream, ch);
            if (!sf_cur) continue;

            hash_cur = hash_sf(sf_cur);
            subsong_cur = vgmstream->stream_index;

            for (int i = 0; i < br->count; i++) {
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
