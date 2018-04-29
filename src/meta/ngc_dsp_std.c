#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util.h"
#include "../stack_alloc.h"

/* If these variables are packed properly in the struct (one after another)
 * then this is actually how they are laid out in the file, albeit big-endian */

struct dsp_header {
    uint32_t sample_count;
    uint32_t nibble_count;
    uint32_t sample_rate;
    uint16_t loop_flag;
    uint16_t format;
    uint32_t loop_start_offset;
    uint32_t loop_end_offset;
    uint32_t ca;
    int16_t coef[16]; /* really 8x2 */
    uint16_t gain;
    uint16_t initial_ps;
    int16_t initial_hist1;
    int16_t initial_hist2;
    uint16_t loop_ps;
    int16_t loop_hist1;
    int16_t loop_hist2;
    int16_t channel_count; /* DSPADPCM.exe ~v2.7 extension */
    int16_t block_size;
    /* padding/reserved up to 0x60 */
    /* DSPADPCM.exe from GC adds some extra data here (uninitialized MSVC memory?) */
};

/* read the above struct; returns nonzero on failure */
static int read_dsp_header_endian(struct dsp_header *header, off_t offset, STREAMFILE *streamFile, int big_endian) {
    int32_t (*get_32bit)(uint8_t *) = big_endian ? get_32bitBE : get_32bitLE;
    int16_t (*get_16bit)(uint8_t *) = big_endian ? get_16bitBE : get_16bitLE;
    int i;
    uint8_t buf[0x4e];

    if (read_streamfile(buf, offset, 0x4e, streamFile) != 0x4e)
        return 1;
    header->sample_count =      get_32bit(buf+0x00);
    header->nibble_count =      get_32bit(buf+0x04);
    header->sample_rate =       get_32bit(buf+0x08);
    header->loop_flag =         get_16bit(buf+0x0c);
    header->format =            get_16bit(buf+0x0e);
    header->loop_start_offset = get_32bit(buf+0x10);
    header->loop_end_offset =   get_32bit(buf+0x14);
    header->ca =                get_32bit(buf+0x18);
    for (i=0; i < 16; i++)
        header->coef[i] =       get_16bit(buf+0x1c+i*0x02);
    header->gain =              get_16bit(buf+0x3c);
    header->initial_ps =        get_16bit(buf+0x3e);
    header->initial_hist1 =     get_16bit(buf+0x40);
    header->initial_hist2 =     get_16bit(buf+0x42);
    header->loop_ps =           get_16bit(buf+0x44);
    header->loop_hist1 =        get_16bit(buf+0x46);
    header->loop_hist2 =        get_16bit(buf+0x48);
    header->channel_count =     get_16bit(buf+0x4a);
    header->block_size =        get_16bit(buf+0x4c);
    return 0;
}
static int read_dsp_header(struct dsp_header *header, off_t offset, STREAMFILE *file) {
    return read_dsp_header_endian(header, offset, file, 1);
}
static int read_dsp_header_le(struct dsp_header *header, off_t offset, STREAMFILE *file) {
    return read_dsp_header_endian(header, offset, file, 0);
}


static void setup_vgmstream_dsp(VGMSTREAM* vgmstream, struct dsp_header* ch_header) {
    int i, j;

    /* set coeffs and initial history (usually 0) */
    for (i = 0; i < vgmstream->channels; i++){
        for (j = 0; j < 16; j++) {
            vgmstream->ch[i].adpcm_coef[j] = ch_header[i].coef[j];
        }
        vgmstream->ch[i].adpcm_history1_16 = ch_header[i].initial_hist1;
        vgmstream->ch[i].adpcm_history2_16 = ch_header[i].initial_hist2;
    }
}

static int dsp_load_header_endian(struct dsp_header* ch_header, int channels, STREAMFILE *streamFile, off_t offset, size_t spacing, int big_endian) {
    int i;

    /* load standard dsp header per channel */
    for (i = 0; i < channels; i++) {
        if (read_dsp_header_endian(&ch_header[i], offset + i*spacing, streamFile, big_endian))
            goto fail;
    }

    return 1;
fail:
    return 0;
}
static int dsp_load_header(struct dsp_header* ch_header, int channels, STREAMFILE *streamFile, off_t offset, size_t spacing) {
    return dsp_load_header_endian(ch_header, channels, streamFile, offset, spacing, 1);
}
//static int dsp_load_header_le(struct dsp_header* ch_header, int channels, STREAMFILE *streamFile, off_t offset, size_t spacing) {
//    return dsp_load_header_endian(ch_header, channels, streamFile, offset, spacing, 0);
//}
static int check_dsp_format(struct dsp_header* ch_header, int channels) {
    int i;

    /* check type==0 and gain==0 */
    for (i = 0; i < channels; i++) {
        if (ch_header[i].format || ch_header[i].gain)
            goto fail;
    }

    return 1;
fail:
    return 0;
}
static int check_dsp_samples(struct dsp_header* ch_header, int channels) {
    int i;

    /* check for agreement between channels */
    for (i = 0; i < channels - 1; i++) {
        if (ch_header[i].sample_count != ch_header[i+1].sample_count ||
            ch_header[i].nibble_count != ch_header[i+1].nibble_count ||
            ch_header[i].sample_rate != ch_header[i+1].sample_rate ||
            ch_header[i].loop_flag != ch_header[i+1].loop_flag ||
            ch_header[i].loop_start_offset != ch_header[i+1].loop_start_offset ||
            ch_header[i].loop_end_offset != ch_header[i+1].loop_end_offset ) {
            goto fail;
        }
    }

    return 1;
fail:
    return 0;
}
static int check_dsp_initial_ps(struct dsp_header* ch_header, int channels, STREAMFILE *streamFile, off_t offset, size_t interleave) {
    int i;

    /* check initial predictor/scale */
    for (i = 0; i < channels; i++) {
        off_t start_offset = offset + i*interleave;
        if (ch_header[i].initial_ps != (uint8_t)read_8bit(start_offset, streamFile)){
            goto fail;
        }
    }

    return 1;
fail:
    return 0;
}
static int check_dsp_loop_ps(struct dsp_header* ch_header, int channels, STREAMFILE *streamFile, off_t offset, size_t interleave) {
    int i;

    if (!ch_header[0].loop_flag)
        return 1;

    /* check loop predictor/scale */
    for (i = 0; i < channels; i++) {
        off_t loop_offset = ch_header[i].loop_start_offset;
        if (interleave) {
            loop_offset = loop_offset / 16 * 8;
            loop_offset = (loop_offset / interleave * interleave * channels) + (loop_offset % interleave);
        }

        if (ch_header[i].loop_ps != (uint8_t)read_8bit(offset + i*interleave + loop_offset,streamFile))
            goto fail;
    }

    return 1;
fail:
    return 0;
}

/* ********************************* */

/* common parser config as most DSPs are basically the same with minor changes */
typedef struct {
    int little_endian;
    int channel_count;
    int max_channels;

    int force_loop; /* force full loop */
    int fix_looping; /* fix loop end going past num_samples */
    int fix_loop_start; /* weird files with bad loop start */
    int single_header; /* all channels share header, thus totals are off */
    int ignore_header_agreement; /* sometimes there are minor differences between headers */
    int ignore_loop_check; /* loop info in header should match data, but sometimes it's weird */ //todo check if needed anymore

    off_t header_offset;
    size_t header_spacing;
    off_t start_offset;
    size_t interleave;

    meta_t meta_type;
} dsp_meta;

#define COMMON_DSP_MAX_CHANNELS 6
static VGMSTREAM * init_vgmstream_dsp_common(STREAMFILE *streamFile, dsp_meta *dspm) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag;
    struct dsp_header ch_header[COMMON_DSP_MAX_CHANNELS];

    if (dspm->channel_count > dspm->max_channels)
        goto fail;
    if (dspm->channel_count > COMMON_DSP_MAX_CHANNELS)
        goto fail;


    /* read dsp */
    if (!dsp_load_header_endian(ch_header, dspm->channel_count, streamFile,dspm->header_offset,dspm->header_spacing, !dspm->little_endian))
        goto fail;

    if (dspm->fix_loop_start) {
        int i;
        for (i = 0; i < dspm->channel_count; i++) {
            /* bad/fixed value in loop start */
            if (ch_header[i].loop_flag)
                ch_header[i].loop_start_offset = 0x00;
        }
    }

    if (!check_dsp_format(ch_header, dspm->channel_count))
        goto fail;

    if (!dspm->ignore_header_agreement && !check_dsp_samples(ch_header, dspm->channel_count))
        goto fail;

    if (dspm->single_header && !check_dsp_initial_ps(ch_header, 1, streamFile,dspm->start_offset,dspm->interleave))
        goto fail;
    if (!dspm->single_header && !check_dsp_initial_ps(ch_header, dspm->channel_count, streamFile,dspm->start_offset,dspm->interleave))
        goto fail;

    if (!dspm->ignore_loop_check) {
        if (dspm->single_header && !check_dsp_loop_ps(ch_header, 1, streamFile,dspm->start_offset,dspm->interleave))
            goto fail;
        if (!dspm->single_header && !check_dsp_loop_ps(ch_header, dspm->channel_count, streamFile,dspm->start_offset,dspm->interleave))
            goto fail;
    }


    loop_flag = ch_header[0].loop_flag;
    if (dspm->force_loop)
        loop_flag = 1;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(dspm->channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = dspm->meta_type;
    vgmstream->coding_type = coding_NGC_DSP;
    if (dspm->interleave > 0 && dspm->interleave < 0x08)
        vgmstream->coding_type = coding_NGC_DSP_subint;
    vgmstream->layout_type = layout_interleave;
    if (dspm->interleave == 0 || vgmstream->coding_type == coding_NGC_DSP_subint)
        vgmstream->layout_type = layout_none;
    vgmstream->interleave_block_size = dspm->interleave;

    setup_vgmstream_dsp(vgmstream, ch_header);

    /* don't know why, but it does happen*/
    if (dspm->fix_looping && vgmstream->loop_end_sample > vgmstream->num_samples)
        vgmstream->loop_end_sample = vgmstream->num_samples;

    if (dspm->single_header) {
        vgmstream->num_samples /= dspm->channel_count;
        vgmstream->loop_start_sample /= dspm->channel_count;
        vgmstream->loop_end_sample /= dspm->channel_count;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,dspm->start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ********************************* */

/* .dsp - standard dsp as generated by DSPADPCM.exe */
VGMSTREAM * init_vgmstream_ngc_dsp_std(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    struct dsp_header header;
    const size_t header_size = 0x60;
    off_t start_offset;
    int i, channel_count;

    /* checks */
    /* .dsp: standard, .adp: Dr. Muto/Battalion Wars (GC) mono files */
    if (!check_extensions(streamFile, "dsp,adp"))
        goto fail;

    if (read_dsp_header(&header, 0x00, streamFile))
        goto fail;

    channel_count = 1;
    start_offset = header_size;

    if (header.initial_ps != (uint8_t)read_8bit(start_offset,streamFile))
        goto fail; /* check initial predictor/scale */
    if (header.format || header.gain)
        goto fail; /* check type==0 and gain==0 */

    /* Check for a matching second header. If we find one and it checks
     * out thoroughly, we're probably not dealing with a genuine mono DSP.
     * In many cases these will pass all the other checks, including the
     * predictor/scale check if the first byte is 0 */
    {
        struct dsp_header header2;
        read_dsp_header(&header2, header_size, streamFile);

        if (header.sample_count == header2.sample_count &&
            header.nibble_count == header2.nibble_count &&
            header.sample_rate == header2.sample_rate &&
            header.loop_flag == header2.loop_flag) {
            goto fail;
        }
    }
        
    if (header.loop_flag) {
        off_t loop_off;
        /* check loop predictor/scale */
        loop_off = header.loop_start_offset/16*8;
        if (header.loop_ps != (uint8_t)read_8bit(start_offset+loop_off,streamFile)) {
            /* rarely won't match (ex ESPN 2002), not sure if header or calc problem, but doesn't seem to matter
             *  (there may be a "click" when looping, or loop values may be too big and loop disabled anyway) */
            VGM_LOG("DSP (std): bad loop_predictor\n");
            //header.loop_flag = 0;
            //goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,header.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = header.sample_rate;
    vgmstream->num_samples = header.sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(header.loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(header.loop_end_offset)+1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* don't know why, but it does happen */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_STD;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;

    {
        /* adpcm coeffs/history */
        for (i = 0; i < 16; i++)
            vgmstream->ch[0].adpcm_coef[i] = header.coef[i];
        vgmstream->ch[0].adpcm_history1_16 = header.initial_hist1;
        vgmstream->ch[0].adpcm_history2_16 = header.initial_hist2;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .dsp - little endian dsp, possibly main Switch .dsp [LEGO Worlds (Switch)] */
VGMSTREAM * init_vgmstream_ngc_dsp_std_le(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    struct dsp_header header;
    const size_t header_size = 0x60;
    off_t start_offset;
    int i, channel_count;

    /* checks */
    /* .adpcm: LEGO Worlds */
    if (!check_extensions(streamFile, "adpcm"))
        goto fail;

    if (read_dsp_header_le(&header, 0x00, streamFile))
        goto fail;

    channel_count = 1;
    start_offset = header_size;

    if (header.initial_ps != (uint8_t)read_8bit(start_offset,streamFile))
        goto fail; /* check initial predictor/scale */
    if (header.format || header.gain)
        goto fail; /* check type==0 and gain==0 */

    /* Check for a matching second header. If we find one and it checks
     * out thoroughly, we're probably not dealing with a genuine mono DSP.
     * In many cases these will pass all the other checks, including the
     * predictor/scale check if the first byte is 0 */
    {
        struct dsp_header header2;
        read_dsp_header_le(&header2, header_size, streamFile);

        if (header.sample_count == header2.sample_count &&
            header.nibble_count == header2.nibble_count &&
            header.sample_rate == header2.sample_rate &&
            header.loop_flag == header2.loop_flag) {
            goto fail;
        }
    }

    if (header.loop_flag) {
        off_t loop_off;
        /* check loop predictor/scale */
        loop_off = header.loop_start_offset/16*8;
        if (header.loop_ps != (uint8_t)read_8bit(start_offset+loop_off,streamFile)) {
            /* rarely won't match (ex ESPN 2002), not sure if header or calc problem, but doesn't seem to matter
             *  (there may be a "click" when looping, or loop values may be too big and loop disabled anyway) */
            VGM_LOG("DSP (std): bad loop_predictor\n");
            //header.loop_flag = 0;
            //goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,header.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = header.sample_rate;
    vgmstream->num_samples = header.sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(header.loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(header.loop_end_offset)+1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* don't know why, but it does happen */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_STD;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;

    {
        /* adpcm coeffs/history */
        for (i = 0; i < 16; i++)
            vgmstream->ch[0].adpcm_coef[i] = header.coef[i];
        vgmstream->ch[0].adpcm_history1_16 = header.initial_hist1;
        vgmstream->ch[0].adpcm_history2_16 = header.initial_hist2;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .dsp - standard multi-channel dsp as generated by DSPADPCM.exe (later revisions) */
VGMSTREAM * init_vgmstream_ngc_mdsp_std(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    struct dsp_header header;
    const size_t header_size = 0x60;
    off_t start_offset;
    int i, c, channel_count;

    /* checks */
    if (!check_extensions(streamFile, "dsp,mdsp"))
        goto fail;

    if (read_dsp_header(&header, 0x00, streamFile))
        goto fail;

    channel_count = header.channel_count==0 ? 1 : header.channel_count;
    start_offset = header_size * channel_count;

    /* named .dsp and no channels? likely another interleaved dsp */
    if (check_extensions(streamFile,"dsp") && header.channel_count == 0)
        goto fail;

    if (header.initial_ps != (uint8_t)read_8bit(start_offset, streamFile))
        goto fail; /* check initial predictor/scale */
    if (header.format || header.gain)
        goto fail; /* check type==0 and gain==0 */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, header.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = header.sample_rate;
    vgmstream->num_samples = header.sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(header.loop_start_offset);
    vgmstream->loop_end_sample = dsp_nibbles_to_samples(header.loop_end_offset) + 1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* don't know why, but it does happen*/
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_STD;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = channel_count == 1 ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = header.block_size * 8;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (header.nibble_count / 2 % vgmstream->interleave_block_size + 7) / 8 * 8;

    for (i = 0; i < channel_count; i++) {
        if (read_dsp_header(&header, header_size * i, streamFile)) goto fail;

        /* adpcm coeffs/history */
        for (c = 0; c < 16; c++)
            vgmstream->ch[i].adpcm_coef[c] = header.coef[c];
        vgmstream->ch[i].adpcm_history1_16 = header.initial_hist1;
        vgmstream->ch[i].adpcm_history2_16 = header.initial_hist2;
    }

    if (!vgmstream_open_stream(vgmstream, streamFile, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* Some very simple stereo variants of standard dsp just use the standard header
 * twice and add interleave, or just concatenate the channels. We'll support them all here. */

/* .stm - Intelligent Systems + others (same programmers) full interleaved dsp [Paper Mario TTYD (GC), Fire Emblem: POR (GC), Cubivore (GC)] */
VGMSTREAM * init_vgmstream_ngc_dsp_stm(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    /* .lstm/dsp: renamed to avoid hijacking Scream Tracker 2 Modules */
    if (!check_extensions(streamFile, "stm,lstm,dsp"))
        goto fail;
    if (read_16bitBE(0x00, streamFile) != 0x0200)
        goto fail;
    /* 0x02(2): sample rate, 0x08+: channel sizes/loop offsets? */

    dspm.channel_count = read_32bitBE(0x04, streamFile);
    dspm.max_channels = 2;
    dspm.fix_looping = 1;

    dspm.header_offset =  0x40;
    dspm.header_spacing = 0x60;
    dspm.start_offset = 0x100;
    dspm.interleave = (read_32bitBE(0x08, streamFile) + 0x20) / 0x20 * 0x20; /* strange rounding, but works */

    dspm.meta_type = meta_DSP_STM;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .(mp)dsp - single header + interleaved dsp [Monopoly Party! (GC)] */
VGMSTREAM * init_vgmstream_ngc_mpdsp(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    /* .mpdsp: renamed since standard .dsp would catch it otherwise */
    if (!check_extensions(streamFile, "mpdsp"))
        goto fail;

    /* at 0x48 is extra data that could help differenciating these DSPs, but other games
     * put similar stuff there, needs more checks (ex. Battallion Wars, Army Men) */
    //0x00005300 60A94000 64FF1200 00000000 00000000 00000000
    /* 0x02(2): sample rate, 0x08+: channel sizes/loop offsets? */

    dspm.channel_count = 2;
    dspm.max_channels = 2;
    dspm.single_header = 1;

    dspm.header_offset =  0x00;
    dspm.header_spacing = 0x00; /* same header for both channels */
    dspm.start_offset = 0x60;
    dspm.interleave = 0xf000;

    dspm.meta_type = meta_DSP_MPDSP;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* various dsp with differing extensions and interleave values */
VGMSTREAM * init_vgmstream_ngc_dsp_std_int(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};
    char filename[PATH_LIMIT];

    /* checks */
    if (!check_extensions(streamFile, "dsp,mss,gcm"))
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;
    dspm.fix_looping = 1;

    dspm.header_offset  = 0x00;
    dspm.header_spacing = 0x60;
    dspm.start_offset = 0xc0;

    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strlen(filename) > 7 && !strcasecmp("_lr.dsp",filename+strlen(filename)-7)) {
        dspm.interleave = 0x14180;
        dspm.meta_type = meta_DSP_JETTERS; /* Bomberman Jetters (GC) */
    } else if (!strcasecmp("mss",filename_extension(filename))) {
        dspm.interleave = 0x1000;
        dspm.meta_type = meta_DSP_MSS; /* Free Radical GC games */
        /* Timesplitters 2 GC's ts2_atom_smasher_44_fx.mss differs slightly in samples but plays ok */
        dspm.ignore_header_agreement = 1;
    } else if (!strcasecmp("gcm",filename_extension(filename))) {
        dspm.interleave = 0x8000;
        dspm.meta_type = meta_DSP_GCM; /* some of Traveller's Tales games */
    } else {
        goto fail;
    }


    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* IDSP - Namco header + interleaved dsp [SSB4 (3DS), Tekken Tag Tournament 2 (WiiU)] */
VGMSTREAM * init_vgmstream_3ds_idsp(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};
    off_t offset;

    /* checks */
    if (!check_extensions(streamFile, "idsp,nus3bank"))
        goto fail;

    /* try NUS3BANK container */
    if (read_32bitBE(0x00,streamFile) == 0x4E555333) { /* "NUS3" */
        offset  = 0x14 + read_32bitLE(0x10, streamFile); /* header size */
        offset += read_32bitLE(0x1C, streamFile) + 0x08;
        offset += read_32bitLE(0x24, streamFile) + 0x08;
        offset += read_32bitLE(0x2C, streamFile) + 0x08;
        offset += read_32bitLE(0x34, streamFile) + 0x08;
        offset += read_32bitLE(0x3C, streamFile) + 0x08;
        offset += read_32bitLE(0x44, streamFile) + 0x08;
        offset += 0x08;
    }
    else {
        offset = 0x00;
    }

    if (read_32bitBE(offset,streamFile) != 0x49445350) /* "IDSP" */
        goto fail;
    /* 0x0c: sample rate, 0x10: num_samples, 0x14: loop_start_sample, 0x18: loop_start_sample */

    dspm.channel_count = read_32bitBE(offset+0x08, streamFile);
    dspm.max_channels = 8;
    /* games do adjust loop_end if bigger than num_samples (only happens in user-created IDSPs) */
    dspm.fix_looping = 1;

    dspm.header_offset = read_32bitBE(offset+0x20,streamFile) + offset;
    dspm.header_spacing = read_32bitBE(offset+0x24,streamFile);
    dspm.start_offset = read_32bitBE(offset+0x28,streamFile) + offset;
    dspm.interleave = read_32bitBE(offset+0x1c,streamFile); /* usually 0x10 */
    if (dspm.interleave == 0) /* Taiko no Tatsujin: Atsumete Tomodachi Daisakusen (WiiU) */
        dspm.interleave = read_32bitBE(offset+0x2c,streamFile); /* half interleave, use channel size */

    dspm.meta_type = meta_3DS_IDSP;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* sadb - Procyon Studio header + interleaved dsp [Shiren the Wanderer 3 (Wii), Disaster: Day of Crisis (Wii)] */
VGMSTREAM * init_vgmstream_sadb(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "sad"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x73616462) /* "sadb" */
        goto fail;

    dspm.channel_count = read_8bit(0x32, streamFile);
    dspm.max_channels = 2;

    dspm.header_offset =  0x80;
    dspm.header_spacing = 0x60;
    dspm.start_offset = read_32bitBE(0x48,streamFile);
    dspm.interleave = 0x10;

    dspm.meta_type = meta_DSP_SADB;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

#define WSI_MAX_CHANNELS 2
/* .wsi - blocked dsp [Alone in the Dark (Wii)] */
VGMSTREAM * init_vgmstream_wsi(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing;
    struct dsp_header ch_header[WSI_MAX_CHANNELS];
    int channel_count;

    /* checks */
    if (!check_extensions(streamFile, "wsi"))
        goto fail;

    /* I don't know if this is actually the channel count, or a block type
     * for the first block. Won't know until I see a mono .wsi */
    channel_count = read_32bitBE(0x04,streamFile);
    if (channel_count != 2) goto fail;

    /* check for consistent block headers */
    {
        off_t block_offset;
        off_t block_size_has_been;
        int i;
       
        block_offset = read_32bitBE(0x00,streamFile);
        if (block_offset < 0x08) goto fail;

        block_size_has_been = block_offset;

        /* check 4 blocks, to get an idea */
        for (i = 0; i < 4*channel_count; i++) {
            off_t block_size = read_32bitBE(block_offset,streamFile);

            if (block_size < 0x10)
                goto fail; /* expect at least the block header */
            if (i%channel_count+1 != read_32bitBE(block_offset+0x08,streamFile))
                goto fail; /* expect the channel numbers to alternate */

            if (i%channel_count==0)
                block_size_has_been = block_size;
            else if (block_size != block_size_has_been)
                goto fail; /* expect every block in a set of channels to have the same size */

            block_offset += block_size;
        }
    }

    start_offset = read_32bitBE(0x00, streamFile);
    header_offset = start_offset + 0x10;
    header_spacing = read_32bitBE(start_offset,streamFile);

    /* read dsp */
    if (!dsp_load_header(ch_header, channel_count, streamFile,header_offset,header_spacing)) goto fail;
    if (!check_dsp_format(ch_header, channel_count)) goto fail;
    if (!check_dsp_samples(ch_header, channel_count)) goto fail;
    //if (!check_dsp_initial_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;
    //if (!check_dsp_loop_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_header[0].loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ch_header[0].sample_rate;

    vgmstream->num_samples = ch_header[0].sample_count / 14 * 14; /* remove incomplete last frame */
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* don't know why, but it does happen*/
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_WSI;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_blocked_wsi;

    setup_vgmstream_dsp(vgmstream, ch_header);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    block_update_wsi(start_offset,vgmstream);

    /* first block has DSP header */
    {
        int i;

        vgmstream->current_block_size -= 0x60;
        for (i = 0; i < vgmstream->channels; i++) {
            vgmstream->ch[i].offset += 0x60;
        }
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* SWD - PSF chunks + interleaved dsps [Conflict: Desert Storm 1 & 2] */
VGMSTREAM * init_vgmstream_ngc_swd(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "swd"))
        goto fail;

    //todo blocked layout when first chunk is 0x50534631 (count + table of 0x0c with offset/sizes)

    if (read_32bitBE(0x00,streamFile) != 0x505346d1) /* PSF\0xd1 */
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;

    dspm.header_offset = 0x08;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + 0x60 * dspm.channel_count;
    dspm.interleave = 0x08;

    dspm.meta_type = meta_NGC_SWD;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* IDSP .gcm files, two standard DSP headers */
/* found in:  Lego Batman (Wii)
              Lego Indiana Jones - The Original Adventures (Wii)
              Lego Indiana Jones 2 - The Adventure Continues (Wii)
              Lego Star Wars - The Complete Saga (Wii)
              Lego The Lord of the Rings (Wii)
              The Chronicles of Narnia - Prince Caspian (Wii) */
VGMSTREAM * init_vgmstream_wii_idsp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    off_t interleave;
    struct dsp_header ch0_header,ch1_header;
    int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if ((strcasecmp("gcm",filename_extension(filename))) &&
                (strcasecmp("idsp",filename_extension(filename))))
    goto fail;

    /* check header magic */
    if (read_32bitBE(0x0,streamFile) != 0x49445350) goto fail; /* "IDSP" */

    /* different versions? */
    if (read_32bitBE(0x4, streamFile) == 1 &&
            read_32bitBE(0x8, streamFile) == 0xc8)
    {
        if (read_dsp_header(&ch0_header, 0x10, streamFile)) goto fail;
        if (read_dsp_header(&ch1_header, 0x70, streamFile)) goto fail;

        start_offset = 0xd0;
    }
    else if (read_32bitBE(0x4, streamFile) == 2 &&
            read_32bitBE(0x8, streamFile) == 0xd2)
    {
        if (read_dsp_header(&ch0_header, 0x20, streamFile)) goto fail;
        if (read_dsp_header(&ch1_header, 0x80, streamFile)) goto fail;

        start_offset = 0xe0;
    }
    else if (read_32bitBE(0x4, streamFile) == 3 && //Lego The Lord of the Rings (Wii)
        read_32bitBE(0x8, streamFile) == 0x12c)
    {
        if (read_dsp_header(&ch0_header, 0x20, streamFile)) goto fail;
        if (read_dsp_header(&ch1_header, 0x80, streamFile)) goto fail;

        start_offset = 0xe0;
    }
    else goto fail;

    interleave = read_32bitBE(0xc, streamFile);

    /* check initial predictor/scale */
    if (ch0_header.initial_ps != (uint8_t)read_8bit(start_offset,streamFile))
        goto fail;
    if (ch1_header.initial_ps != (uint8_t)read_8bit(start_offset+interleave,streamFile))
        goto fail;

    /* check type==0 and gain==0 */
    if (ch0_header.format || ch0_header.gain ||
        ch1_header.format || ch1_header.gain)
        goto fail;

    /* check for agreement */
    if (
            ch0_header.sample_count != ch1_header.sample_count ||
            ch0_header.nibble_count != ch1_header.nibble_count ||
            ch0_header.sample_rate != ch1_header.sample_rate ||
            ch0_header.loop_flag != ch1_header.loop_flag ||
            ch0_header.loop_start_offset != ch1_header.loop_start_offset ||
            ch0_header.loop_end_offset != ch1_header.loop_end_offset
       ) goto fail;

    if (ch0_header.loop_flag) {
        off_t loop_off;
        /* check loop predictor/scale */
        loop_off = ch0_header.loop_start_offset/16*8;
        loop_off = (loop_off/interleave*interleave*2) + (loop_off%interleave);
        if (ch0_header.loop_ps != (uint8_t)read_8bit(start_offset+loop_off,streamFile))
            goto fail;
        if (ch1_header.loop_ps != (uint8_t)read_8bit(start_offset+loop_off+interleave,streamFile))
            goto fail;
    }

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(2,ch0_header.loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = ch0_header.sample_count;
    vgmstream->sample_rate = ch0_header.sample_rate;

    /* TODO: adjust for interleave? */
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch0_header.loop_start_offset);
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(ch0_header.loop_end_offset)+1;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_DSP_WII_IDSP;

    /* coeffs */
    for (i=0;i<16;i++) {
        vgmstream->ch[0].adpcm_coef[i] = ch0_header.coef[i];
        vgmstream->ch[1].adpcm_coef[i] = ch1_header.coef[i];
    }
    
    /* initial history */
    /* always 0 that I've ever seen, but for completeness... */
    vgmstream->ch[0].adpcm_history1_16 = ch0_header.initial_hist1;
    vgmstream->ch[0].adpcm_history2_16 = ch0_header.initial_hist2;
    vgmstream->ch[1].adpcm_history1_16 = ch1_header.initial_hist1;
    vgmstream->ch[1].adpcm_history2_16 = ch1_header.initial_hist2;

    vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    vgmstream->ch[1].streamfile = vgmstream->ch[0].streamfile;

    if (!vgmstream->ch[0].streamfile) goto fail;
    /* open the file for reading */
    for (i=0;i<2;i++) {
        vgmstream->ch[i].channel_start_offset=
            vgmstream->ch[i].offset=start_offset+i*interleave;
    }

    return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* .wsd - Custom header + full interleaved dsp [Phantom Brave (Wii)] */
VGMSTREAM * init_vgmstream_wii_wsd(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "wsd"))
        goto fail;
    if (read_32bitBE(0x08,streamFile) != read_32bitBE(0x0c,streamFile)) /* channel sizes */
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;

    dspm.header_offset =  read_32bitBE(0x00,streamFile);
    dspm.header_spacing = read_32bitBE(0x04,streamFile) - dspm.header_offset;
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_WII_WSD;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .ddsp - full interleaved dsp [The Sims 2 - Pets (Wii)] */
VGMSTREAM * init_vgmstream_dsp_ddsp(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "ddsp"))
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;

    dspm.header_offset = 0x00;
    dspm.header_spacing = (get_streamfile_size(streamFile) / dspm.channel_count);
    dspm.start_offset = 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_DDSP;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* iSWS - Sumo Digital header + interleaved dsp [DiRT 2 (Wii), F1 2009 (Wii)] */
VGMSTREAM * init_vgmstream_wii_was(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "was,dsp,isws"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x69535753) /* "iSWS" */
        goto fail;

    dspm.channel_count = read_32bitBE(0x08,streamFile);
    dspm.max_channels = 2;

    dspm.header_offset = 0x08 + read_32bitBE(0x04,streamFile);
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channel_count*dspm.header_spacing;
    dspm.interleave = read_32bitBE(0x10,streamFile);

    dspm.meta_type = meta_WII_WAS;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .str - Infogrames raw interleaved dsp [Micro Machines (GC), Superman: Shadow of Apokolips (GC)] */
VGMSTREAM * init_vgmstream_dsp_str_ig(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "str"))
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x80;
    dspm.start_offset = 0x800;
    dspm.interleave = 0x4000;
    
    dspm.meta_type = meta_DSP_STR_IG;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .dsp - Ubisoft interleaved dsp with bad loop start [Speed Challenge: Jacques Villeneuve's Racing Vision (GC), XIII (GC)] */
VGMSTREAM * init_vgmstream_dsp_xiii(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "dsp"))
        goto fail;

    dspm.channel_count = 2;
    dspm.max_channels = 2;
    dspm.fix_loop_start = 1; /* loop flag but strange loop start instead of 0 (maybe shouldn't loop) */

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing * dspm.channel_count;
    dspm.interleave = 0x08;

    dspm.meta_type = meta_DSP_XIII;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* NPD - Icon Games header + subinterleaved DSPs [Vertigo (Wii), Build n' Race (Wii)] */
VGMSTREAM * init_vgmstream_wii_ndp(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "ndp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4E445000) /* "NDP\0" */
        goto fail;
    if (read_32bitLE(0x08,streamFile) + 0x18 != get_streamfile_size(streamFile))
        goto fail;
    /* 0x0c: sample rate */

    dspm.channel_count = read_32bitLE(0x10,streamFile);
    dspm.max_channels = 2;

    dspm.header_offset = 0x18;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channel_count*dspm.header_spacing;
    dspm.interleave = 0x04;

    dspm.meta_type = meta_WII_NDP;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* Cabela's series (Magic Wand dev?) - header + interleaved dsp [Cabela's Big Game Hunt 2005 Adventures (GC), Cabela's Outdoor Adventures (GC)] */
VGMSTREAM * init_vgmstream_dsp_cabelas(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "dsp"))
        goto fail;
    /* has extra stuff in the reserved data, without it this meta may catch other DSPs it shouldn't */
    if (read_32bitBE(0x50,streamFile) == 0 || read_32bitBE(0x54,streamFile) == 0)
        goto fail;

    /* sfx are mono, but standard dsp will catch them tho */
    dspm.channel_count = read_32bitBE(0x00,streamFile) == read_32bitBE(0x60,streamFile) ? 2 : 1;
    dspm.max_channels = 2;
    dspm.force_loop = (dspm.channel_count > 1);

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channel_count*dspm.header_spacing;
    dspm.interleave = 0x10;

    dspm.meta_type = meta_DSP_CABELAS;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* AAAp - Acclaim Austin Audio header + interleaved dsp [Vexx (GC), Turok: Evolution (GC)] */
VGMSTREAM * init_vgmstream_ngc_dsp_aaap(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "dsp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x41414170) /* "AAAp" */
        goto fail;

    dspm.channel_count = read_16bitBE(0x06,streamFile);
    dspm.max_channels = 2;

    dspm.header_offset = 0x08;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channel_count*dspm.header_spacing;
    dspm.interleave = (uint16_t)read_16bitBE(0x04,streamFile);

    dspm.meta_type = meta_NGC_DSP_AAAP;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* DSPW - Capcom header + full interleaved DSP [Sengoku Basara 3 (Wii), Monster Hunter 3 Ultimate (WiiU)] */
VGMSTREAM * init_vgmstream_dsp_dspw(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};
    size_t data_size;

    /* check extension */
    if (!check_extensions(streamFile, "dspw"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x44535057) /* "DSPW" */
        goto fail;

    /* ignore time marker */
    data_size = read_32bitBE(0x08, streamFile);
    if (read_32bitBE(data_size - 0x10, streamFile) == 0x74494D45) /* "tIME" */
        data_size -= 0x10; /* (ignore, 2 ints in YYYYMMDD hhmmss00) */

    /* some files have a mrkr section with multiple loop regions added at the end (variable size) */
    {
        off_t mrkr_offset = data_size - 0x04;
        off_t max_offset = data_size - 0x1000;
        while (mrkr_offset > max_offset) {
            if (read_32bitBE(mrkr_offset, streamFile) != 0x6D726B72) { /* "mrkr" */
                mrkr_offset -= 0x04;
            } else {
                data_size = mrkr_offset;
                break;
            }
        }
    }
    data_size -= 0x20; /* header size */
    /* 0x10: loop start, 0x14: loop end, 0x1c: num_samples */

    dspm.channel_count = read_32bitBE(0x18, streamFile);
    dspm.max_channels = 6; /* 6ch in Monster Hunter 3 Ultimate */

    dspm.header_offset = 0x20;
    dspm.header_spacing = data_size / dspm.channel_count;
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = data_size / dspm.channel_count;

    dspm.meta_type = meta_DSP_DSPW;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* iadp - custom header + interleaved dsp [Dr. Muto (GC)] */
VGMSTREAM * init_vgmstream_ngc_dsp_iadp(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    /* .adp: actual extension, .iadp: header id */
    if (!check_extensions(streamFile, "adp,iadp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x69616470) /* "iadp" */
        goto fail;

    dspm.channel_count = read_32bitBE(0x04,streamFile);
    dspm.max_channels = 2;

    dspm.header_offset = 0x20;
    dspm.header_spacing = 0x60;
    dspm.start_offset = read_32bitBE(0x1C,streamFile);
    dspm.interleave = read_32bitBE(0x08,streamFile);

    dspm.meta_type = meta_NGC_DSP_IADP;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

//todo might be only part of a full header?
/* CSMP - Retro Studios header + interleaved DSPs [Metroid Prime 3 (Wii), Donkey Kong Country Returns (Wii)] */
VGMSTREAM * init_vgmstream_ngc_dsp_csmp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    char filename[PATH_LIMIT];
    long current_offset;
    int tries;
    struct dsp_header header;
    int chanel_count, i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("csmp",filename_extension(filename))) goto fail;

    if (read_32bitBE(0x00, streamFile) != 0x43534D50) /* "CSMP" */
        goto fail;
    if (read_32bitBE(0x04, streamFile) != 1)  /* version? */
        goto fail;

    chanel_count = 1;
    start_offset = 0x60;

    current_offset = 0x08;
    tries = 0;
    while (1) {
        uint32_t chunk_id, chunk_size;

        if (tries > 4)
            goto fail;

        chunk_id   = read_32bitBE(current_offset + 0x00, streamFile);
        chunk_size = read_32bitBE(current_offset + 0x04, streamFile);
        current_offset += 0x08;
        if (chunk_id != 0x44415441) { /* "DATA" */
            current_offset += chunk_size;
            tries++;
            continue;
        }

        break;
    }

    if (read_dsp_header(&header, current_offset, streamFile)) goto fail;



    /* check initial predictor/scale */
    /* Retro doesn't seem to abide by this */
    //if (header.initial_ps != (uint8_t)read_8bit(current_offset + start_offset,streamFile))
    //    goto fail;

    /* check type==0 and gain==0 */
    if (header.format || header.gain)
        goto fail;

    /* Retro doesn't seem to abide by this */
    /* check loop predictor/scale */
    if (header.loop_flag) {
//        off_t loop_off = header.loop_start_offset/16*8;
//        if (header.loop_ps != (uint8_t)read_8bit(current_offset + start_offset+loop_off,streamFile))
//            goto fail;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(chanel_count,header.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = header.sample_rate;
    vgmstream->num_samples = header.sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(header.loop_start_offset);
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(header.loop_end_offset)+1;

    /* don't know why, but it does happen*/
    if (vgmstream->loop_end_sample > vgmstream->num_samples)
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_DSP_CSMP;

    /* coeffs */
    for (i=0;i<16;i++)
        vgmstream->ch[0].adpcm_coef[i] = header.coef[i];
    vgmstream->ch[0].adpcm_history1_16 = header.initial_hist1;
    vgmstream->ch[0].adpcm_history2_16 = header.initial_hist2;

    /* open the file for reading */
    vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!vgmstream->ch[0].streamfile) goto fail;
    vgmstream->ch[0].channel_start_offset=
         vgmstream->ch[0].offset=current_offset + start_offset;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .mcadpcm - Custom header + full interleaved dsp [Skyrim (Switch)] */
VGMSTREAM * init_vgmstream_dsp_mcadpcm(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(streamFile, "mcadpcm"))
        goto fail;
    /* could validate dsp sizes but only for +1ch, check_dsp_samples will do it anyway */
    //if (read_32bitLE(0x08,streamFile) != read_32bitLE(0x10,streamFile))
    //   goto fail;

    dspm.channel_count = read_32bitLE(0x00,streamFile);
    dspm.max_channels = 2;
    dspm.little_endian = 1;

    dspm.header_offset =  read_32bitLE(0x04,streamFile);
    dspm.header_spacing = dspm.channel_count == 1 ? 0 :
        read_32bitLE(0x0c,streamFile) - dspm.header_offset; /* channel 2 start, only with Nch */
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_MCADPCM;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}

/* .switch_audio - UE4 standard LE header + full interleaved dsp [Gal Gun 2 (Switch)] */
VGMSTREAM * init_vgmstream_dsp_switch_audio(STREAMFILE *streamFile) {
    dsp_meta dspm = {0};

    /* checks */
    /* .switch_audio: possibly UE4 class name rather than extension, .dsp: assumed */
    if (!check_extensions(streamFile, "switch_audio,dsp"))
        goto fail;

    /* manual double header test */
    if (read_32bitLE(0x00, streamFile) == read_32bitLE(get_streamfile_size(streamFile) / 2, streamFile))
        dspm.channel_count = 2;
    else
        dspm.channel_count = 1;
    dspm.max_channels = 2;
    dspm.little_endian = 1;

    dspm.header_offset = 0x00;
    dspm.header_spacing = get_streamfile_size(streamFile) / dspm.channel_count;
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_SWITCH_AUDIO;
    return init_vgmstream_dsp_common(streamFile, &dspm);
fail:
    return NULL;
}
