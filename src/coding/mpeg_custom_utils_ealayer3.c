#include "mpeg_decoder.h"

#ifdef VGM_USE_MPEG

/**
 * Utils to parse EALayer3, an MP3 variant. EALayer3 frames have custom headers (removing unneded bits)
 * with regular MPEG data and optional PCM blocks. We transform EA-frames to MPEG-frames on the fly
 * and manually fill the sample PCM sample buffer.
 *
 * Layer III MPEG1 uses two granules (data chunks) per frame, while MPEG2/2.5 ("LSF mode") only one.
 * EA-frames contain one granule, so to reconstruct one MPEG-frame we need two EA-frames (MPEG1) or
 * one (MPEG2). This is only for our decoder, real EALayer3 would decode EA-frames directly.
 * EALayer v1 and v2 differ in part of the header, but are mostly the same.
 *
 * Reverse engineering by Zench: https://bitbucket.org/Zenchreal/ealayer3 (ealayer3.exe decoder)
 * Reference: https://www.mp3-tech.org/programmer/docs/mp3_theory.pdf
 *            https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/mpegaudiodec_template.c#L1306
 */

/* **************************************************************************** */
/* DEFS                                                                         */
/* **************************************************************************** */
 
#define EALAYER3_EA_FRAME_BUFFER_SIZE  0x1000*4  /* enough for one EA-frame */
#define EALAYER3_MAX_GRANULES  2
#define EALAYER3_MAX_CHANNELS  2

/* parsed info from a single EALayer3 frame */
typedef struct {
    /* EALayer3 v1 header */
    uint32_t v1_pcm_flag;
    uint32_t v1_pcm_decode_discard;
    uint32_t v1_pcm_number;
    uint32_t v1_pcm_unknown;

    /* EALayer3 v2 header */
    uint32_t v2_extended_flag;
    uint32_t v2_stereo_flag;
    uint32_t v2_unknown; /* unused? */
    uint32_t v2_frame_size; /* full size including headers and pcm block */
    uint32_t v2_mode; /* discard mode */
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


static int ealayer3_parse_frame(mpeg_codec_data *data, vgm_bitstream *is, ealayer3_frame_info * eaf);
static int ealayer3_parse_frame_v1(vgm_bitstream *is, ealayer3_frame_info * eaf, int channels_per_frame, int is_v1b);
static int ealayer3_parse_frame_v2(vgm_bitstream *is, ealayer3_frame_info * eaf);
static int ealayer3_parse_frame_common(vgm_bitstream *is, ealayer3_frame_info * eaf);
static int ealayer3_rebuild_mpeg_frame(vgm_bitstream* is_0, ealayer3_frame_info* eaf_0, vgm_bitstream* is_1, ealayer3_frame_info* eaf_1, vgm_bitstream* os);
static int ealayer3_write_pcm_block(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, ealayer3_frame_info * eaf);
static int ealayer3_skip_data(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, int at_start);

/* **************************************************************************** */
/* EXTERNAL API                                                                 */
/* **************************************************************************** */

/* init codec from an EALayer3 frame */
int mpeg_custom_setup_init_ealayer3(STREAMFILE *streamFile, off_t start_offset, mpeg_codec_data *data, coding_t *coding_type) {
    int ok;
    ealayer3_frame_info eaf;
    vgm_bitstream is = {0};
    uint8_t ibuf[EALAYER3_EA_FRAME_BUFFER_SIZE];

    //;VGM_LOG("init at %lx\n", start_offset);

    /* get first frame for info */
    {
        is.buf = ibuf;
        is.bufsize = read_streamfile(ibuf,start_offset,EALAYER3_EA_FRAME_BUFFER_SIZE, streamFile); /* reads less at EOF */;
        is.b_off = 0;

        ok = ealayer3_parse_frame(data, &is, &eaf);
        if (!ok) goto fail;
    }
    VGM_ASSERT(!eaf.mpeg1, "MPEG EAL3: mpeg2 found at 0x%lx\n", start_offset); /* untested */

    *coding_type = coding_MPEG_ealayer3;
    data->channels_per_frame = eaf.channels;
    data->samples_per_frame = eaf.mpeg1 ? 1152 : 576;

    /* encoder delay: EALayer3 handles this while decoding (skips samples as writes PCM blocks) */

    return 1;
fail:
    return 0;
}

/* writes data to the buffer and moves offsets, transforming EALayer3 frames */
int mpeg_custom_parse_frame_ealayer3(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream) {
    mpeg_custom_stream *ms = data->streams[num_stream];
    int ok, granule_found;
    off_t info_offset = stream->offset;

    ealayer3_frame_info eaf_0, eaf_1;
    vgm_bitstream is_0 = {0}, is_1 = {0}, os = {0};
    uint8_t ibuf_0[EALAYER3_EA_FRAME_BUFFER_SIZE], ibuf_1[EALAYER3_EA_FRAME_BUFFER_SIZE];

    /* read first frame/granule, or PCM-only frame (found alone at the end of SCHl streams) */
	{
        //;VGM_LOG("s%i: get granule0 at %lx\n", num_stream,stream->offset);
        if (!ealayer3_skip_data(stream, data, num_stream, 1))
            goto fail;

        is_0.buf = ibuf_0;
        is_0.bufsize = read_streamfile(ibuf_0,stream->offset,EALAYER3_EA_FRAME_BUFFER_SIZE, stream->streamfile); /* reads less at EOF */
        is_0.b_off = 0;

        ok = ealayer3_parse_frame(data, &is_0, &eaf_0);
        if (!ok) goto fail;

        ok = ealayer3_write_pcm_block(stream, data, num_stream, &eaf_0);
        if (!ok) goto fail;

        stream->offset += eaf_0.eaframe_size;
        //;VGM_LOG("s%i: get granule0 done at %lx (eaf_size=%x, common_size=%x)\n", num_stream,stream->offset, eaf_0.eaframe_size, eaf_0.common_size);

        if (!ealayer3_skip_data(stream, data, num_stream, 0))
            goto fail;

	}

    /* get second frame/granule (MPEG1 only) if first granule was found */
    granule_found = 0;
    while (eaf_0.common_size && eaf_0.mpeg1 && !granule_found) {
        //;VGM_LOG("s%i: get granule1 at %lx\n", num_stream,stream->offset);
        if (!ealayer3_skip_data(stream, data, num_stream, 1))
            goto fail;

        is_1.buf = ibuf_1;
        is_1.bufsize = read_streamfile(ibuf_1,stream->offset,EALAYER3_EA_FRAME_BUFFER_SIZE, stream->streamfile); /* reads less at EOF */
        is_1.b_off = 0;

        ok = ealayer3_parse_frame(data, &is_1, &eaf_1);
        if (!ok) goto fail;

        ok = ealayer3_write_pcm_block(stream, data, num_stream, &eaf_1);
        if (!ok) goto fail;

        stream->offset += eaf_1.eaframe_size;
        //;VGM_LOG("s%i: get granule0 done at %lx (eaf_size=%x, common_size=%x)\n", num_stream,stream->offset, eaf_0.eaframe_size, eaf_0.common_size);

        if (!ealayer3_skip_data(stream, data, num_stream, 0))
            goto fail;

        /* in V1a there may be PCM-only frames between granules so read until next one (or parse fails) */
        if (eaf_1.common_size > 0)
            granule_found = 1;
    }

    /* rebuild EALayer frame to MPEG frame */
    {
        os.buf = ms->buffer;
        os.bufsize = ms->buffer_size;
        os.b_off = 0;
        os.info_offset = info_offset;

        ok = ealayer3_rebuild_mpeg_frame(&is_0, &eaf_0, &is_1, &eaf_1, &os);
        if (!ok) goto fail;

        ms->bytes_in_buffer = os.b_off / 8; /* wrote full MPEG frame, hopefully */
    }

    return 1;
fail:
    return 0;
}


/* **************************************************************************** */
/* INTERNAL HELPERS                                                             */
/* **************************************************************************** */

static int ealayer3_parse_frame(mpeg_codec_data *data, vgm_bitstream *is, ealayer3_frame_info * eaf) {
    int ok;

    /* make sure as there is re-parsing in loops */
    memset(eaf, 0, sizeof(ealayer3_frame_info));

    switch(data->type) {
        case MPEG_EAL31:        ok = ealayer3_parse_frame_v1(is, eaf, data->channels_per_frame, 0); break;
        case MPEG_EAL31b:       ok = ealayer3_parse_frame_v1(is, eaf, data->channels_per_frame, 1); break;
        case MPEG_EAL32P:
        case MPEG_EAL32S:       ok = ealayer3_parse_frame_v2(is, eaf); break;
        default: goto fail;
    }
    if (!ok) goto fail;

    return 1;
fail:
    return 0;
}


/* read V1"a" (in SCHl) and V1"b" (in SNS) EALayer3 frame */
static int ealayer3_parse_frame_v1(vgm_bitstream *is, ealayer3_frame_info * eaf, int channels_per_frame, int is_v1b) {
    int ok;

    /* read EA-frame V1 header */
    r_bits(is,  8,&eaf->v1_pcm_flag);

    eaf->pre_size = 1; /* 8b */

    if (eaf->v1_pcm_flag != 0x00 && eaf->v1_pcm_flag != 0xEE) {
        VGM_LOG("MPEG EAL3 v1: header not 0x00 or 0xEE\n");
        goto fail; /* wrong offset? */
    }
    if (eaf->v1_pcm_flag == 0xEE && !channels_per_frame) {
        VGM_LOG("MPEG EAL3 v1: PCM block in first frame\n");
        goto fail; /* must know from a prev frame */
    }

    /* read EA-frame common header (v1a PCM blocks don't have EA-frames, while v1b do) */
    if (is_v1b || eaf->v1_pcm_flag == 0x00) {
        ok = ealayer3_parse_frame_common(is, eaf);
        if (!ok) goto fail;
    }

    /* check PCM block */
    if (eaf->v1_pcm_flag == 0xEE) {
        r_bits(is, 16,&eaf->v1_pcm_decode_discard); /* samples to discard of the next decoded (not PCM block) samples */
        r_bits(is, 16,&eaf->v1_pcm_number); /* number of PCM samples, can be 0 */

        eaf->pre_size += 2+2; /* 16b+16b */
        eaf->pcm_size = (2*eaf->v1_pcm_number * channels_per_frame);

        if (is_v1b) { /* extra 32b in v1b */
            r_bits(is, 32,&eaf->v1_pcm_unknown);
            eaf->pre_size += 4; /* 32b */
            VGM_ASSERT(eaf->v1_pcm_unknown != 0, "EA EAL3 v1: v1_pcm_unknown not 0\n");
        }
    }

    eaf->eaframe_size = eaf->pre_size + eaf->common_size + eaf->pcm_size;

    return 1;
fail:
    return 0;
}

/* read V2"PCM" and V2"Spike" EALayer3 frame (exactly the same but V2P seems to have bigger
 * PCM blocks and maybe smaller frames) */
static int ealayer3_parse_frame_v2(vgm_bitstream *is, ealayer3_frame_info * eaf) {
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
    if (!eaf->v2_extended_flag || (eaf->v2_extended_flag && eaf->v2_common_size)) {
        ok = ealayer3_parse_frame_common(is, eaf);
        if (!ok) goto fail;
    }
    VGM_ASSERT(eaf->v2_extended_flag && eaf->v2_common_size == 0, "EA EAL3: v2 empty frame\n"); /* seen in V2S */
    VGM_ASSERT(eaf->v2_extended_flag && eaf->v2_mode_value > 0, "EA EAL3: v2_mode=%x with 0x%x\n", eaf->v2_mode, eaf->v2_mode_value);
    //VGM_ASSERT(eaf->v2_pcm_number > 0, "EA EAL3: v2_pcm_number 0x%x\n", eaf->v2_pcm_number);

    eaf->pcm_size = (2*eaf->v2_pcm_number * eaf->channels);

    eaf->eaframe_size = eaf->pre_size + eaf->common_size + eaf->pcm_size;

    if (eaf->v2_frame_size != eaf->eaframe_size) {
        VGM_LOG("MPEG EAL3: different v2 frame size vs calculated (0x%x vs 0x%x)\n", eaf->v2_frame_size, eaf->eaframe_size);
        goto fail;
    }


    return 1;
fail:
    return 0;
}


/* parses an EALayer3 frame (common part) */
static int ealayer3_parse_frame_common(vgm_bitstream *is, ealayer3_frame_info * eaf) {
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
    is->b_off += eaf->data_size_b;

    if ((eaf->base_size_b+eaf->data_size_b) % 8) /* aligned to closest 8b */
        eaf->padding_size_b = 8 - ((eaf->base_size_b+eaf->data_size_b) % 8);
    is->b_off += eaf->padding_size_b;

    eaf->common_size = (eaf->base_size_b + eaf->data_size_b + eaf->padding_size_b)/8;


    return 1;
fail:
    return 0;
}


/* converts an EALAYER3 frame to a standard MPEG frame from pre-parsed info */
static int ealayer3_rebuild_mpeg_frame(vgm_bitstream* is_0, ealayer3_frame_info* eaf_0, vgm_bitstream* is_1, ealayer3_frame_info* eaf_1, vgm_bitstream* os) {
    uint32_t c = 0;
    int i,j;
    int expected_bitrate_index, expected_frame_size;

    /* ignore PCM-only frames */
    if (!eaf_0->common_size)
        return 1;

    /* extra checks */
    if (eaf_0->mpeg1 && (!eaf_1
            || eaf_0->mpeg1 != eaf_1->mpeg1
            || eaf_0->version != eaf_1->version
            || eaf_0->granule_index == eaf_1->granule_index
            || !eaf_0->common_size || !eaf_1->common_size)) {
        VGM_LOG("MPEG EAL3: EA-frames for MPEG1 don't match at 0x%lx\n", os->info_offset);
        goto fail;
    }


    /* get bitrate: use "free format" (bigger bitrate) to avoid the need of bit reservoir
     * this feature is in the spec but some decoders may not support it
     * (free format detection is broken in some MP3 in mpg123 < 1.25.8 but works ok) */
    expected_bitrate_index = 0x00;
    if (eaf_0->mpeg1) {
        expected_frame_size = 144l * (320*2) * 1000l / eaf_0->sample_rate;
    } else {
        expected_frame_size =  72l * (160*2) * 1000l / eaf_0->sample_rate;
    }
#if 0
    /* this uses max official bitrate (320/160) but some frames need more = complex bit reservoir */
    expected_bitrate_index = 0x0E;
    if (eaf_0->mpeg1) { /* 320: 44100=0x414, 48000=0x3C0, 32000=0x5A0 */
        expected_frame_size = 144l * 320 * 1000l / eaf_0->sample_rate;
    } else { /* 160: 22050=0x20A, 24000=0x1E0, 16000=0x2D0, 11025=0x414, 12000=0x3C0, 8000=0x5A0 */
        expected_frame_size =  72l * 160 * 1000l / eaf_0->sample_rate;
    }
#endif

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
        /* bit reservoir! shouldn't happen with free bitrate, otherwise it's hard to fix as needs complex buffering/calcs */
        VGM_LOG("MPEG EAL3: written 0x%lx but expected less than 0x%x at 0x%lx\n", os->b_off/8, expected_frame_size, os->info_offset);
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

/* write PCM block directly to sample buffer and setup decode discard (EALayer3 seems to use this as a prefetch of sorts).
 * Meant to be written inmediatedly, as those PCM are parts that can be found after 1 decoded frame.
 * (ex. EA-frame_gr0, PCM-frame_0, EA-frame_gr1, PCM-frame_1 actually writes PCM-frame_0+1, decode of EA-frame_gr0+1 + discard part */
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
            if (data->type == MPEG_EAL31) {
                if (decode_to_discard == 576)
                    decode_to_discard = data->samples_per_frame;//+ eaf->v1_pcm_number;
            }
            else {
                if (decode_to_discard == 0) /* seems ok? */
                    decode_to_discard += data->samples_per_frame;//+ eaf->v1_pcm_number;
                else if (decode_to_discard == 576) /* untested */
                    decode_to_discard = data->samples_per_frame;//+ eaf->v1_pcm_number;
            }
            ms->decode_to_discard += decode_to_discard;
        }
    }

    if (eaf->v2_extended_flag) {

        if (eaf->v2_pcm_number) {
            /* read + write PCM block samples (always BE) */
            for (i = 0; i < eaf->v2_pcm_number * data->channels_per_frame; i++) {
                off_t pcm_offset = stream->offset + eaf->pre_size + eaf->common_size + sizeof(sample)*i;
                int16_t pcm_sample = read_16bitBE(pcm_offset,stream->streamfile);
                put_16bitLE(ms->output_buffer + bytes_filled + sizeof(sample)*i, pcm_sample);
            }
            ms->samples_filled += eaf->v2_pcm_number;
        }

#if 0
        /* todo supposed skip modes (only seen 0x00):
         *
         * AB00CCCC CCCCCCCC  if A is set:  DDEEEEEE EEEEFFFF FFFFFFGG GGGGGGGG
         * D = BLOCKOFFSETMODE: IGNORE = 0x0, PRESERVE = 0x1, MUTE = 0x2, MAX = 0x3
         * E = samples to discard (mode == 0) or skip (mode == 1 or 2) before outputting the uncompressed samples
         *    (when mode == 3 this is ignored)
         * F = number of uncompressed sample frames (pcm block)
         * G = MPEG granule size (can be zero)
         *
         *   if 0: 576 - E if G == 0 then F
         *   if 1: 576 if G == 0 then F
         *   if 2: 576 if G == 0 then F * 2
         *   if 3: 576
         */

        /* modify decoded samples depending on flag */
        if (eaf->v2_mode == 0x00) {
            size_t decode_to_discard = eaf->v2_mode_value;

            if (decode_to_discard == 576)
                decode_to_discard = data->samples_per_frame;//+ eaf->v2_pcm_number;

            ms->decode_to_discard += decode_to_discard;
        }
#endif
    }

    return 1;
fail:
    return 0;
}


/* Skip EA-frames from other streams for multichannel (interleaved 1 EA-frame per stream).
 * Due to EALayer3 being in blocks and other complexities (we can't go past a block) all
 * streams's offsets should start in the first stream's EA-frame.
 *
 * So to properly read one MPEG-frame from a stream we need to:
 * - skip one EA-frame per previous streams until offset is in current stream's EA-frame
 *   (ie. 1st stream skips 0, 2nd stream skips 1, 3rd stream skips 2)
 * - read EA-frame (granule0)
 * - skip one EA-frame per following streams until offset is in first stream's EA-frame
 *   (ie. 1st stream skips 2, 2nd stream skips 1, 3rd stream skips 0)
 * - repeat again for granule1
 */
static int ealayer3_skip_data(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, int at_start) {
    int ok, i;
    ealayer3_frame_info eaf;
    vgm_bitstream is = {0};
    uint8_t ibuf[EALAYER3_EA_FRAME_BUFFER_SIZE];
    int skips = at_start ? num_stream : data->streams_size - 1 - num_stream;


    for (i = 0; i < skips; i++) {
        is.buf = ibuf;
        is.bufsize = read_streamfile(ibuf,stream->offset,EALAYER3_EA_FRAME_BUFFER_SIZE, stream->streamfile); /* reads less at EOF */
        is.b_off = 0;

        ok = ealayer3_parse_frame(data, &is, &eaf);
        if (!ok) goto fail;

        stream->offset += eaf.eaframe_size;
    }
    //;VGM_LOG("s%i: skipped %i frames, now at %lx\n", num_stream,skips,stream->offset);

    return 1;
fail:
    return 0;
}

#endif
