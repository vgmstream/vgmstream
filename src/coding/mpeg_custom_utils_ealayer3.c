#include "mpeg_decoder.h"

#ifdef VGM_USE_MPEG

/**
 * Utils to parse EALayer3, an MP3 variant. EALayer3 frames have custom headers (removing unneded bits)
 * with regular MPEG data and optional PCM blocks. We transform EA-frames to MPEG-frames on the fly
 * and manually fill the sample PCM sample buffer.
 *
 * Layer III MPEG1 uses two granules (data chunks) per frame, while MPEG2/2.5 ("LSF mode") only one. EA-frames
 * contain one granule, so to reconstruct one MPEG-frame we need two EA-frames (MPEG1) or one (MPEG2).
 * EALayer v1 and v2 differ in part of the header, but are mostly the same.
 *
 * Reverse engineering: https://bitbucket.org/Zenchreal/ealayer3 (ealayer3.exe decoder)
 * Reference: https://www.mp3-tech.org/programmer/docs/mp3_theory.pdf
 *            https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/mpegaudiodec_template.c#L1306
 */

/* **************************************************************************** */
/* DEFS                                                                         */
/* **************************************************************************** */
 
#define EALAYER3_EA_FRAME_BUFFER_SIZE  0x1000*4  /* enough for one EA-frame */
#define EALAYER3_MAX_GRANULES  2
#define EALAYER3_MAX_CHANNELS  2

/* helper to simulate a bitstream */
typedef struct {
    uint8_t * buf;      /* buffer to read/write*/
    size_t bufsize;     /* max size of the buffer */
    off_t b_off;        /* current offset in bits inside the buffer */
    off_t offset;       /* info only */
} ealayer3_bitstream;

/* parsed info from a single EALayer3 frame */
typedef struct {
    /* EALayer3 v1 header */
    uint32_t v1_pcm_flag;
    uint32_t v1_pcm_decode_discard;
    uint32_t v1_pcm_number;

    /* EALayer3 v2 header */
    uint32_t v2_extended_flag;
    uint32_t v2_stereo_flag;
    uint32_t v2_unknown; /* unused? */
    uint32_t v2_frame_size; /* full size including headers and pcm block */
    uint32_t v2_mode; /* BLOCKOFFSETMODE: IGNORE = 0x0, PRESERVE = 0x1, MUTE = 0x2, MAX = 0x3 */
    uint32_t v2_mode_value; /* samples to use depending on mode */
    uint32_t v2_pcm_number;
    uint32_t v2_common_size; /* common header+data size; can be zero */

    /* EALayer3 common header + side info */
    uint32_t version_index;
    uint32_t sample_rate_index;
    uint32_t channel_mode;
    uint32_t mode_extension;

    uint32_t granule_index; /* 0 = first, 1 = second (for MPEG1, that needs pairs) */
    uint32_t scfsi[EALAYER3_MAX_CHANNELS]; /* SCaleFactor Selection Info */
    uint32_t main_data_size[EALAYER3_MAX_CHANNELS]; /* AKA part2_3_length */
    uint32_t others_1[EALAYER3_MAX_CHANNELS]; /* rest of the side info as-is, divided in 2 */
    uint32_t others_2[EALAYER3_MAX_CHANNELS];

    /* derived from the above */
    uint32_t data_offset_b; /* start of the MPEG data */
    uint32_t pre_size; /* size of the V1/V2 part */
    uint32_t base_size_b; /* size (bits) of the header+side info, up to data_size */
    uint32_t data_size_b; /* size (bits) of the main MPEG data up to pcm block; can be zero */
    uint32_t padding_size_b; /* size (bits) of the padding after base+data */
    uint32_t common_size; /* size of the common part (base+data+padding) */
    uint32_t pcm_size; /* size of the pcm block */
    uint32_t eaframe_size; /* size of all of the above, for convenience */

    int mpeg1; /* flag, as MPEG2/2.5 ("low sample frequency" mode) has some differences */
    int version;
    int channels;
    int sample_rate;

} ealayer3_frame_info;


static int ealayer3_parse_frame(mpeg_codec_data *data, ealayer3_bitstream *is, ealayer3_frame_info * eaf);
static int ealayer3_parse_frame_v1(ealayer3_bitstream *is, ealayer3_frame_info * eaf, int channels_per_frame);
static int ealayer3_parse_frame_v2(ealayer3_bitstream *is, ealayer3_frame_info * eaf);
static int ealayer3_parse_frame_common(ealayer3_bitstream *is, ealayer3_frame_info * eaf);
static int ealayer3_rebuild_mpeg_frame(ealayer3_bitstream* is_0, ealayer3_frame_info* eaf_0, ealayer3_bitstream* is_1, ealayer3_frame_info* eaf_1, ealayer3_bitstream* os);
static int ealayer3_write_pcm_block(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, ealayer3_frame_info * eaf);

static int r_bits(ealayer3_bitstream * iw, int num_bits, uint32_t * value);
static int w_bits(ealayer3_bitstream * ow, int num_bits, uint32_t value);


/* **************************************************************************** */
/* EXTERNAL API                                                                 */
/* **************************************************************************** */

/* init codec from a EALayer3 frame */
int mpeg_custom_setup_init_ealayer3(STREAMFILE *streamFile, off_t start_offset, mpeg_codec_data *data, coding_t *coding_type) {
    int ok;
    ealayer3_frame_info eaf;
    ealayer3_bitstream is;
    uint8_t ibuf[EALAYER3_EA_FRAME_BUFFER_SIZE];

    //;VGM_LOG("EAFRAME: EALayer3 init at %lx\n", start_offset);

    if (data->type == MPEG_EAL32P || data->type == MPEG_EAL32S)
        goto fail; /* untested */

    /* get first frame for info */
    {
        is.buf = ibuf;
        is.bufsize = read_streamfile(ibuf,start_offset,EALAYER3_EA_FRAME_BUFFER_SIZE, streamFile); /* reads less at EOF */;
        is.b_off = 0;

        ok = ealayer3_parse_frame(data, &is, &eaf);
        if (!ok) goto fail;
    }

    ;VGM_ASSERT(!eaf.mpeg1, "MPEG EAL3: mpeg2 found at 0x%lx\n", start_offset);

    *coding_type = coding_MPEG_ealayer3;
    data->channels_per_frame = eaf.channels;
    data->samples_per_frame = eaf.mpeg1 ? 1152 : 576;

    /* extra checks */
    if (!data->channels_per_frame || data->config.channels != data->channels_per_frame){
        VGM_LOG("MPEG EAL3: unknown %i multichannel layout\n", data->config.channels);
        goto fail; /* unknown layout */
    }


    /* encoder delay: EALayer3 handles this while decoding (skips samples as writes PCM blocks) */

    return 1;
fail:
    return 0;
}

/* writes data to the buffer and moves offsets, transforming EALayer3 frames */
int mpeg_custom_parse_frame_ealayer3(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream) {
    int ok;
    off_t current_offset = stream->offset;

    ealayer3_frame_info eaf_0, eaf_1;
    ealayer3_bitstream is_0, is_1, os;
    uint8_t ibuf_0[EALAYER3_EA_FRAME_BUFFER_SIZE], ibuf_1[EALAYER3_EA_FRAME_BUFFER_SIZE];

    /* read first frame/granule */
    {
        is_0.buf = ibuf_0;
        is_0.bufsize = read_streamfile(ibuf_0,stream->offset,EALAYER3_EA_FRAME_BUFFER_SIZE, stream->streamfile); /* reads less at EOF */
        is_0.b_off = 0;

        ok = ealayer3_parse_frame(data, &is_0, &eaf_0);
        if (!ok) goto fail;

        ok = ealayer3_write_pcm_block(stream, data, num_stream, &eaf_0);
        if (!ok) goto fail;

        stream->offset += eaf_0.eaframe_size;
    }

    /* get second frame/granule */
    if (eaf_0.mpeg1) {
        int granule1_found;
        do {
            is_1.buf = ibuf_1;
            is_1.bufsize = read_streamfile(ibuf_1,stream->offset,EALAYER3_EA_FRAME_BUFFER_SIZE, stream->streamfile); /* reads less at EOF */
            is_1.b_off = 0;

            ok = ealayer3_parse_frame(data, &is_1, &eaf_1);
            if (!ok) goto fail;

            ok = ealayer3_write_pcm_block(stream, data, num_stream, &eaf_1);
            if (!ok) goto fail;

            stream->offset += eaf_1.eaframe_size;


            /* in V1 sometimes there is a PCM block between two granules, try next */
            if (eaf_1.v1_pcm_flag == 0xEE)
                granule1_found = 0;
            else
                granule1_found = 1; /* assume it does (bad infinite loops) */
        }
        while(!granule1_found);
    }
    else {
        memset(&eaf_1, 0, sizeof(ealayer3_frame_info));
    }

    /* rebuild EALayer frame to MPEG frame */
    {
        os.buf = data->buffer;
        os.bufsize = data->buffer_size;
        os.b_off = 0;
        os.offset = current_offset;

        ok = ealayer3_rebuild_mpeg_frame(&is_0, &eaf_0, &is_1, &eaf_1, &os);
        if (!ok) goto fail;

        data->bytes_in_buffer = os.b_off / 8; /* wrote full MPEG frame, hopefully */
    }


    return 1;
fail:
    return 0;
}


/* **************************************************************************** */
/* INTERNAL HELPERS                                                             */
/* **************************************************************************** */

static int ealayer3_parse_frame(mpeg_codec_data *data, ealayer3_bitstream *is, ealayer3_frame_info * eaf) {
    int ok;

    memset(eaf, 0, sizeof(ealayer3_frame_info));

    switch(data->type) {
        case MPEG_EAL31:        ok = ealayer3_parse_frame_v1(is, eaf, data->channels_per_frame); break;
        case MPEG_EAL32P:
        case MPEG_EAL32S:       ok = ealayer3_parse_frame_v2(is, eaf); break;
        default: goto fail;
    }
    if (!ok) goto fail;


    //;VGM_LOG("EAFRAME: v=%i, ch=%i, sr=%i, index=%i / pre=%x, common=%x, pcm=%x, eaframe=%x\n", eaf->version, eaf->channels, eaf->sample_rate, eaf->granule_index, eaf->pre_size, eaf->common_size, eaf->pcm_size, eaf->eaframe_size);
    //if (data->type==MPEG_EAL31) VGM_LOG("EAFRAME v1: pcm=%x, unk=%x, number=%x\n", eaf->v1_pcm_flag, eaf->v1_pcm_unknown, eaf->v1_pcm_number);
    //else VGM_LOG("EAFRAME v2: stereo=%x, unk=%x, fs=%x, mode=%x, val=%x, number=%x, size=%x\n", eaf->v2_stereo_flag, eaf->v2_unknown, eaf->v2_frame_size, eaf->v2_mode, eaf->v2_mode_value, eaf->v2_pcm_number, eaf->v2_common_size);

    return 1;
fail:
    return 0;
}


static int ealayer3_parse_frame_v1(ealayer3_bitstream *is, ealayer3_frame_info * eaf, int channels_per_frame) {
    int ok;

    /* read EA-frame V1 header */
    r_bits(is,  8,&eaf->v1_pcm_flag);

    eaf->pre_size = 1; /* 8b */

    if (eaf->v1_pcm_flag != 0x00 && eaf->v1_pcm_flag != 0xEE) {
        VGM_LOG("MPEG EAL3 v1: header not 0x00 or 0xEE\n");
        goto fail; /* wrong offset? */
    }


    /* check PCM block */
    if (eaf->v1_pcm_flag == 0xEE) {
        r_bits(is, 16,&eaf->v1_pcm_decode_discard); /* samples to discard of the next decoded (not PCM block) samples */
        r_bits(is, 16,&eaf->v1_pcm_number); /* number of PCM samples, can be 0 */

        if (!channels_per_frame) {
            VGM_LOG("MPEG EAL3 v1: PCM block as first frame\n");
            goto fail; /* must know from a prev frame */
        }

        eaf->pre_size += 2+2; /* 16b+16b */
        eaf->pcm_size = (2*eaf->v1_pcm_number * channels_per_frame);
    }
    else {
        /* read EA-frame common header */
        ok = ealayer3_parse_frame_common(is, eaf);
        if (!ok) goto fail;
    }

    eaf->eaframe_size = eaf->pre_size + eaf->common_size + eaf->pcm_size;

    return 1;
fail:
    return 0;
}


static int ealayer3_parse_frame_v2(ealayer3_bitstream *is, ealayer3_frame_info * eaf) {
    int ok;

    /* read EA-frame V2 header */
    r_bits(is,  1,&eaf->v2_extended_flag);
    r_bits(is,  1,&eaf->v2_stereo_flag);
    r_bits(is,  2,&eaf->v2_unknown);
    r_bits(is, 12,&eaf->v2_frame_size);
    
    eaf->pre_size = 2; /* 16b */

    if (eaf->v2_extended_flag) {
        r_bits(is,  2,&eaf->v2_mode);
        r_bits(is, 10,&eaf->v2_mode_value);
        r_bits(is, 10,&eaf->v2_pcm_number);
        r_bits(is, 10,&eaf->v2_common_size);
        
        eaf->pre_size += 4; /* 32b */
    }

    /* read EA-frame common header */
    ok = ealayer3_parse_frame_common(is, eaf);
    if (!ok) goto fail;

    //todo maybe v2 frames can be PCM-only like v1
    if (!eaf->channels) {
        VGM_LOG("MPEG EAL3: v2 frame with no channel number");
        goto fail;
    }

    eaf->pcm_size = (2*eaf->v2_pcm_number * eaf->channels);

    eaf->eaframe_size = eaf->pre_size + eaf->common_size + eaf->pcm_size;

    if(eaf->v2_frame_size != eaf->eaframe_size) {
        VGM_LOG("MPEG EAL3: different v2 frame size vs calculated (0x%x vs 0x%x)\n", eaf->v2_frame_size, eaf->eaframe_size);
        goto fail;
    }


    return 1;
fail:
    return 0;
}


/* Parses a EALayer3 frame (common part) */
static int ealayer3_parse_frame_common(ealayer3_bitstream *is, ealayer3_frame_info * eaf) {
    /* index tables */
    static const int versions[4] = { /* MPEG 2.5 */ 3, /* reserved */ -1,  /* MPEG 2 */ 2, /* MPEG 1 */ 1 };
    static const int sample_rates[4][4] = { /* [version_index][sample rate index] */
            { 11025, 12000,  8000, -1}, /* MPEG2.5 */
            {    -1,    -1,    -1, -1}, /* reserved */
            { 22050, 24000, 16000, -1}, /* MPEG2 */
            { 44100, 48000, 32000, -1}, /* MPEG1 */
    };
    static const int channels[4] = { 2,2,2, 1 }; /* [channel_mode] */

    off_t start_b_off = is->b_off;
    int i;

    /* read main header */
    r_bits(is,  2,&eaf->version_index);
    r_bits(is,  2,&eaf->sample_rate_index);
    r_bits(is,  2,&eaf->channel_mode);
    r_bits(is,  2,&eaf->mode_extension);

    /* check empty frame */
    if (eaf->version_index == 0 &&
        eaf->sample_rate_index == 0 &&
        eaf->channel_mode == 0 &&
        eaf->mode_extension == 0) {
        VGM_LOG("MPEG EAL3: empty frame\n");
        goto fail;
    }

    /* derived */
    eaf->version = versions[eaf->version_index];
    eaf->channels = channels[eaf->channel_mode];
    eaf->sample_rate = sample_rates[eaf->version_index][eaf->sample_rate_index];
    eaf->mpeg1 = (eaf->version == 1);

    if (eaf->version == -1 || eaf->sample_rate == -1) {
        VGM_LOG("MPEG EAL3: illegal header values\n");
        goto fail;
    }
    

    /* read side info */
    r_bits(is,  1,&eaf->granule_index);

    if (eaf->mpeg1 && eaf->granule_index == 1) {
        for (i = 0; i < eaf->channels; i++) {
            r_bits(is,  4,&eaf->scfsi[i]);
        }
    }

    for (i = 0; i < eaf->channels; i++) {
        int others_2_bits = eaf->mpeg1 ? 47-32 : 51-32;

        r_bits(is, 12,&eaf->main_data_size[i]);
        /* divided in 47b=32+15 (MPEG1) or 51b=32+19 (MPEG2), arbitrarily */
        r_bits(is, 32,&eaf->others_1[i]);
        r_bits(is, others_2_bits,&eaf->others_2[i]);
    }


    /* derived */
    eaf->data_offset_b = is->b_off;

    eaf->base_size_b = (is->b_off - start_b_off); /* header + size info size */

    for (i = 0; i < eaf->channels; i++) { /* data size (can be 0, meaning a micro EA-frame) */
        eaf->data_size_b += eaf->main_data_size[i];
    }

    if ((eaf->base_size_b+eaf->data_size_b) % 8) /* aligned to closest 8b */
        eaf->padding_size_b = 8 - ((eaf->base_size_b+eaf->data_size_b) % 8);

    eaf->common_size = (eaf->base_size_b + eaf->data_size_b + eaf->padding_size_b)/8;


    return 1;
fail:
    return 0;
}


/* Converts a EALAYER3 frame to a standard MPEG frame from pre-parsed info */
static int ealayer3_rebuild_mpeg_frame(ealayer3_bitstream* is_0, ealayer3_frame_info* eaf_0, ealayer3_bitstream* is_1, ealayer3_frame_info* eaf_1, ealayer3_bitstream* os) {
    uint32_t c = 0;
    int i,j;
    int expected_bitrate_index, expected_frame_size;

    if (!eaf_0->common_size && !eaf_1->common_size)
        return 1; /* empty frames, PCM block only */

    /* get bitrate: use max bitrate (320/160) to simplify calcs for now (but some EA-frames use bit reservoir) */
    expected_bitrate_index = 0x0E;
    if (eaf_0->mpeg1) { /* 44100=0x414, 48000=0x3C0, 32000=0x5A0 */
        expected_frame_size = 144l * 320 * 1000l / eaf_0->sample_rate;
    } else { /* 22050=0x20A, 24000=0x1E0, 16000=0x2D0, 11025=0x414, 12000=0x3C0, 8000=0x5A0 */
        expected_frame_size =  72l * 160 * 1000l / eaf_0->sample_rate;
    }

    /* extra checks */
    if (eaf_0->mpeg1) {
        if (!eaf_1
                || eaf_0->mpeg1 != eaf_1->mpeg1
                || eaf_0->version != eaf_1->version
                || eaf_0->granule_index == eaf_1->granule_index
                || !eaf_0->common_size || !eaf_1->common_size) {
            VGM_LOG("MPEG EAL3: EA-frames for MPEG1 don't match at 0x%lx\n", os->offset);
            goto fail;
        }
    }


    /* write MPEG1/2 frame header */
    w_bits(os, 11, 0x7FF);  /* sync */
    w_bits(os,  2, eaf_0->version_index);
    w_bits(os,  2, 0x01);  /* layer III index */
    w_bits(os,  1, 1);  /* "no CRC" flag */
    w_bits(os,  4, expected_bitrate_index);
    w_bits(os,  2, eaf_0->sample_rate_index);
    w_bits(os,  1, 0);  /* padding */
    w_bits(os,  1, 0);  /* private */
    w_bits(os,  2, eaf_0->channel_mode);
    w_bits(os,  2, eaf_0->mode_extension);
    w_bits(os,  1, 1);  /* copyrighted */
    w_bits(os,  1, 1);  /* original */
    w_bits(os,  2, 0);  /* emphasis */

    if (eaf_0->mpeg1) {
        int private_bits = (eaf_0->channels==1 ? 5 : 3);

        /* write MPEG1 side info */
        w_bits(os,  9, 0);  /* main data start (no bit reservoir) */
        w_bits(os,  private_bits, 0);

        for (i = 0; i < eaf_1->channels; i++) {
            w_bits(os,  4, eaf_1->scfsi[i]); /* saved in granule1 only */
        }
        for (i = 0; i < eaf_0->channels; i++) { /* granule0 */
            w_bits(os, 12,    eaf_0->main_data_size[i]);
            w_bits(os, 32,    eaf_0->others_1[i]);
            w_bits(os, 47-32, eaf_0->others_2[i]);
        }
        for (i = 0; i < eaf_1->channels; i++) { /* granule1 */
            w_bits(os, 12,    eaf_1->main_data_size[i]);
            w_bits(os, 32,    eaf_1->others_1[i]);
            w_bits(os, 47-32, eaf_1->others_2[i]);
        }

        /* write MPEG1 main data */
        is_0->b_off = eaf_0->data_offset_b;
        for (i = 0; i < eaf_0->channels; i++) { /* granule0 */
            for (j = 0; j < eaf_0->main_data_size[i]; j++) {
                uint32_t c = 0;
                r_bits(is_0,  1, &c);
                w_bits(os,    1,  c);
            }
        }

        is_1->b_off = eaf_1->data_offset_b;
        for (i = 0; i < eaf_1->channels; i++) { /* granule1 */
            for (j = 0; j < eaf_1->main_data_size[i]; j++) {
                r_bits(is_1,  1, &c);
                w_bits(os,    1,  c);
            }
        }
    }
    else {
        int private_bits = (eaf_0->channels==1 ? 1 : 2);

        /* write MPEG2 side info */
        w_bits(os,  8, 0);  /* main data start (no bit reservoir) */
        w_bits(os,  private_bits, 0);

        for (i = 0; i < eaf_0->channels; i++) {
            w_bits(os, 12,    eaf_0->main_data_size[i]);
            w_bits(os, 32,    eaf_0->others_1[i]);
            w_bits(os, 51-32, eaf_0->others_2[i]);
        }

        /* write MPEG2 main data */
        is_0->b_off = eaf_0->data_offset_b;
        for (i = 0; i < eaf_0->channels; i++) {
            for (j = 0; j < eaf_0->main_data_size[i]; j++) {
                r_bits(is_0,  1, &c);
                w_bits(os,    1,  c);
            }
        }
    }

    /* align to closest 8b */
    if (os->b_off % 8) {
        int align_bits = 8 - (os->b_off % 8);
        w_bits(os,  align_bits,  0);
    }


    if (os->b_off/8 > expected_frame_size)  {
        VGM_LOG("MPEG EAL3: written 0x%lx but expected less than 0x%x at 0x%lx\n", os->b_off/8, expected_frame_size, os->offset);
        //todo bit reservoir! (doesn't seem to affect the output too much)

        //;VGM_LOG("EAFRAME: F0 v=%i, ch=%i, sr=%i, index=%i / pre=%x, common=%x, pcm=%x, eaframe=%x\n", eaf_0->version, eaf_0->channels, eaf_0->sample_rate, eaf_0->granule_index, eaf_0->pre_size, eaf_0->common_size, eaf_0->pcm_size, eaf_0->eaframe_size);
        //;VGM_LOG("EAFRAME: F1 v=%i, ch=%i, sr=%i, index=%i / pre=%x, common=%x, pcm=%x, eaframe=%x\n", eaf_1->version, eaf_1->channels, eaf_1->sample_rate, eaf_1->granule_index, eaf_1->pre_size, eaf_1->common_size, eaf_1->pcm_size, eaf_1->eaframe_size);
        //;VGM_LOGB(os->buf, os->b_off/8, 0);
    }
    else {
        /* fill ancillary data (ignored) */
        memset(os->buf + os->b_off/8, 0x77, expected_frame_size - os->b_off/8);
    }

    os->b_off = expected_frame_size*8;


    return 1;
fail:
    return 0;
}

/* write PCM block directly to sample buffer (EALayer3 seems to use this as a prefectch of sorts) */
static int ealayer3_write_pcm_block(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, ealayer3_frame_info * eaf) {
    mpeg_custom_stream *ms = data->streams[num_stream];
    size_t bytes_filled;
    int i;

    if (!eaf->pcm_size)
        return 1;

    bytes_filled = sizeof(sample)*ms->samples_filled*data->channels_per_frame;
    if (bytes_filled + eaf->pcm_size > ms->output_buffer_size) {
       VGM_LOG("MPEG EAL3: can't fill the sample buffer with 0x%x\n", eaf->pcm_size);
       goto fail;
    }


    if (eaf->v1_pcm_number) {
        //;VGM_LOG("pcm discard = %i, number = %i at 0x%lx\n", eaf->v1_pcm_decode_discard, eaf->v1_pcm_number, stream->offset);
        VGM_ASSERT(eaf->v1_pcm_decode_discard > 576, "MPEG EAL3: big discard %i at 0x%lx\n", eaf->v1_pcm_decode_discard, stream->offset);
        VGM_ASSERT(eaf->v1_pcm_number > 0x100, "MPEG EAL3: big samples %i at 0x%lx\n", eaf->v1_pcm_number, stream->offset);

        /* read + write PCM block samples (always BE) */
        for (i = 0; i < eaf->v1_pcm_number * data->channels_per_frame; i++) {
            off_t pcm_offset = stream->offset + eaf->pre_size + eaf->common_size + sizeof(sample)*i;
            int16_t pcm_sample = read_16bitBE(pcm_offset,stream->streamfile);
            put_16bitLE(ms->output_buffer + bytes_filled + sizeof(sample)*i, pcm_sample);
        }
        ms->samples_filled += eaf->v1_pcm_number;

        /* skip decoded samples as PCM block 'overwrites' them */
        {
            size_t decode_to_discard = eaf->v1_pcm_decode_discard;

            //todo should also discard v1_pcm_number, but block layout samples may be exhausted and won't move (maybe new block if offset = new offset detected)
            /* special meanings */
            if (decode_to_discard == 576)
                decode_to_discard = data->samples_per_frame;//+ eaf->v1_pcm_number;

            ms->decode_to_discard += decode_to_discard;
        }
    }



    /* todo V2 (id7 only?) supposed skip modes:
     * BLOCKOFFSETMODE: IGNORE = 0x0, PRESERVE = 0x1, MUTE = 0x2, MAX = 0x3
     *
     * AB00CCCC CCCCCCCC  if A is set:  DDEEEEEE EEEEFFFF FFFFFFGG GGGGGGGG
     * D = mode:
     * E = bytes to discard (mode == 0) or skip (mode == 1 or 2) before outputting the uncompressed samples
     *    (when mode == 3 this is ignored)
     * F = number of uncompressed sample frames
     * G = MPEG granule size (can be zero)
     *
     *   if 0: 576 - E if G == 0 then F
     *   if 1: 576 if G == 0 then F
     *   if 2: 576 if G == 0 then F * 2
     *   if 3: 576
     */

    return 1;
fail:
    return 0;
}

/* ********************************************* */

/* Read bits (max 32) from buf and update the bit offset. Order is BE (MSF). */
static int r_bits(ealayer3_bitstream * is, int num_bits, uint32_t * value) {
    off_t off, pos;
    int i, bit_buf, bit_val;
    if (num_bits == 0) return 1;
    if (num_bits > 32 || num_bits < 0 || is->b_off + num_bits > is->bufsize*8) goto fail;

    *value = 0; /* set all bits to 0 */
    off = is->b_off / 8; /* byte offset */
    pos = is->b_off % 8; /* bit sub-offset */
    for (i = 0; i < num_bits; i++) {
        bit_buf = (1U << (8-1-pos)) & 0xFF;   /* bit check for buf */
        bit_val = (1U << (num_bits-1-i));     /* bit to set in value */

        if (is->buf[off] & bit_buf)         /* is bit in buf set? */
            *value |= bit_val;              /* set bit */

        pos++;
        if (pos%8 == 0) {                   /* new byte starts */
            pos = 0;
            off++;
        }
    }

    is->b_off += num_bits;
    return 1;
fail:
    return 0;
}

/* Write bits (max 32) to buf and update the bit offset. Order is BE (MSF). */
static int w_bits(ealayer3_bitstream * os, int num_bits, uint32_t value) {
    off_t off, pos;
    int i, bit_val, bit_buf;
    if (num_bits == 0) return 1;
    if (num_bits > 32 || num_bits < 0 || os->b_off + num_bits > os->bufsize*8) goto fail;


    off = os->b_off / 8; /* byte offset */
    pos = os->b_off % 8; /* bit sub-offset */
    for (i = 0; i < num_bits; i++) {
        bit_val = (1U << (num_bits-1-i));     /* bit check for value */
        bit_buf = (1U << (8-1-pos)) & 0xFF;   /* bit to set in buf */

        if (value & bit_val)                /* is bit in val set? */
            os->buf[off] |= bit_buf;        /* set bit */
        else
            os->buf[off] &= ~bit_buf;       /* unset bit */

        pos++;
        if (pos%8 == 0) {                   /* new byte starts */
            pos = 0;
            off++;
        }
    }

    os->b_off += num_bits;
    return 1;
fail:
    return 0;
}

#endif
