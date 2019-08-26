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
 * EALayer3 v1 and v2 differ in part of the header, but are mostly the same.
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
    uint32_t v1_offset_samples;
    uint32_t v1_pcm_samples;
    uint32_t v1_pcm_unknown;

    /* EALayer3 v2 header */
    uint32_t v2_extended_flag;
    uint32_t v2_stereo_flag;
    uint32_t v2_reserved;
    uint32_t v2_frame_size; /* full size including headers and pcm block */
    uint32_t v2_offset_mode; /* discard mode */
    uint32_t v2_offset_samples; /* use depends on mode */
    uint32_t v2_pcm_samples;
    uint32_t v2_common_size; /* granule size: common header+data size; can be zero */

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


static int ealayer3_parse_frame(mpeg_codec_data *data, int num_stream, vgm_bitstream *is, ealayer3_frame_info * eaf);
static int ealayer3_parse_frame_v1(vgm_bitstream *is, ealayer3_frame_info * eaf, int channels_per_frame, int is_v1b);
static int ealayer3_parse_frame_v2(vgm_bitstream *is, ealayer3_frame_info * eaf);
static int ealayer3_parse_frame_common(vgm_bitstream *is, ealayer3_frame_info * eaf);
static int ealayer3_rebuild_mpeg_frame(vgm_bitstream* is_0, ealayer3_frame_info* eaf_0, vgm_bitstream* is_1, ealayer3_frame_info* eaf_1, vgm_bitstream* os);
static int ealayer3_write_pcm_block(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, ealayer3_frame_info * eaf);
static int ealayer3_skip_data(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, int at_start);
static int ealayer3_is_empty_frame(vgm_bitstream *is);

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

        ok = ealayer3_parse_frame(data, -1, &is, &eaf);
        if (!ok) goto fail;
    }
    //;VGM_ASSERT(!eaf.mpeg1, "MPEG EAL3: mpeg2 found at 0x%lx\n", start_offset); /* rare [FIFA 08 (PS3) abk] */

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

        ok = ealayer3_parse_frame(data, num_stream, &is_0, &eaf_0);
        if (!ok) goto fail;

        ok = ealayer3_write_pcm_block(stream, data, num_stream, &eaf_0);
        if (!ok) goto fail;

        stream->offset += eaf_0.eaframe_size;
        //;VGM_LOG("s%i: get granule0 done at %lx (eaf_size=%x, common_size=%x)\n", num_stream,stream->offset, eaf_0.eaframe_size, eaf_0.common_size);

        if (!ealayer3_skip_data(stream, data, num_stream, 0))
            goto fail;
	}

	/* In EAL3 V2P sometimes there is a new SNS/SPS block between granules. Instead of trying to fix it here
	 * or in blocked layout (too complex/patchy), SNS/SPS uses a custom streamfile that simply removes all
	 * block headers, so this parser only sees raw EALayer3 data. It also discards samples, which confuses
	 * blocked layout calculations
	 *
	 * Similarly (as V2P decodes and writes 1 granule at a time) stream can end in granule0. Since mpg123
	 * decodes in pairs we detect and feed it a fake end granule1, to get the last granule0's 576 samples. */

    granule_found = 0;
    /* get second frame/granule (MPEG1 only) if first granule was found */
    while (eaf_0.common_size && eaf_0.mpeg1 && !granule_found) {
        //;VGM_LOG("s%i: get granule1 at %lx\n", num_stream,stream->offset);
        if (!ealayer3_skip_data(stream, data, num_stream, 1))
            goto fail;

        is_1.buf = ibuf_1;
        is_1.bufsize = read_streamfile(ibuf_1,stream->offset,EALAYER3_EA_FRAME_BUFFER_SIZE, stream->streamfile); /* reads less at EOF */
        is_1.b_off = 0;

        /* detect end granule0 (which may be before stream end in multichannel) and create a usable last granule1 */
        if (data->type == MPEG_EAL32P && eaf_0.mpeg1 && ealayer3_is_empty_frame(&is_1)) {
            ;VGM_LOG("EAL3: fake granule1 needed\n");
            /* memcpy/clone for now as I'm not sure now to create a valid empty granule1, but
             * probably doesn't matter since num_samples should end before its samples */
            eaf_1 = eaf_0;
            is_1 = is_0;
            eaf_1.granule_index = 1;
            break;
        }

        ok = ealayer3_parse_frame(data, num_stream, &is_1, &eaf_1);
        if (!ok) goto fail;

        ok = ealayer3_write_pcm_block(stream, data, num_stream, &eaf_1);
        if (!ok) goto fail;

        stream->offset += eaf_1.eaframe_size;
        //;VGM_LOG("s%i: get granule1 done at %lx (eaf_size=%x, common_size=%x)\n", num_stream,stream->offset, eaf_1.eaframe_size, eaf_1.common_size);

        if (!ealayer3_skip_data(stream, data, num_stream, 0))
            goto fail;

        /* in V1a there may be PCM-only frames between granules so read until next one (or parse fails) */
        if (eaf_1.common_size > 0)
            granule_found = 1;
    }

    /* rebuild EALayer3 frame to MPEG frame */
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

static int ealayer3_parse_frame(mpeg_codec_data *data, int num_stream, vgm_bitstream *is, ealayer3_frame_info * eaf) {
    int ok;

    /* We must pass this from state, as not all EA-frames have channel info.
     * (unknown in the first EA-frame but that's ok) */
    int channels_per_frame = 0;
    if (num_stream >= 0) {
        channels_per_frame = data->streams[num_stream]->channels_per_frame;
    }

    /* make sure as there is re-parsing in loops */
    memset(eaf, 0, sizeof(ealayer3_frame_info));

    switch(data->type) {
        case MPEG_EAL31:        ok = ealayer3_parse_frame_v1(is, eaf, channels_per_frame, 0); break;
        case MPEG_EAL31b:       ok = ealayer3_parse_frame_v1(is, eaf, channels_per_frame, 1); break;
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
        goto fail; /* must know from a prev frame (can't use eaf->channels for V1a) */
    }

    /* read EA-frame common header (V1a PCM blocks don't have EA-frames, while V1b do) */
    if (is_v1b || eaf->v1_pcm_flag == 0x00) {
        ok = ealayer3_parse_frame_common(is, eaf);
        if (!ok) goto fail;
    }

    /* check PCM block */
    if (eaf->v1_pcm_flag == 0xEE) {
        r_bits(is, 16,&eaf->v1_offset_samples); /* samples to discard of the next decoded (not PCM block) samples */
        r_bits(is, 16,&eaf->v1_pcm_samples); /* number of PCM samples, can be 0 */

        eaf->pre_size += 2+2; /* 16b+16b */
        eaf->pcm_size = (2*eaf->v1_pcm_samples * channels_per_frame);

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
    r_bits(is,  2,&eaf->v2_reserved);
    r_bits(is, 12,&eaf->v2_frame_size);

    eaf->pre_size = 2; /* 16b */

    if (eaf->v2_extended_flag) {
        r_bits(is,  2,&eaf->v2_offset_mode);
        r_bits(is, 10,&eaf->v2_offset_samples);
        r_bits(is, 10,&eaf->v2_pcm_samples);
        r_bits(is, 10,&eaf->v2_common_size);

        eaf->pre_size += 4; /* 32b */
    }

    /* read EA-frame common header */
    if (!eaf->v2_extended_flag || (eaf->v2_extended_flag && eaf->v2_common_size)) {
        ok = ealayer3_parse_frame_common(is, eaf);
        if (!ok) goto fail;
    }
    VGM_ASSERT(eaf->v2_extended_flag && eaf->v2_common_size == 0, "EA EAL3: v2 empty frame\n"); /* seen in V2S */
    VGM_ASSERT(eaf->v2_extended_flag && eaf->v2_offset_samples > 0, "EA EAL3: v2_offset_mode=%x with value=0x%x\n", eaf->v2_offset_mode, eaf->v2_offset_samples);
    //VGM_ASSERT(eaf->v2_pcm_samples > 0, "EA EAL3: v2_pcm_samples 0x%x\n", eaf->v2_pcm_samples);

    eaf->pcm_size = (2*eaf->v2_pcm_samples * eaf->channels);

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
        VGM_LOG("MPEG EAL3: EA-frames for MPEG1 don't match\n");
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
        VGM_LOG("MPEG EAL3: written 0x%x but expected less than 0x%x at 0x%x\n", (uint32_t)(os->b_off/8), expected_frame_size, (uint32_t)os->info_offset);
    }
    else {
        /* fill ancillary data (should be ignored, but 0x00 seems to improve mpg123's free bitrate detection) */
        memset(os->buf + os->b_off/8, 0x00, expected_frame_size - os->b_off/8);
    }

    os->b_off = expected_frame_size*8;


    return 1;
fail:
    return 0;
}

static void ealayer3_copy_pcm_block(uint8_t* outbuf, off_t pcm_offset, int pcm_number, int channels_per_frame, int is_packed, STREAMFILE * streamfile) {
    int i, ch;

    if (pcm_number == 0)
        return;

    /* read + write PCM block samples (always BE) */
    if (is_packed) {
        /* ch0+ch1 packed together */
        off_t put_offset = 0;
        for (i = 0; i < pcm_number * channels_per_frame; i++) {
            int16_t pcm_sample = read_16bitBE(pcm_offset,streamfile);
            put_16bitLE(outbuf + put_offset, pcm_sample);

            pcm_offset += sizeof(sample);
            put_offset += sizeof(sample);
        }
    }
    else {
        /* all of ch0 first, then all of ch1 (EAL3 v1b only) */
        for (ch = 0; ch < channels_per_frame; ch++) {
            off_t put_offset = sizeof(sample)*ch;
            for (i = 0; i < pcm_number; i++) {
                int16_t pcm_sample = read_16bitBE(pcm_offset,streamfile);
                put_16bitLE(outbuf + put_offset, pcm_sample);

                pcm_offset += sizeof(sample);
                put_offset += sizeof(sample)*channels_per_frame;
            }
        }
    }
}

/* write PCM block directly to sample buffer and setup decode discard (EALayer3 seems to use this as a prefetch of sorts).
 * Meant to be written inmediatedly, as those PCM are parts that can be found after 1 decoded frame.
 * (ex. EA-frame_gr0, PCM-frame_0, EA-frame_gr1, PCM-frame_1 actually writes PCM-frame_0+1, decode of EA-frame_gr0+1 + discard part */
static int ealayer3_write_pcm_block(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, ealayer3_frame_info * eaf) {
    mpeg_custom_stream *ms = data->streams[num_stream];
    int channels_per_frame = ms->channels_per_frame;
    size_t bytes_filled;


    bytes_filled = sizeof(sample) * ms->samples_filled * channels_per_frame;
    if (bytes_filled + eaf->pcm_size > ms->output_buffer_size) {
        VGM_LOG("MPEG EAL3: can't fill the sample buffer with 0x%x\n", eaf->pcm_size);
        goto fail;
    }

    if (eaf->v1_pcm_samples && !eaf->pcm_size) {
        VGM_LOG("MPEG EAL3: pcm_size without pcm_samples\n");
        goto fail;
    }

    if (eaf->v1_pcm_samples || eaf->v1_offset_samples) {
        uint8_t* outbuf = ms->output_buffer + bytes_filled;
        off_t pcm_offset = stream->offset + eaf->pre_size + eaf->common_size;

        VGM_ASSERT(eaf->v1_offset_samples > 576, "MPEG EAL3: big discard %i at 0x%x\n", eaf->v1_offset_samples, (uint32_t)stream->offset);
        VGM_ASSERT(eaf->v1_pcm_samples > 0x100, "MPEG EAL3: big samples %i at 0x%x\n", eaf->v1_pcm_samples, (uint32_t)stream->offset);
        VGM_ASSERT(eaf->v1_offset_samples > 0 && eaf->v1_pcm_samples == 0, "EAL3: offset_samples without pcm_samples\n"); /* not seen but could work */

        //;VGM_LOG("EA EAL3 v1: off=%lx, discard=%x, pcm=%i, pcm_o=%lx\n",
        //        stream->offset, eaf->v1_offset_samples, eaf->v1_pcm_samples, pcm_offset);

        /* V1 usually discards + copies samples at the same time
         * V1b PCM block is interleaved/'planar' format (ex. NFS:U PS3) */
        ealayer3_copy_pcm_block(outbuf, pcm_offset, eaf->v1_pcm_samples, channels_per_frame, (data->type == MPEG_EAL31), stream->streamfile);
        ms->samples_filled += eaf->v1_pcm_samples;

        /* skip decoded samples as PCM block 'overwrites' them w/ special meanings */
        {
            size_t decode_to_discard = eaf->v1_offset_samples;

            if (data->type == MPEG_EAL31) {
                //todo should also discard v1_pcm_samples, but block layout samples may be exhausted
                // and won't move (maybe new block if offset = new offset detected)
                if (decode_to_discard == 576)
                    decode_to_discard = data->samples_per_frame;//+ eaf->v1_pcm_samples;
            }
            else {
                VGM_ASSERT(decode_to_discard > 0, "EAL3: found offset_samples in V1b\n");
                /* probably (576 or samples_per_frame - eaf->v1_offset_samples) but V1b seems to always use 0 and ~47 samples */
                if (decode_to_discard == 0) /* seems ok (ex. comparing NFS:UC PS3 vs PC gets correct waveform this way) */
                    decode_to_discard = data->samples_per_frame;//+ eaf->v1_pcm_samples; /* musn't discard pcm_number */
            }

            ms->decode_to_discard += decode_to_discard;
        }
    }

    if (eaf->v2_extended_flag) {
        uint8_t* outbuf = ms->output_buffer + bytes_filled;
        off_t pcm_offset = stream->offset + eaf->pre_size + eaf->common_size;
        size_t usable_samples, decode_to_discard;

        /* V2P usually only copies big PCM, while V2S discards then copies few samples (similar to V1b).
         * Unlike V1b, both modes seem to use 'packed' PCM block */
        ealayer3_copy_pcm_block(outbuf, pcm_offset, eaf->v2_pcm_samples, channels_per_frame, 1, stream->streamfile);
        ms->samples_filled += eaf->v2_pcm_samples;

        //;VGM_LOG("EA EAL3 v2: off=%lx, mode=%x, value=%i, pcm=%i, c-size=%x, pcm_o=%lx\n",
        //        stream->offset, eaf->v2_offset_mode, eaf->v2_offset_samples, eaf->v2_pcm_samples, eaf->v2_common_size, pcm_offset);

        //todo improve how discarding works since there could exists a subtle-but-unlikely PCM+granule usage
        //todo test other modes (only seen IGNORE)

        /* get how many samples we can use in this granule + pcm block (thus how many we can't) */
        if (eaf->v2_offset_mode == 0x00) { /* IGNORE (looks correct in V2P loops, ex. NFS:W) */
            /* offset_samples is usually 0 in V2P (no discard), varies in V2S and may be 576 (full discard).
             * If block has pcm_samples then usable_samples will be at least that value (for all known files),
             * and is assumed PCM isn't discarded so only discards the decoded granule. */
            usable_samples = 576 - eaf->v2_offset_samples;
            if (eaf->common_size == 0)
                usable_samples = eaf->v2_pcm_samples;

            if (usable_samples == eaf->v2_pcm_samples) {
                decode_to_discard = 576;
            }
            else {
                VGM_LOG("EAL3: unknown discard\n");
                decode_to_discard = 0;
            }
        }
        else if (eaf->v2_offset_mode == 0x01)  { /* PRESERVE */
            usable_samples = 576;
            if (eaf->common_size == 0)
                usable_samples = eaf->v2_pcm_samples;
            decode_to_discard = 0; /* all preserved */
        }
        else if (eaf->v2_offset_mode == 0x02)  { /* MUTE */
            usable_samples = 576;
            if (eaf->common_size == 0)
                usable_samples = eaf->v2_pcm_samples * 2; // why 2?
            decode_to_discard = 0; /* not discarded but muted */
            //mute_samples = eaf->v2_offset_samples; //todo must 0 first N decoded samples
        }
        else {
            VGM_LOG("EAL3: unknown mode\n"); /* not defined */
            usable_samples = 576;
            decode_to_discard = 0;
        }

        ms->decode_to_discard += decode_to_discard;
    }

    return 1;
fail:
    return 0;
}


/* Skip EA-frames from other streams for .sns/sps multichannel (interleaved 1 EA-frame per stream).
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
 *
 * EALayer3 v1 in SCHl uses external offsets and 1ch multichannel instead.
 */
static int ealayer3_skip_data(VGMSTREAMCHANNEL *stream, mpeg_codec_data *data, int num_stream, int at_start) {
    int ok, i;
    ealayer3_frame_info eaf;
    vgm_bitstream is = {0};
    uint8_t ibuf[EALAYER3_EA_FRAME_BUFFER_SIZE];
    int skips = at_start ? num_stream : data->streams_size - 1 - num_stream;

    /* v1 does multichannel with set offsets */
    if (data->type == MPEG_EAL31)
        return 1;

    for (i = 0; i < skips; i++) {
        is.buf = ibuf;
        is.bufsize = read_streamfile(ibuf,stream->offset,EALAYER3_EA_FRAME_BUFFER_SIZE, stream->streamfile); /* reads less at EOF */
        is.b_off = 0;

        ok = ealayer3_parse_frame(data, num_stream, &is, &eaf);
        if (!ok) goto fail;

        stream->offset += eaf.eaframe_size;
        //;VGM_LOG("s%i-%i: skipping %x, now at %lx\n", num_stream,i,eaf.eaframe_size,stream->offset);
    }
    //;VGM_LOG("s%i: skipped %i frames, now at %lx\n", num_stream,skips,stream->offset);

    return 1;
fail:
    return 0;
}

static int ealayer3_is_empty_frame(vgm_bitstream *is) {
    int i;

    for (i = 0; i < is->bufsize; i++) {
        if (is->buf[i] != 0)
            return 0;
    }

    return 1;
}

#endif
