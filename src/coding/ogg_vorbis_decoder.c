#include <math.h>
#include "coding.h"
#include "../base/decode_state.h"
#include "../base/sbuf.h"
#include "../base/codec_info.h"
#include "../util.h"

#ifdef VGM_USE_VORBIS
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

#define VORBIS_CALL_SAMPLES 1024  // allowed frame 'blocksizes' range from 2^6 ~ 2^13 (64 ~ 8192) but we can return partial samples
#define OGG_DEFAULT_BITSTREAM 0

/* opaque struct */
struct ogg_vorbis_codec_data {
    OggVorbis_File ogg_vorbis_file;
    int bitstream;              // special flag for current stream (in practice can be ignored)

    bool ovf_init;
    bool disable_reordering;    /* Xiph Ogg must reorder channels on output, but some pre-ordered games don't need it */
    bool force_seek;            /* Ogg with wrong granules can't seek correctly */
    int32_t discard;

    ogg_vorbis_io io;
    vorbis_comment* comment;
    int comment_number;
    vorbis_info* info;

    float* fbuf;
};


static size_t ov_read_func(void* ptr, size_t size, size_t nmemb, void* datasource);
static int ov_seek_func(void* datasource, ogg_int64_t offset, int whence);
static long ov_tell_func(void* datasource);
static int ov_close_func(void* datasource);

static void free_ogg_vorbis(void* priv_data) {
    ogg_vorbis_codec_data* data = priv_data;
    if (!data) return;

    if (data->ovf_init) {
        ov_clear(&data->ogg_vorbis_file);
    }

    close_streamfile(data->io.streamfile);
    free(data->fbuf);
    free(data);
}

ogg_vorbis_codec_data* init_ogg_vorbis(STREAMFILE* sf, off_t start, off_t size, ogg_vorbis_io* io) {
    ogg_vorbis_codec_data* data = NULL;
    ov_callbacks callbacks = {0};

    //todo clean up
    if (!sf)
        return NULL;

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
        data->ovf_init = true;
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

static bool decode_frame_ogg_vorbis(VGMSTREAM* v) {
    ogg_vorbis_codec_data* data = v->codec_data;
    decode_state_t* ds = v->decode_state;
    float** pcm_channels;

    //TODO: helper? maybe should init in init_vorbis_custom but right now not all vorbises pass channels
    if (data->fbuf == NULL) {
        data->fbuf = malloc(VORBIS_CALL_SAMPLES * sizeof(float) * v->channels);
        if (!data->fbuf) return -1;
    }

    // Ogg frame samples vary per frame, and API allows to ask for arbitrary max (may return less).
    // Limit totals as loop end needs to stop at exact point, since seeking is smoothed between current + loop start
    // (decoding a bit more than loop end results in slightly different loops, very minor but done to match older code).
    int max_samples = ds->samples_left;
    if (max_samples > VORBIS_CALL_SAMPLES)
        max_samples = VORBIS_CALL_SAMPLES;

    long rc = ov_read_float(&data->ogg_vorbis_file, &pcm_channels, max_samples, &data->bitstream);
    if (rc <= 0)  // rc is samples done
        return false;

    sbuf_init_flt(&ds->sbuf, data->fbuf, rc, v->channels);
    ds->sbuf.filled = rc;

    if (data->disable_reordering)
        sbuf_interleave(&ds->sbuf, pcm_channels);
    else
        sbuf_interleave_vorbis(&ds->sbuf, pcm_channels);

    if (data->discard) {
        ds->discard = data->discard;
        data->discard = 0;
    }

    return true;
}

/* ********************************************** */

static void reset_ogg_vorbis(void* priv_data) {
    ogg_vorbis_codec_data* data = priv_data;
    if (!data) return;

    /* this raw seek cleans internal buffers, and it's preferable to
     * ov_pcm_seek as doesn't get confused by wrong granules */
    ov_raw_seek(&data->ogg_vorbis_file, 0);
    //;VGM_ASSERT(res != 0, "OGG: bad reset=%i\n", res);

    data->discard = 0;
}

static void seek_ogg_vorbis(VGMSTREAM* v, int32_t num_sample) {
    ogg_vorbis_codec_data* data = v->codec_data;
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

void ogg_vorbis_set_disable_reordering(ogg_vorbis_codec_data* data, bool set) {
    if (!data) return;

    data->disable_reordering = set;
}

void ogg_vorbis_set_force_seek(ogg_vorbis_codec_data* data, bool set) {
    if (!data) return;

    data->force_seek = set;
}

STREAMFILE* ogg_vorbis_get_streamfile(ogg_vorbis_codec_data* data) {
    if (!data) return NULL;
    return data->io.streamfile;
}


const codec_info_t ogg_vorbis_decoder = {
    .sample_type = SFMT_FLT,
    .decode_frame = decode_frame_ogg_vorbis,
    .free = free_ogg_vorbis,
    .reset = reset_ogg_vorbis,
    .seek = seek_ogg_vorbis,
};

#endif
