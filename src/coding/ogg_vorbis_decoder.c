#include <math.h>
#include "coding.h"
#include "../util.h"

#ifdef VGM_USE_VORBIS
#include <vorbis/vorbisfile.h>


static void pcm_convert_float_to_16(int channels, sample_t * outbuf, int samples_to_do, float ** pcm, int disable_ordering);


void decode_ogg_vorbis(ogg_vorbis_codec_data * data, sample_t * outbuf, int32_t samples_to_do, int channels) {
    int samples_done = 0;
    long rc;
    float **pcm_channels; /* pointer to Xiph's double array buffer */

    while (samples_done < samples_to_do) {
        rc = ov_read_float(
                &data->ogg_vorbis_file, /* context */
                &pcm_channels, /* buffer pointer */
                (samples_to_do - samples_done), /* samples to produce */
                &data->bitstream); /* bitstream*/
        if (rc <= 0) goto fail; /* rc is samples done */

        pcm_convert_float_to_16(channels, outbuf, rc, pcm_channels, data->disable_reordering);

        outbuf += rc * channels;
        samples_done += rc;


#if 0   // alt decoding
        /* we use ov_read_float as to reuse the xiph's buffer for easier remapping,
         * but seems ov_read is slightly faster due to optimized (asm) float-to-int. */
        rc = ov_read(
                &data->ogg_vorbis_file, /* context */
                (char *)(outbuf), /* buffer */
                (samples_to_do - samples_done) * sizeof(sample_t) * channels, /* length in bytes */
                0, /* pcm endianness */
                sizeof(sample), /* pcm size */
                1, /* pcm signedness */
                &data->bitstream); /* bitstream */
        if (rc <= 0) goto fail; /* rc is bytes done (for all channels) */

        swap_samples_le(outbuf, rc / sizeof(sample_t)); /* endianness is a bit weird with ov_read though */

        outbuf += rc / sizeof(sample_t);
        samples_done += rc / sizeof(sample_t) / channels;
#endif
    }

    return;
fail:
    VGM_LOG("OGG: error %lx during decode\n", rc);
    memset(outbuf, 0, (samples_to_do - samples_done) * channels * sizeof(sample));
}

/* vorbis encodes channels in non-standard order, so we remap during conversion to fix this oddity.
 * (feels a bit weird as one would think you could leave as-is and set the player's output
 * order, but that isn't possible and remapping is what FFmpeg and every other plugin do). */
static const int xiph_channel_map[8][8] = {
    { 0 },                          /* 1ch: FC > same */
    { 0, 1 },                       /* 2ch: FL FR > same */
    { 0, 2, 1 },                    /* 3ch: FL FC FR > FL FR FC */
    { 0, 1, 2, 3 },                 /* 4ch: FL FR BL BR > same */
    { 0, 2, 1, 3, 4 },              /* 5ch: FL FC FR BL BR > FL FR FC BL BR */
    { 0, 2, 1, 5, 3, 4 },           /* 6ch: FL FC FR BL BR LFE > FL FR FC LFE BL BR */
    { 0, 2, 1, 6, 5, 3, 4 },        /* 7ch: FL FC FR SL SR BC LFE > FL FR FC LFE BC SL SR */
    { 0, 2, 1, 7, 5, 6, 3, 4 },     /* 8ch: FL FC FR SL SR BL BR LFE > FL FR FC LFE BL BR SL SR */
};

/* converts from internal Vorbis format to standard PCM and remaps (mostly from Xiph's decoder_example.c) */
static void pcm_convert_float_to_16(int channels, sample_t * outbuf, int samples_to_do, float ** pcm, int disable_ordering) {
    int ch, s, ch_map;
    sample_t *ptr;
    float *channel;

    /* convert float PCM (multichannel float array, with pcm[0]=ch0, pcm[1]=ch1, pcm[2]=ch0, etc)
     * to 16 bit signed PCM ints (host order) and interleave + fix clipping */
    for (ch = 0; ch < channels; ch++) {
        ch_map = disable_ordering ?
                ch :
                (channels > 8) ? ch : xiph_channel_map[channels - 1][ch]; /* put Vorbis' ch to other outbuf's ch */
        ptr = outbuf + ch;
        channel = pcm[ch_map];
        for (s = 0; s < samples_to_do; s++) {
            int val = (int)floor(channel[s] * 32767.0f + 0.5f); /* use floorf? doesn't seem any faster */
            if (val > 32767) val = 32767;
            else if (val < -32768) val = -32768;

            *ptr = val;
            ptr += channels;
        }
    }
}

/* ********************************************** */

void reset_ogg_vorbis(VGMSTREAM *vgmstream) {
    ogg_vorbis_codec_data *data = vgmstream->codec_data;
    if (!data) return;

    /* this seek cleans internal buffers */
    ov_pcm_seek(&data->ogg_vorbis_file, 0);
}

void seek_ogg_vorbis(VGMSTREAM *vgmstream, int32_t num_sample) {
    ogg_vorbis_codec_data *data = vgmstream->codec_data;
    if (!data) return;

    /* this seek crosslaps to avoid possible clicks, so seeking to 0 will
     * decode a bit differently than ov_pcm_seek */
    ov_pcm_seek_lap(&data->ogg_vorbis_file, num_sample);
}

void free_ogg_vorbis(ogg_vorbis_codec_data *data) {
    if (!data) return;

    ov_clear(&data->ogg_vorbis_file);

    close_streamfile(data->ov_streamfile.streamfile);
    free(data);
}

#endif
