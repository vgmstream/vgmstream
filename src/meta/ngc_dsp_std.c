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
    /* later/mdsp extension */
    int16_t channel_count;
    int16_t block_size;
};

/* read the above struct; returns nonzero on failure */
static int read_dsp_header_endian(struct dsp_header *header, off_t offset, STREAMFILE *streamFile, int big_endian) {
    int32_t (*get_32bit)(uint8_t *) = big_endian ? get_32bitBE : get_32bitLE;
    int16_t (*get_16bit)(uint8_t *) = big_endian ? get_16bitBE : get_16bitLE;
    int i;
    uint8_t buf[0x4e]; /* usually padded out to 0x60 */

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
static int dsp_load_header_le(struct dsp_header* ch_header, int channels, STREAMFILE *streamFile, off_t offset, size_t spacing) {
    return dsp_load_header_endian(ch_header, channels, streamFile, offset, spacing, 0);
}
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
static int check_dsp_initial_ps(struct dsp_header* ch_header, int channels, STREAMFILE *streamFile, off_t offset, size_t spacing) {
    int i;

    /* check initial predictor/scale */
    for (i = 0; i < channels; i++) {
        if (ch_header[i].initial_ps != (uint8_t)read_8bit(offset + i*spacing, streamFile))
            goto fail;
    }

    return 1;
fail:
    return 0;
}
static int check_dsp_loop_ps(struct dsp_header* ch_header, int channels, STREAMFILE *streamFile, off_t offset, size_t spacing) {
    int i;

    if (!ch_header[0].loop_flag)
        return 1;

    /* check loop predictor/scale */
    for (i = 0; i < channels; i++) {
        off_t loop_offset = ch_header[i].loop_start_offset / 16 * 8;
        if (ch_header[i].loop_ps != (uint8_t)read_8bit(offset + i*spacing + loop_offset,streamFile))
            goto fail;
    }

    return 1;
fail:
    return 0;
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
    /* .dsp: standard, .adp: Dr. Muto (GC) mono files */
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
 * twice and add interleave, or just concatenate the channels. We'll support
 * them all here.
 * Note that Cstr isn't here, despite using the form of the standard header,
 * because its loop values are wacky. */

/* .stm
 * Used in Paper Mario 2, Fire Emblem: Path of Radiance, Cubivore
 * I suspected that this was an Intelligent Systems format, but its use in
 * Cubivore calls that into question. */
VGMSTREAM * init_vgmstream_ngc_dsp_stm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    struct dsp_header ch0_header, ch1_header;
    int i;
    int stm_header_sample_rate;
    int channel_count;
    const off_t start_offset = 0x100;
    off_t first_channel_size;
    off_t second_channel_start;

    /* check extension, case insensitive */
    /* to avoid collision with Scream Tracker 2 Modules, also ending in .stm
     * and supported by default in Winamp, it was policy in the old days to
     * rename these files to .dsp */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("stm",filename_extension(filename)) &&
            strcasecmp("dsp",filename_extension(filename))) goto fail;

    /* check intro magic */
    if (read_16bitBE(0, streamFile) != 0x0200) goto fail;

    channel_count = read_32bitBE(4, streamFile);
    /* only stereo and mono are known */
    if (channel_count != 1 && channel_count != 2) goto fail;

    first_channel_size = read_32bitBE(8, streamFile);
    /* this is bad rounding, wastes space, but it looks like that's what's
     * used */
    second_channel_start = ((start_offset+first_channel_size)+0x20)/0x20*0x20;

    /* an additional check */
    stm_header_sample_rate = (uint16_t)read_16bitBE(2, streamFile);

    /* read the DSP headers */
    if (read_dsp_header(&ch0_header, 0x40, streamFile)) goto fail;
    if (channel_count == 2) {
        if (read_dsp_header(&ch1_header, 0xa0, streamFile)) goto fail;
    }

    /* checks for fist channel */
    {
        if (ch0_header.sample_rate != stm_header_sample_rate) goto fail;

        /* check initial predictor/scale */
        if (ch0_header.initial_ps != (uint8_t)read_8bit(start_offset, streamFile))
            goto fail;

        /* check type==0 and gain==0 */
        if (ch0_header.format || ch0_header.gain)
            goto fail;

        if (ch0_header.loop_flag) {
            off_t loop_off;
            /* check loop predictor/scale */
            loop_off = ch0_header.loop_start_offset/16*8;
            if (ch0_header.loop_ps != (uint8_t)read_8bit(start_offset+loop_off,streamFile))
                goto fail;
        }
    }


    /* checks for second channel */
    if (channel_count == 2) {
        if (ch1_header.sample_rate != stm_header_sample_rate) goto fail;

        /* check for agreement with first channel header */
        if (
            ch0_header.sample_count != ch1_header.sample_count ||
            ch0_header.nibble_count != ch1_header.nibble_count ||
            ch0_header.loop_flag != ch1_header.loop_flag ||
            ch0_header.loop_start_offset != ch1_header.loop_start_offset ||
            ch0_header.loop_end_offset != ch1_header.loop_end_offset
           ) goto fail;

        /* check initial predictor/scale */
        if (ch1_header.initial_ps != (uint8_t)read_8bit(second_channel_start, streamFile))
            goto fail;

        /* check type==0 and gain==0 */
        if (ch1_header.format || ch1_header.gain)
            goto fail;

        if (ch1_header.loop_flag) {
            off_t loop_off;
            /* check loop predictor/scale */
            loop_off = ch1_header.loop_start_offset/16*8;
            /*printf("loop_start_offset=%x\nloop_ps=%x\nloop_off=%x\n",ch1_header.loop_start_offset,ch1_header.loop_ps,second_channel_start+loop_off);*/
            if (ch1_header.loop_ps != (uint8_t)read_8bit(second_channel_start+loop_off,streamFile))
                goto fail;
        }
    }

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(channel_count, ch0_header.loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = ch0_header.sample_count;
    vgmstream->sample_rate = ch0_header.sample_rate;

    vgmstream->loop_start_sample = dsp_nibbles_to_samples(
            ch0_header.loop_start_offset);
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(
            ch0_header.loop_end_offset)+1;

    /* don't know why, but it does happen*/
    if (vgmstream->loop_end_sample > vgmstream->num_samples)
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_DSP_STM;

    /* coeffs */
    for (i=0;i<16;i++)
        vgmstream->ch[0].adpcm_coef[i] = ch0_header.coef[i];

    /* initial history */
    /* always 0 that I've ever seen, but for completeness... */
    vgmstream->ch[0].adpcm_history1_16 = ch0_header.initial_hist1;
    vgmstream->ch[0].adpcm_history2_16 = ch0_header.initial_hist2;

    if (channel_count == 2) {
        /* coeffs */
        for (i=0;i<16;i++)
            vgmstream->ch[1].adpcm_coef[i] = ch1_header.coef[i];

        /* initial history */
        /* always 0 that I've ever seen, but for completeness... */
        vgmstream->ch[1].adpcm_history1_16 = ch1_header.initial_hist1;
        vgmstream->ch[1].adpcm_history2_16 = ch1_header.initial_hist2;
    }

    /* open the file for reading */
    vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

    if (!vgmstream->ch[0].streamfile) goto fail;

    vgmstream->ch[0].channel_start_offset=
        vgmstream->ch[0].offset=start_offset;

    if (channel_count == 2) {
        vgmstream->ch[1].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

        if (!vgmstream->ch[1].streamfile) goto fail;

        vgmstream->ch[1].channel_start_offset=
            vgmstream->ch[1].offset=second_channel_start;
    }

    return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* mpdsp: looks like a standard .dsp header, but the data is actually
 * interleaved stereo 
 * The files originally had a .dsp extension, we rename them to .mpdsp so we
 * can catch this.
 */

VGMSTREAM * init_vgmstream_ngc_mpdsp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    struct dsp_header header;
    const off_t start_offset = 0x60;
    int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("mpdsp",filename_extension(filename))) goto fail;

    if (read_dsp_header(&header, 0, streamFile)) goto fail;

    /* none have loop flag set, save us from loop code that involves them */
    if (header.loop_flag) goto fail;

    /* check initial predictor/scale */
    if (header.initial_ps != (uint8_t)read_8bit(start_offset,streamFile))
        goto fail;

    /* check type==0 and gain==0 */
    if (header.format || header.gain)
        goto fail;
        
    /* build the VGMSTREAM */


    /* no loop flag, but they do loop */
    vgmstream = allocate_vgmstream(2,0);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = header.sample_count/2;
    vgmstream->sample_rate = header.sample_rate;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0xf000;
    vgmstream->meta_type = meta_DSP_MPDSP;

    /* coeffs */
    for (i=0;i<16;i++) {
        vgmstream->ch[0].adpcm_coef[i] = header.coef[i];
        vgmstream->ch[1].adpcm_coef[i] = header.coef[i];
    }
    
    /* initial history */
    /* always 0 that I've ever seen, but for completeness... */
    vgmstream->ch[0].adpcm_history1_16 = header.initial_hist1;
    vgmstream->ch[0].adpcm_history2_16 = header.initial_hist2;
    vgmstream->ch[1].adpcm_history1_16 = header.initial_hist1;
    vgmstream->ch[1].adpcm_history2_16 = header.initial_hist2;

    /* open the file for reading */
    for (i=0;i<2;i++) {
        vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,
                vgmstream->interleave_block_size);

        if (!vgmstream->ch[i].streamfile) goto fail;

        vgmstream->ch[i].channel_start_offset=
            vgmstream->ch[i].offset=start_offset+
            vgmstream->interleave_block_size*i;
    }

    return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* a bunch of formats that are identical except for file extension,
 * but have different interleaves */
VGMSTREAM * init_vgmstream_ngc_dsp_std_int(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];

    const off_t start_offset = 0xc0;
    off_t interleave;
    int meta_type;

    struct dsp_header ch0_header,ch1_header;

    int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strlen(filename) > 7 && !strcasecmp("_lr.dsp",filename+strlen(filename)-7)) {
        /* Bomberman Jetters */
        interleave = 0x14180;
        meta_type = meta_DSP_JETTERS;
    } else if (!strcasecmp("mss",filename_extension(filename))) {
        interleave = 0x1000;
        meta_type = meta_DSP_MSS;
    } else if (!strcasecmp("gcm",filename_extension(filename))) {
        interleave = 0x8000;
        meta_type = meta_DSP_GCM;
    } else goto fail;

    if (read_dsp_header(&ch0_header, 0, streamFile)) goto fail;
    if (read_dsp_header(&ch1_header, 0x60, streamFile)) goto fail;

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
       ) {
        /* Timesplitters 2 GC's ts2_atom_smasher_44_fx.mss differs slightly in samples but plays ok */
        if (meta_type != meta_DSP_MSS)
            goto fail;
    }

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
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(
            ch0_header.loop_start_offset);
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(
            ch0_header.loop_end_offset)+1;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_type;

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

    /* open the file for reading */
    for (i=0;i<2;i++) {
        vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

        if (!vgmstream->ch[i].streamfile) goto fail;

        vgmstream->ch[i].channel_start_offset=
            vgmstream->ch[i].offset=start_offset+i*interleave;
    }

    return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

/* IDSP with multiple standard DSP headers - from SSB4 (3DS), Tekken Tag Tournament 2 (Wii U) */
#define MULTI_IDSP_MAX_CHANNELS  8
VGMSTREAM * init_vgmstream_3ds_idsp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;

    off_t idsp_offset = 0;
    off_t start_offset;
    off_t interleave;

    struct dsp_header ch_headers[MULTI_IDSP_MAX_CHANNELS];
    int i, ch;
    int channel_count;

    /* check extension, case insensitive */
    //if (check_extensions(streamFile,"idsp,nus3bank")) goto fail;

    /* check header magic */
    if( read_32bitBE(0x0,streamFile) != 0x49445350 ) /* "IDSP" */
    {
        /* try NUS3 format instead */
        if (read_32bitBE(0,streamFile) != 0x4E555333) goto fail; /* "NUS3" */
        
        /* Header size */
        idsp_offset  = 0x14 + read_32bitLE( 0x10, streamFile );
        
        idsp_offset += read_32bitLE( 0x1C, streamFile ) + 8;
        idsp_offset += read_32bitLE( 0x24, streamFile ) + 8;
        idsp_offset += read_32bitLE( 0x2C, streamFile ) + 8;
        idsp_offset += read_32bitLE( 0x34, streamFile ) + 8;
        idsp_offset += read_32bitLE( 0x3C, streamFile ) + 8;
        idsp_offset += read_32bitLE( 0x44, streamFile ) + 8;
        idsp_offset += 8;

        /* check magic */
        if (read_32bitBE(idsp_offset,streamFile) != 0x49445350) goto fail; /* "IDSP" */
    }
    
    channel_count = read_32bitBE(idsp_offset+0x8, streamFile);
    if (channel_count > MULTI_IDSP_MAX_CHANNELS) goto fail;

    start_offset = read_32bitBE(idsp_offset+0x28,streamFile) + idsp_offset;
    interleave = 0x10;

    /* read standard dsp header per channel and do some validations */
    for (ch=0; ch < channel_count; ch++) {
        /* read 0x60 header per channel */
        if (read_dsp_header(&ch_headers[ch], idsp_offset + 0x40 + 0x60*ch, streamFile)) goto fail;

        /* check initial values */
        if (ch_headers[ch].initial_ps != (uint8_t)read_8bit(start_offset + interleave*ch, streamFile)) goto fail;
        if (ch_headers[ch].format || ch_headers[ch].gain) goto fail;

        /* check for agreement with prev channel*/
        if (ch > 0 && (
                ch_headers[ch].sample_count      != ch_headers[ch-1].sample_count ||
                ch_headers[ch].nibble_count      != ch_headers[ch-1].nibble_count ||
                ch_headers[ch].sample_rate       != ch_headers[ch-1].sample_rate ||
                ch_headers[ch].loop_flag         != ch_headers[ch-1].loop_flag ||
                ch_headers[ch].loop_start_offset != ch_headers[ch-1].loop_start_offset ||
                ch_headers[ch].loop_end_offset   != ch_headers[ch-1].loop_end_offset
            )) goto fail;


#if 0   //this is wrong for >2ch and will fail
        /* check loop predictor/scale */
        if (ch_headers[ch].loop_flag) {
            off_t loop_off;
            loop_off = ch_headers[ch].loop_start_offset / 8 / channel_count * 8;
            loop_off = (loop_off / interleave * interleave * channel_count) + (loop_off%interleave);
            if (ch_headers[ch].loop_ps != (uint8_t)read_8bit(start_offset + loop_off + interleave*ch, streamFile)) goto fail;
        }
#endif
    }
    /* check first channel (implicitly all ch) agree with main sample rate */
    if (ch_headers[0].sample_rate  != read_32bitBE(idsp_offset+0xc, streamFile)) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_headers[0].loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->num_samples = ch_headers[0].sample_count;
    vgmstream->sample_rate = ch_headers[0].sample_rate;

    /* TODO: adjust for interleave? */
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_headers[0].loop_start_offset);
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(ch_headers[0].loop_end_offset) + 1;
    /* games will ignore loop_end and use num_samples if going over it
     *  only needed for user-created IDSPs, but it's possible loop_end_sample shouldn't add +1 above */
    if (vgmstream->loop_end_sample > vgmstream->num_samples)
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = channel_count > 1 ? layout_interleave : layout_none;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_3DS_IDSP;


    /* set DSP coefs/history */
    for (ch=0; ch < channel_count; ch++) {
        for (i=0;i<16;i++) {
            vgmstream->ch[ch].adpcm_coef[i] = ch_headers[ch].coef[i];
        }
        /* always 0 that I've ever seen, but for completeness... */
        vgmstream->ch[ch].adpcm_history1_16 = ch_headers[ch].initial_hist1;
        vgmstream->ch[ch].adpcm_history2_16 = ch_headers[ch].initial_hist2;
    }

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#define SADB_MAX_CHANNELS 2
/* sadb - Procyon Studio header + interleaved dsp [Shiren the Wanderer 3 (Wii), Disaster: Day of Crisis (Wii)] */
VGMSTREAM * init_vgmstream_sadb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing, interleave;
    int channel_count;
    struct dsp_header ch_header[SADB_MAX_CHANNELS];

    /* check extension */
    if (!check_extensions(streamFile, "sad"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x73616462) /* "sadb" */
        goto fail;

    channel_count = read_8bit(0x32, streamFile);
    if (channel_count > SADB_MAX_CHANNELS) goto fail;

    header_offset = 0x80;
    header_spacing = 0x60;
    start_offset = read_32bitBE(0x48,streamFile);
    interleave = 0x10;

    /* read dsp */
    if (!dsp_load_header(ch_header, channel_count, streamFile,header_offset,header_spacing)) goto fail;
    if (!check_dsp_format(ch_header, channel_count)) goto fail;
    if (!check_dsp_samples(ch_header, channel_count)) goto fail;
    if (!check_dsp_initial_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;
    //todo: loop check fails unless adjusted:
    // loop_offset = (loop_offset / spacing * spacing * channels) + (loop_offset % spacing);
    //if (!check_dsp_loop_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_header[0].loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = meta_DSP_SADB;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    setup_vgmstream_dsp(vgmstream, ch_header);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
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


/* SWD (found in Conflict - Desert Storm 1 & 2 */
VGMSTREAM * init_vgmstream_ngc_swd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    off_t start_offset;
    off_t interleave;

    struct dsp_header ch0_header, ch1_header;
    int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("swd",filename_extension(filename))) goto fail;

    if (read_dsp_header(&ch0_header, 0x08, streamFile)) goto fail;
    if (read_dsp_header(&ch1_header, 0x68, streamFile)) goto fail;

    /* check header magic */
    if (read_32bitBE(0x00,streamFile) != 0x505346D1) /* PSF\0xD1 */
        goto fail;

    start_offset = 0xC8;
    interleave = 0x8;

#if 0
    /* check initial predictor/scale */
    if (ch0_header.initial_ps != (uint8_t)read_8bit(start_offset,streamFile))
        goto fail;
    if (ch1_header.initial_ps != (uint8_t)read_8bit(start_offset+interleave,streamFile))
        goto fail;
#endif

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

#if 0
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
#endif

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(2,ch0_header.loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = ch0_header.sample_count;
    vgmstream->sample_rate = ch0_header.sample_rate;

    /* TODO: adjust for interleave? */
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(
            ch0_header.loop_start_offset);
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(
            ch0_header.loop_end_offset)+1;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_NGC_SWD;

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
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(
            ch0_header.loop_start_offset);
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(
            ch0_header.loop_end_offset)+1;

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

#define WSD_MAX_CHANNELS 2
/* .wsd - Custom header + full interleaved dsp [Phantom Brave (Wii)] */
VGMSTREAM * init_vgmstream_wii_wsd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing, interleave;
    int channel_count;
    struct dsp_header ch_header[WSD_MAX_CHANNELS];

    /* check extension */
    if (!check_extensions(streamFile, "wsd"))
        goto fail;
    if (read_32bitBE(0x08,streamFile) != read_32bitBE(0x0c,streamFile)) /* channel sizes */
        goto fail;

    channel_count = 2;
    if (channel_count > WSD_MAX_CHANNELS) goto fail;

    header_offset =  read_32bitBE(0x00,streamFile);
    header_spacing = read_32bitBE(0x04,streamFile) - header_offset;
    start_offset = header_offset + 0x60;
    interleave = header_spacing;

    /* read dsp */
    if (!dsp_load_header(ch_header, channel_count, streamFile,header_offset,header_spacing)) goto fail;
    if (!check_dsp_format(ch_header, channel_count)) goto fail;
    if (!check_dsp_samples(ch_header, channel_count)) goto fail;
    if (!check_dsp_initial_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;
    if (!check_dsp_loop_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_header[0].loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = meta_DSP_WII_WSD;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    setup_vgmstream_dsp(vgmstream, ch_header);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#define DDSP_MAX_CHANNELS 2
/* .ddsp - full interleaved dsp [The Sims 2 - Pets (Wii)] */
VGMSTREAM * init_vgmstream_dsp_ddsp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing, interleave;
    int channel_count;
    struct dsp_header ch_header[DDSP_MAX_CHANNELS];

    /* check extension */
    if (!check_extensions(streamFile, "ddsp"))
        goto fail;

    channel_count = 2;
    if (channel_count > DDSP_MAX_CHANNELS) goto fail;

    header_offset = 0x00;
    header_spacing = (get_streamfile_size(streamFile) / channel_count);
    start_offset = 0x60;
    interleave = header_spacing;

    /* read dsp */
    if (!dsp_load_header(ch_header, channel_count, streamFile,header_offset,header_spacing)) goto fail;
    if (!check_dsp_format(ch_header, channel_count)) goto fail;
    if (!check_dsp_samples(ch_header, channel_count)) goto fail;
    if (!check_dsp_initial_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;
    if (!check_dsp_loop_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_header[0].loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = meta_DSP_DDSP;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    setup_vgmstream_dsp(vgmstream, ch_header);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#define ISWS_MAX_CHANNELS 2
/* iSWS - Sumo Digital header + interleaved dsp [DiRT 2 (Wii), F1 2009 (Wii)] */
VGMSTREAM * init_vgmstream_wii_was(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing, interleave;
    int channel_count;
    struct dsp_header ch_header[ISWS_MAX_CHANNELS];

    /* check extension */
    if (!check_extensions(streamFile, "was,dsp,isws"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x69535753) /* "iSWS" */
        goto fail;

    channel_count = read_32bitBE(0x08,streamFile);
    if (channel_count > ISWS_MAX_CHANNELS) goto fail;

    header_offset = 0x08 + read_32bitBE(0x04,streamFile);
    header_spacing = 0x60;
    start_offset = header_offset + channel_count*header_spacing;
    interleave = read_32bitBE(0x10,streamFile);

    /* read dsp */
    if (!dsp_load_header(ch_header, channel_count, streamFile,header_offset,header_spacing)) goto fail;
    if (!check_dsp_format(ch_header, channel_count)) goto fail;
    if (!check_dsp_samples(ch_header, channel_count)) goto fail;
    if (!check_dsp_initial_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;
    if (!check_dsp_loop_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_header[0].loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = meta_WII_WAS;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    setup_vgmstream_dsp(vgmstream, ch_header);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#define STR_IG_MAX_CHANNELS 2
/* .str - Infogrames raw interleaved dsp [Micro Machines (GC), Superman: Shadow of Apokolips (GC)] */
VGMSTREAM * init_vgmstream_dsp_str_ig(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing, interleave;
    int channel_count;
    struct dsp_header ch_header[STR_IG_MAX_CHANNELS];

    /* check extension */
    if (!check_extensions(streamFile, "str"))
        goto fail;

    channel_count = 2;
    if (channel_count > STR_IG_MAX_CHANNELS) goto fail;

    header_offset = 0x00;
    header_spacing = 0x80;
    start_offset = 0x800;
    interleave = 0x4000;

    /* read dsp */
    if (!dsp_load_header(ch_header, channel_count, streamFile,header_offset,header_spacing)) goto fail;
    if (!check_dsp_format(ch_header, channel_count)) goto fail;
    if (!check_dsp_samples(ch_header, channel_count)) goto fail;
    if (!check_dsp_initial_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;
    if (!check_dsp_loop_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_header[0].loop_flag);
    if (!vgmstream) goto fail;
    
    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = meta_DSP_STR_IG;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    setup_vgmstream_dsp(vgmstream, ch_header);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .dsp - Ubisoft raw interleaved dsp [Speed Challenge: Jacques Villeneuve's Racing Vision (GC), XIII (GC)] */
//todo unusual loop values
VGMSTREAM * init_vgmstream_dsp_xiii(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    struct dsp_header ch0_header,ch1_header;
    off_t ch1_header_start, ch2_header_start, ch1_start, ch2_start;
    int channel_count;
    int i;

    /* check extension */
    if (!check_extensions(streamFile, "dsp"))
        goto fail;

    channel_count = 2;

    ch1_header_start = 0x00;
    ch2_header_start = 0x60;
    ch1_start = 0xC0;
    ch2_start = 0xC8;

    /* get DSP headers */
    if (read_dsp_header(&ch0_header, ch1_header_start, streamFile)) goto fail;
    if (read_dsp_header(&ch1_header, ch2_header_start, streamFile)) goto fail;
    
    /* check initial predictor/scale */
    if (ch0_header.initial_ps != (uint8_t)read_8bit(ch1_start, streamFile))
      goto fail;
    if (ch1_header.initial_ps != (uint8_t)read_8bit(ch2_start, streamFile))
      goto fail;

    /* check type==0 and gain==0 */
    if (ch0_header.format || ch0_header.gain)
      goto fail;
    if (ch1_header.format || ch1_header.gain)
      goto fail;

    /* check for agreement */
    if (
            ch0_header.sample_count != ch1_header.sample_count ||
            ch0_header.nibble_count != ch1_header.nibble_count ||
            ch0_header.sample_rate != ch1_header.sample_rate ||
            ch0_header.loop_flag != ch1_header.loop_flag ||
            //ch0_header.loop_start_offset != ch1_header.loop_start_offset ||
            ch0_header.loop_end_offset != ch1_header.loop_end_offset
       ) goto fail;

    if (ch0_header.loop_flag)
    {
        off_t loop_off;
        /* check loop predictor/scale */
        loop_off = 0x0; //ch0_header.loop_start_offset/16*8;

        if (ch0_header.loop_ps != (uint8_t)read_8bit(ch1_start+loop_off,streamFile))
          goto fail;
        if (ch1_header.loop_ps != (uint8_t)read_8bit(ch2_start+loop_off,streamFile))
          goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, ch1_header.loop_flag);
    if (!vgmstream) goto fail;
    
    /* fill in the vital statistics */
    vgmstream->num_samples = ch0_header.sample_count;
    vgmstream->sample_rate = ch0_header.sample_rate;

    vgmstream->loop_start_sample = 0x0; //dsp_nibbles_to_samples(ch0_header.loop_start_offset);
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(
            ch0_header.loop_end_offset)+1;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x8;
    vgmstream->meta_type = meta_DSP_XIII;

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

    /* open the file for reading */
    vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!vgmstream->ch[0].streamfile)
        goto fail;
    vgmstream->ch[0].channel_start_offset = vgmstream->ch[0].offset=ch1_start;
    
    vgmstream->ch[1].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!vgmstream->ch[1].streamfile)
        goto fail;
    vgmstream->ch[1].channel_start_offset = vgmstream->ch[1].offset=ch2_start;
    
    return vgmstream;


fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


#define NDP_MAX_CHANNELS 2
/* NPD - Icon Games header + subinterleaved DSPs [Vertigo (Wii), Build n' Race (Wii)] */
VGMSTREAM * init_vgmstream_wii_ndp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing, interleave;
    int channel_count;
    struct dsp_header ch_header[NDP_MAX_CHANNELS];


    /* check extension */
    if (!check_extensions(streamFile, "ndp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x4E445000) /* "NDP\0" */
        goto fail;
    if (read_32bitLE(0x08,streamFile) + 0x18 != get_streamfile_size(streamFile))
        goto fail;
    /* 0x0c: sample rate */

    channel_count = read_32bitLE(0x10,streamFile);
    if (channel_count > NDP_MAX_CHANNELS) goto fail;

    header_offset = 0x18;
    header_spacing = 0x60;
    start_offset = header_offset + channel_count*header_spacing;
    interleave = 0x04;

    /* read dsp */
    if (!dsp_load_header(ch_header, channel_count, streamFile,header_offset,header_spacing)) goto fail;
    if (!check_dsp_format(ch_header, channel_count)) goto fail;
    if (!check_dsp_samples(ch_header, channel_count)) goto fail;
    if (!check_dsp_initial_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;
    if (!check_dsp_loop_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_header[0].loop_flag);
    if (!vgmstream) goto fail;
    
    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = meta_WII_NDP;
    vgmstream->coding_type = coding_NGC_DSP_subint;
    vgmstream->layout_type = layout_none;
    vgmstream->interleave_block_size = interleave;
    setup_vgmstream_dsp(vgmstream, ch_header);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* found in "Cabelas" games, always stereo, looped and an interleave of 0x10 bytes */
VGMSTREAM * init_vgmstream_dsp_cabelas(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[PATH_LIMIT];
    struct dsp_header ch0_header,ch1_header;
    off_t ch1_header_start, ch2_header_start, ch1_start, ch2_start;
    int channel_count;
    int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("dsp",filename_extension(filename)))
          goto fail;
    
    channel_count = 2;

    ch1_header_start = 0x00;
    ch2_header_start = 0x60;
    ch1_start = 0xC0;
    ch2_start = 0xD0;

    /* get DSP headers */
    if (read_dsp_header(&ch0_header, ch1_header_start, streamFile)) goto fail;
    if (read_dsp_header(&ch1_header, ch2_header_start, streamFile)) goto fail;
    
    /* check initial predictor/scale */
    if (ch0_header.initial_ps != (uint8_t)read_8bit(ch1_start, streamFile))
      goto fail;
    if (ch1_header.initial_ps != (uint8_t)read_8bit(ch2_start, streamFile))
      goto fail;

    /* check type==0 and gain==0 */
    if (ch0_header.format || ch0_header.gain)
      goto fail;
    if (ch1_header.format || ch1_header.gain)
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


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, 1);
    if (!vgmstream) goto fail;
    
    /* fill in the vital statistics */
    vgmstream->num_samples = ch0_header.sample_count;
    vgmstream->sample_rate = ch0_header.sample_rate;

    vgmstream->loop_start_sample = 0;
    vgmstream->loop_end_sample =  dsp_nibbles_to_samples(ch0_header.loop_end_offset)+1;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;
    vgmstream->meta_type = meta_DSP_CABELAS;

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

    /* open the file for reading */
    vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!vgmstream->ch[0].streamfile)
        goto fail;
    vgmstream->ch[0].channel_start_offset = vgmstream->ch[0].offset=ch1_start;
    
    vgmstream->ch[1].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!vgmstream->ch[1].streamfile)
        goto fail;
    vgmstream->ch[1].channel_start_offset = vgmstream->ch[1].offset=ch2_start;
    
    return vgmstream;


fail:
    /* clean up anything we may have opened */
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}


#define AAAP_MAX_CHANNELS 2
/* AAAp - Acclaim Austin Audio header + interleaved dsp [Vexx (GC), Turok: Evolution (GC)] */
VGMSTREAM * init_vgmstream_ngc_dsp_aaap(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing, interleave;
    int channel_count;
    struct dsp_header ch_header[AAAP_MAX_CHANNELS];

    /* check extension */
    if (!check_extensions(streamFile, "dsp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x41414170) /* "AAAp" */
        goto fail;
    
    channel_count = read_16bitBE(0x06,streamFile);
    if (channel_count > AAAP_MAX_CHANNELS) goto fail;

    header_offset = 0x08;
    header_spacing = 0x60;
    start_offset = header_offset + channel_count*header_spacing;
    interleave = (uint16_t)read_16bitBE(0x04,streamFile);

    /* read dsp */
    if (!dsp_load_header(ch_header, channel_count, streamFile,header_offset,header_spacing)) goto fail;
    if (!check_dsp_format(ch_header, channel_count)) goto fail;
    if (!check_dsp_samples(ch_header, channel_count)) goto fail;
    if (!check_dsp_initial_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;
    if (!check_dsp_loop_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_header[0].loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = meta_NGC_DSP_AAAP;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    setup_vgmstream_dsp(vgmstream, ch_header);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


#define DSPW_MAX_CHANNELS 6  /* 6ch in Monster Hunter 3 Ultimate */
/* DSPW - Capcom header + full interleaved DSP [Sengoku Basara 3 (Wii), Monster Hunter 3 Ultimate (WiiU)] */
VGMSTREAM * init_vgmstream_dsp_dspw(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing, interleave, data_size;
    int channel_count;
    struct dsp_header ch_header[DSPW_MAX_CHANNELS];

    /* check extension */
    if (!check_extensions(streamFile, "dspw"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x44535057) /* "DSPW" */
        goto fail;


    /* 0x10: loop start, 0x14: loop end, 0x1c: num_samples */
    channel_count = read_32bitBE(0x18, streamFile);
    if (channel_count > DSPW_MAX_CHANNELS) goto fail;

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

    header_offset = 0x20;
    header_spacing = data_size / channel_count;
    start_offset = header_offset + 0x60;
    interleave = data_size / channel_count;

    /* read dsp */
    if (!dsp_load_header(ch_header, channel_count, streamFile,header_offset,header_spacing)) goto fail;
    if (!check_dsp_format(ch_header, channel_count)) goto fail;
    if (!check_dsp_samples(ch_header, channel_count)) goto fail;
    if (!check_dsp_initial_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;
    if (!check_dsp_loop_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_header[0].loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = meta_DSP_DSPW;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    setup_vgmstream_dsp(vgmstream, ch_header);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#define IADP_MAX_CHANNELS 2
/* iadp - custom header + interleaved dsp [Dr. Muto (GC)] */
VGMSTREAM * init_vgmstream_ngc_dsp_iadp(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing, interleave;
    int channel_count;
    struct dsp_header ch_header[IADP_MAX_CHANNELS];


    /* checks */
    /* .adp: actual extension, .iadp: header id */
    if (!check_extensions(streamFile, "adp,iadp"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x69616470) /* "iadp" */
        goto fail;
    
    channel_count = read_32bitBE(0x04,streamFile);
    if (channel_count != IADP_MAX_CHANNELS) goto fail;

    header_offset = 0x20;
    header_spacing = 0x60;
    start_offset = read_32bitBE(0x1C,streamFile);
    interleave = read_32bitBE(0x08,streamFile);

    /* read dsp */
    if (!dsp_load_header(ch_header, channel_count, streamFile,header_offset,header_spacing)) goto fail;
    if (!check_dsp_format(ch_header, channel_count)) goto fail;
    if (!check_dsp_samples(ch_header, channel_count)) goto fail;
    if (!check_dsp_initial_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;
    if (!check_dsp_loop_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_header[0].loop_flag);
    if (!vgmstream) goto fail;
    
    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = meta_NGC_DSP_IADP;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->interleave_block_size = read_32bitBE(0x8,streamFile);
    vgmstream->layout_type = layout_interleave;
    setup_vgmstream_dsp(vgmstream, ch_header);


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
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

    if (header.loop_flag) {
//        off_t loop_off;
        /* check loop predictor/scale */
//        loop_off = header.loop_start_offset/16*8;
        /* Retro doesn't seem to abide by this */
//        if (header.loop_ps != (uint8_t)read_8bit(current_offset + start_offset+loop_off,streamFile))
//            goto fail;
    }

    /* compare num_samples with nibble count */
    /*
    fprintf(stderr,"num samples (literal): %d\n",read_32bitBE(0,streamFile));
    fprintf(stderr,"num samples (nibbles): %d\n",dsp_nibbles_to_samples(read_32bitBE(4,streamFile)));
    */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(chanel_count,header.loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = header.sample_count;
    vgmstream->sample_rate = header.sample_rate;

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

#define MCADPCM_MAX_CHANNELS 2
/* .mcadpcm - Custom header + full interleaved dsp [Skyrim (Switch)] */
VGMSTREAM * init_vgmstream_dsp_mcadpcm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, header_offset;
    size_t header_spacing, interleave;
    int channel_count;
    struct dsp_header ch_header[MCADPCM_MAX_CHANNELS];

    /* checks */
    if (!check_extensions(streamFile, "mcadpcm"))
        goto fail;

    /* could validate dsp sizes but only with +1ch, should be done below in check_dsp_samples */
    //if (read_32bitLE(0x08,streamFile) != read_32bitLE(0x10,streamFile))
    //   goto fail;

    channel_count = read_32bitLE(0x00,streamFile);
    if (channel_count > MCADPCM_MAX_CHANNELS) goto fail;

    header_offset =  read_32bitLE(0x04,streamFile);
    header_spacing = channel_count == 1 ? 0 : read_32bitLE(0x0c,streamFile) - header_offset; /* channel 2 start, only with Nch */
    start_offset = header_offset + 0x60;
    interleave = header_spacing;

    /* read dsp */
    if (!dsp_load_header_le(ch_header, channel_count, streamFile,header_offset,header_spacing)) goto fail;
    if (!check_dsp_format(ch_header, channel_count)) goto fail;
    if (!check_dsp_samples(ch_header, channel_count)) goto fail;
    if (!check_dsp_initial_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;
    if (!check_dsp_loop_ps(ch_header, channel_count, streamFile,start_offset,interleave)) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,ch_header[0].loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset)+1;

    vgmstream->meta_type = meta_DSP_MCADPCM;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = channel_count == 1 ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = interleave;
    setup_vgmstream_dsp(vgmstream, ch_header);

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
