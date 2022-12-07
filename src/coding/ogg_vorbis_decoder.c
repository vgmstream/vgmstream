#include <math.h>
#include "coding.h"
#include "../util.h"

#ifdef VGM_USE_VORBIS
#include <vorbis/vorbisfile.h>

#define OGG_DEFAULT_BITSTREAM 0

/* opaque struct */
struct ogg_vorbis_codec_data {
    OggVorbis_File ogg_vorbis_file;
    int ovf_init;
    int bitstream;
    int disable_reordering; /* Xiph Ogg must reorder channels on output, but some pre-ordered games don't need it */
    int force_seek; /* Ogg with wrong granules can't seek correctly */

    int32_t discard;

    ogg_vorbis_io io;
    vorbis_comment* comment;
    int comment_number;
    vorbis_info* info;
};


static void pcm_convert_float_to_16(int channels, sample_t* outbuf, int start_sample, int samples_to_do, float** pcm, int disable_ordering);

static size_t ov_read_func(void* ptr, size_t size, size_t nmemb, void* datasource);
static int ov_seek_func(void* datasource, ogg_int64_t offset, int whence);
static long ov_tell_func(void* datasource);
static int ov_close_func(void* datasource);


ogg_vorbis_codec_data* init_ogg_vorbis(STREAMFILE* sf, off_t start, off_t size, ogg_vorbis_io* io) {
    ogg_vorbis_codec_data* data = NULL;
    ov_callbacks callbacks = {0};

    //todo clean up

    callbacks.read_func = ov_read_func;
    callbacks.seek_func = ov_seek_func;
    callbacks.close_func = ov_close_func;
    callbacks.tell_func = ov_tell_func;

    if (!size)
        size = get_streamfile_size(sf) - start;

    /* test if this is a proper Ogg Vorbis file, with the current (from init_x) STREAMFILE
     * (quick test without having to malloc first, though if one invoked this it'll probably success) */
    {
        OggVorbis_File temp_ovf = {0};
        ogg_vorbis_io temp_io = {0};

        temp_io.streamfile = sf;

        temp_io.start = start;
        temp_io.offset = 0;
        temp_io.size = size;

        if (io != NULL) {
            temp_io.decryption_callback = io->decryption_callback;
            temp_io.scd_xor = io->scd_xor;
            temp_io.scd_xor_length = io->scd_xor_length;
            temp_io.xor_value = io->xor_value;
        }

        /* open the ogg vorbis file for testing */
        if (ov_test_callbacks(&temp_io, &temp_ovf, NULL, 0, callbacks))
            goto fail;

        /* we have to close this as it has the init_vgmstream meta-reading STREAMFILE */
        ov_clear(&temp_ovf);
    }

    /* proceed to init codec_data and reopen a STREAMFILE for this codec */
    {
        data = calloc(1,sizeof(ogg_vorbis_codec_data));
        if (!data) goto fail;

        data->io.streamfile = reopen_streamfile(sf, 0);
        if (!data->io.streamfile) goto fail;

        data->io.start = start;
        data->io.offset = 0;
        data->io.size = size;

        if (io != NULL) {
            data->io.decryption_callback = io->decryption_callback;
            data->io.scd_xor = io->scd_xor;
            data->io.scd_xor_length = io->scd_xor_length;
            data->io.xor_value = io->xor_value;
        }

        /* open the ogg vorbis file for real */
        if (ov_open_callbacks(&data->io, &data->ogg_vorbis_file, NULL, 0, callbacks))
            goto fail;
        data->ovf_init = 1;
    }

    //todo could set bitstreams as subsongs?
    /* get info from bitstream */
    data->bitstream = OGG_DEFAULT_BITSTREAM;

    data->comment = ov_comment(&data->ogg_vorbis_file, data->bitstream);
    data->info = ov_info(&data->ogg_vorbis_file, data->bitstream);

    return data;
fail:
    free_ogg_vorbis(data);
    return NULL;
}

static size_t ov_read_func(void* ptr, size_t size, size_t nmemb, void* datasource) {
    ogg_vorbis_io *io = datasource;
    size_t bytes_read, items_read;

    off_t real_offset = io->start + io->offset;
    size_t max_bytes = size * nmemb;

    /* clamp for virtual filesize */
    if (max_bytes > io->size - io->offset)
        max_bytes = io->size - io->offset;

    bytes_read = read_streamfile(ptr, real_offset, max_bytes, io->streamfile);
    items_read = bytes_read / size;

    /* may be encrypted */
    if (io->decryption_callback) {
        io->decryption_callback(ptr, size, items_read, io);
    }

    io->offset += items_read * size;

    return items_read;
}

static int ov_seek_func(void* datasource, ogg_int64_t offset, int whence) {
    ogg_vorbis_io* io = datasource;
    ogg_int64_t base_offset, new_offset;

    switch (whence) {
        case SEEK_SET:
            base_offset = 0;
            break;
        case SEEK_CUR:
            base_offset = io->offset;
            break;
        case SEEK_END:
            base_offset = io->size;
            break;
        default:
            return -1;
            break;
    }

    new_offset = base_offset + offset;
    if (new_offset < 0 || new_offset > io->size) {
        return -1; /* *must* return -1 if stream is unseekable */
    } else {
        io->offset = new_offset;
        return 0;
    }
}

static long ov_tell_func(void* datasource) {
    ogg_vorbis_io* io = datasource;
    return io->offset;
}

static int ov_close_func(void* datasource) {
    /* needed as setting ov_close_func in ov_callbacks to NULL doesn't seem to work
     * (closing the streamfile is done in free_ogg_vorbis) */
    return 0;
}

/* ********************************************** */

void decode_ogg_vorbis(ogg_vorbis_codec_data* data, sample_t* outbuf, int32_t samples_to_do, int channels) {
    int samples_done = 0;
    long start, rc;
    float** pcm_channels; /* pointer to Xiph's double array buffer */

    while (samples_done < samples_to_do) {
        rc = ov_read_float(
                &data->ogg_vorbis_file,             /* context */
                &pcm_channels,                      /* buffer pointer */
                (samples_to_do - samples_done),     /* samples to produce */
                &data->bitstream);                  /* bitstream */
        if (rc <= 0) goto fail; /* rc is samples done */

        if (data->discard) {
            start = data->discard;
            if (start > rc)
                start = rc;

            data->discard -= start;
            if (start == rc) /* consume all */
                continue;
        }
        else {
            start = 0;
        }

        pcm_convert_float_to_16(channels, outbuf, start, rc, pcm_channels, data->disable_reordering);

        outbuf += (rc - start) * channels;
        samples_done += (rc - start);


#if 0   // alt decoding
        /* we use ov_read_float as to reuse the xiph's buffer for easier remapping,
         * but seems ov_read is slightly faster due to optimized (asm) float-to-int. */
        rc = ov_read(
                &data->ogg_vorbis_file,             /* context */
                (char *)(outbuf),                   /* buffer */
                (samples_to_do - samples_done) * sizeof(sample_t) * channels, /* length in bytes */
                0,                                  /* pcm endianness */
                sizeof(sample),                     /* pcm size */
                1,                                  /* pcm signedness */
                &data->bitstream);                  /* bitstream */
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
 * (feels a bit weird as one would think you could leave as-is and set the player's output order,
 * but that isn't possible and remapping like this is what FFmpeg and every other plugin does). */
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
static void pcm_convert_float_to_16(int channels, sample_t* outbuf, int start_sample, int samples_to_do, float** pcm, int disable_ordering) {
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
        for (s = start_sample; s < samples_to_do; s++) {
            int val = (int)floor(channel[s] * 32767.0f + 0.5f); /* use floorf? doesn't seem any faster */
            if (val > 32767) val = 32767;
            else if (val < -32768) val = -32768;

            *ptr = val;
            ptr += channels;
        }
    }
}

/* ********************************************** */

void reset_ogg_vorbis(ogg_vorbis_codec_data* data) {
    if (!data) return;

    /* this raw seek cleans internal buffers, and it's preferable to
     * ov_pcm_seek as doesn't get confused by wrong granules */
    ov_raw_seek(&data->ogg_vorbis_file, 0);
    //;VGM_ASSERT(res != 0, "OGG: bad reset=%i\n", res);

    data->discard = 0;
}

void seek_ogg_vorbis(ogg_vorbis_codec_data* data, int32_t num_sample) {
    if (!data) return;

    /* special seek for games with bad granule positions (since ov_*_seek uses granules to seek) */
    if (data->force_seek) {
        reset_ogg_vorbis(data);
        data->discard = num_sample;
        return;
    }

    /* this seek crosslaps to avoid possible clicks, so seeking to 0 + discarding
     * will decode a bit differently than ov_pcm_seek */
    ov_pcm_seek_lap(&data->ogg_vorbis_file, num_sample);
    //VGM_ASSERT(res != 0, "OGG: bad seek=%i\n", res); /* not seen, in theory could give error */
}

void free_ogg_vorbis(ogg_vorbis_codec_data* data) {
    if (!data) return;

    if (data->ovf_init)
        ov_clear(&data->ogg_vorbis_file);

    close_streamfile(data->io.streamfile);
    free(data);
}

/* ********************************************** */

int ogg_vorbis_get_comment(ogg_vorbis_codec_data* data, const char** comment) {
    if (!data) return 0;

    /* dumb reset */
    if (comment == NULL) {
        data->comment_number = 0;
        return 1;
    }

    if (data->comment_number >= data->comment->comments)
        return 0;

    *comment = data->comment->user_comments[data->comment_number];
    data->comment_number++;
    return 1;
}

void ogg_vorbis_get_info(ogg_vorbis_codec_data* data, int* p_channels, int* p_sample_rate) {
    if (!data) {
        if (p_channels) *p_channels = 0;
        if (p_sample_rate) *p_sample_rate = 0;
        return;
    }

    if (p_channels) *p_channels = data->info->channels;
    if (p_sample_rate) *p_sample_rate = (int)data->info->rate;
}

void ogg_vorbis_get_samples(ogg_vorbis_codec_data* data, int* p_samples) {
    if (!data) {
        if (p_samples) *p_samples = 0;
        return;
    }

    if (p_samples) *p_samples = ov_pcm_total(&data->ogg_vorbis_file,-1);
}

void ogg_vorbis_set_disable_reordering(ogg_vorbis_codec_data* data, int set) {
    if (!data) return;

    data->disable_reordering = set;
}

void ogg_vorbis_set_force_seek(ogg_vorbis_codec_data* data, int set) {
    if (!data) return;

    data->force_seek = set;
}

STREAMFILE* ogg_vorbis_get_streamfile(ogg_vorbis_codec_data* data) {
    if (!data) return NULL;
    return data->io.streamfile;
}

#endif
