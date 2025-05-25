#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


/* If these variables are packed properly in the struct (one after another)
 * then this is actually how they are laid out in the file, albeit big-endian */
typedef struct {
    uint32_t sample_count;      /* 0x00 */
    uint32_t nibble_count;      /* 0x04 (includes frame headers) */
    uint32_t sample_rate;       /* 0x08 (generally 22/32/44/48kz but games like Wario World set 32028hz to adjust for GC's rate) */
    uint16_t loop_flag;         /* 0x0c */
    uint16_t format;            /* 0x0e (always 0 for ADPCM) */
    uint32_t loop_start_offset; /* 0x10 (in nibbles, should be 2 if 0/not set) */
    uint32_t loop_end_offset;   /* 0x14 (in nibbles) */
    uint32_t initial_offset;    /* 0x18 ("ca", in nibbles, should be 2) */
    int16_t coef[16];           /* 0x1c (eight pairs) */
    uint16_t gain;              /* 0x3c (always 0 for ADPCM) */
    uint16_t initial_ps;        /* 0x3e (predictor/scale in frame header) */
    int16_t initial_hist1;      /* 0x40 */
    int16_t initial_hist2;      /* 0x42 */
    uint16_t loop_ps;           /* 0x44 (predictor/scale in loop frame header) */
    int16_t loop_hist1;         /* 0x46 */
    int16_t loop_hist2;         /* 0x48 */
    int16_t channels;           /* 0x4a (DSPADPCM.exe ~v2.7 extension) */
    uint16_t block_size;        /* 0x4c */
    /* padding/reserved up to 0x60, DSPADPCM.exe from GC adds garbage here (uninitialized MSVC memory?)
     * [ex. Batallion Wars (GC), Timesplitters 2 (GC)], 0xcccc...cccc with DSPADPCMD */
} dsp_header_t;

typedef struct {
    bool ignore_null_coefs;         /* silent files in rare cases */
} dsp_header_config_t;

/* read and do basic validations to the above struct */
static bool read_dsp_header_endian(dsp_header_t* header, off_t offset, STREAMFILE* sf, bool big_endian, dsp_header_config_t* cfg) {
    get_u32_t get_u32 = big_endian ? get_u32be : get_u32le;
    get_u16_t get_u16 = big_endian ? get_u16be : get_u16le;
    get_s16_t get_s16 = big_endian ? get_s16be : get_s16le;
    uint8_t buf[0x60];
    int zero_coefs;

    if (offset > get_streamfile_size(sf))
        goto fail;
    if (read_streamfile(buf, offset, 0x60, sf) != 0x60)
        goto fail;

    /* Since header is rather basic add some extra checks. Some validations like samples vs nibbles
     * and loop offsets/expected values should be external, since there are buggy variations */

    /* base */
    header->sample_count        = get_u32(buf+0x00);
    if (header->sample_count > 0x10000000 || header->sample_count == 0)
        goto fail; /* unlikely to be that big, should catch fourccs */

    /* usually nibbles = size*2 in mono, but interleaved stereo or L+R may use nibbles =~ size (or not), so can't
     * easily reject files with more nibbles than data (nibbles may be part of the -R file) without redoing L+R handling */
    header->nibble_count        = get_u32(buf+0x04);
    if (header->nibble_count > 0x20000000 || header->nibble_count == 0)
        goto fail;

    header->sample_rate         = get_u32(buf+0x08);
    if (header->sample_rate < 5000 || header->sample_rate > 48000)
        /* validated later but fail faster (unsure of min) */
        /* lowest known so far is 5000 in Judge Dredd (GC) */
        goto fail;

    /* context */
    header->loop_flag           = get_u16(buf+0x0c);
    if (header->loop_flag != 0 && header->loop_flag != 1)
        goto fail;
    header->format              = get_u16(buf+0x0e);
    if (header->format != 0)
        goto fail;
    header->loop_start_offset   = get_u32(buf+0x10);
    header->loop_end_offset     = get_u32(buf+0x14);

    //TODO: test if games react to changed initial offset
    /* Dr. Muto uses 0, and some custom Metroid Prime loop start, so probably ignored by the hardware */
    header->initial_offset      = get_u32(buf+0x18);
    if (header->initial_offset != 2 && header->initial_offset != 0 && header->initial_offset != header->loop_start_offset)
        goto fail;

    zero_coefs = 0;
    for (int i = 0; i < 16; i++) {
        header->coef[i]         = get_s16(buf+0x1c + i*0x02);
        if (header->coef[i] == 0)
            zero_coefs++;
    }
    /* some 0s are ok, more than 8 is probably wrong, but rarely ok */
    if (cfg == NULL || !cfg->ignore_null_coefs) {
        if (zero_coefs == 16)
            goto fail;
    }

    header->gain                = get_u16(buf+0x3c);
    if (header->gain != 0)
        goto fail;

    /* decoder state (could check that ps <= 0xNN but not that useful) */
    header->initial_ps          = get_u16(buf+0x3e);
    header->initial_hist1       = get_s16(buf+0x40);
    header->initial_hist2       = get_s16(buf+0x42);
    header->loop_ps             = get_u16(buf+0x44);
    header->loop_hist1          = get_s16(buf+0x46);
    header->loop_hist2          = get_s16(buf+0x48);

    /* reserved, may contain garbage */
    header->channels            = get_s16(buf+0x4a);
    header->block_size          = get_s16(buf+0x4c);

    if (header->channels > 64) /* arbitrary max */
        header->channels = 0;
    if (header->block_size >= 0xF000) /* same, 16b (usually 0) */
        header->block_size = 0;

    return true;
fail:
    return false;
}

static int read_dsp_header_be(dsp_header_t *header, off_t offset, STREAMFILE* file) {
    return read_dsp_header_endian(header, offset, file, true, NULL);
}

static int read_dsp_header_le(dsp_header_t *header, off_t offset, STREAMFILE* file) {
    return read_dsp_header_endian(header, offset, file, false, NULL);
}

/* ********************************* */

typedef struct {
    /* basic config */
    int little_endian;
    int channels;
    int max_channels;

    off_t header_offset;            /* standard DSP header */
    size_t header_spacing;          /* distance between DSP header of other channels */
    off_t start_offset;             /* data start */
    size_t interleave;              /* distance between data of other channels */
    size_t interleave_first;        /* same, in the first block */
    size_t interleave_first_skip;   /* extra info */
    size_t interleave_last;         /* same, in the last block */

    meta_t meta_type;

    /* hacks */
    bool force_loop;                /* force full loop */
    bool force_loop_seconds;        /* force loop, but must be longer than this (to catch jingles) */
    bool fix_looping;               /* fix loop end going past num_samples */
    bool fix_loop_start;            /* weird files with bad loop start */
    bool single_header;             /* all channels share header, thus totals are off (2=double) */
    bool double_header;             /* all channels share header, thus totals are off (2=double) */
    bool ignore_header_agreement;   /* sometimes there are minor differences between headers */
    bool ignore_initial_ps;         /* rarely has bad start ps */
    bool ignore_loop_ps;            /* sometimes has bad loop start ps */
    dsp_header_config_t cfg;
} dsp_meta;

#define COMMON_DSP_MAX_CHANNELS 6

/* Common parser for most DSPs that are basically the same with minor changes.
 * Custom variants will just concatenate or interleave standard DSP headers and data,
 * so we make sure to validate read vs expected values, based on dsp_meta config. */
static VGMSTREAM* init_vgmstream_dsp_common(STREAMFILE* sf, dsp_meta* dspm) {
    VGMSTREAM* vgmstream = NULL;
    int i, j;
    int loop_flag;
    dsp_header_t ch_header[COMMON_DSP_MAX_CHANNELS];


    if (dspm->channels > dspm->max_channels)
        return NULL;
    if (dspm->channels > COMMON_DSP_MAX_CHANNELS || dspm->channels < 0)
        return NULL;

    /* load standard DSP header per channel */
    {
        for (i = 0; i < dspm->channels; i++) {
            if (!read_dsp_header_endian(&ch_header[i], dspm->header_offset + i*dspm->header_spacing, sf, !dspm->little_endian, &dspm->cfg)) {
                //;VGM_LOG("DSP: bad header\n");
                return NULL;
            }
        }
    }

    /* fix bad/fixed value in loop start */
    if (dspm->fix_loop_start) {
        for (i = 0; i < dspm->channels; i++) {
            if (ch_header[i].loop_flag)
                ch_header[i].loop_start_offset = 0x00;
        }
    }

    /* check for agreement between channels */
    if (!dspm->ignore_header_agreement) {
        for (i = 0; i < dspm->channels - 1; i++) {
            if (ch_header[i].sample_count != ch_header[i+1].sample_count ||
                ch_header[i].nibble_count != ch_header[i+1].nibble_count ||
                ch_header[i].sample_rate != ch_header[i+1].sample_rate ||
                ch_header[i].loop_flag != ch_header[i+1].loop_flag ||
                ch_header[i].loop_start_offset != ch_header[i+1].loop_start_offset ||
                ch_header[i].loop_end_offset != ch_header[i+1].loop_end_offset) {
                //;VGM_LOG("DSP: bad header agreement\n");
                goto fail;
            }
        }
    }

    /* check expected initial predictor/scale */
    if (!dspm->ignore_initial_ps) {
        int channels = dspm->channels;
        if (dspm->single_header)
            channels = 1;

        for (i = 0; i < channels; i++) {
            off_t channel_offset = dspm->start_offset + i*dspm->interleave;
            if (ch_header[i].initial_ps != read_u8(channel_offset, sf)) {
                //;VGM_LOG("DSP: bad initial ps\n");
                goto fail;
            }
        }
    }

    /* check expected loop predictor/scale */
    if (ch_header[0].loop_flag && !dspm->ignore_loop_ps) {
        int channels = dspm->channels;
        if (dspm->single_header)
            channels = 1;

        for (i = 0; i < channels; i++) {
            off_t loop_offset = ch_header[i].loop_start_offset;

            /* Loop offset points to a nibble, but we need closest frame header.
             * Stereo doesn't always point to a proper offset unless de-adjusted with interleave first. */
            if (!dspm->interleave) {
                loop_offset = loop_offset / 0x08 * 0x8;
            }
            else {
                loop_offset = loop_offset / 0x10 * 0x8;
                loop_offset = (loop_offset / dspm->interleave * dspm->interleave * channels) + (loop_offset % dspm->interleave);
            }

            if (ch_header[i].loop_ps != read_u8(dspm->start_offset + i*dspm->interleave + loop_offset,sf)) {
                //;VGM_LOG("DSP: ch%i bad loop ps: %x vs at %lx\n", i, ch_header[i].loop_ps, dspm->start_offset + i*dspm->interleave + loop_offset);
                goto fail;
            }
        }
    }


    /* all done, must be DSP */

    loop_flag = ch_header[0].loop_flag;
    if (!loop_flag && dspm->force_loop) {
        loop_flag = 1;
        if (dspm->force_loop_seconds &&
                ch_header[0].sample_count < dspm->force_loop_seconds*ch_header[0].sample_rate) {
            loop_flag = 0;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(dspm->channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = ch_header[0].sample_rate;
    vgmstream->num_samples = ch_header[0].sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(ch_header[0].loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(ch_header[0].loop_end_offset) + 1;

    vgmstream->meta_type = dspm->meta_type;
    vgmstream->coding_type = coding_NGC_DSP;
    if (dspm->interleave > 0 && dspm->interleave < 0x08)
        vgmstream->coding_type = coding_NGC_DSP_subint;
    vgmstream->layout_type = layout_interleave;
    if (dspm->interleave == 0 || vgmstream->coding_type == coding_NGC_DSP_subint)
        vgmstream->layout_type = layout_none;
    vgmstream->interleave_block_size = dspm->interleave;
    vgmstream->interleave_first_block_size = dspm->interleave_first;
    vgmstream->interleave_first_skip = dspm->interleave_first_skip;
    vgmstream->interleave_last_block_size = dspm->interleave_last;

    {
        /* set coefs and initial history (usually 0) */
        for (i = 0; i < vgmstream->channels; i++) {
            for (j = 0; j < 16; j++) {
                vgmstream->ch[i].adpcm_coef[j] = ch_header[i].coef[j];
            }
            vgmstream->ch[i].adpcm_history1_16 = ch_header[i].initial_hist1;
            vgmstream->ch[i].adpcm_history2_16 = ch_header[i].initial_hist2;
        }
    }

    /* don't know why, but it does happen */
    if (dspm->fix_looping && vgmstream->loop_end_sample > vgmstream->num_samples)
        vgmstream->loop_end_sample = vgmstream->num_samples;

    if (dspm->double_header) { /* double the samples */
        vgmstream->num_samples /= dspm->channels;
        vgmstream->loop_start_sample /= dspm->channels;
        vgmstream->loop_end_sample /= dspm->channels;
    }


    if (!vgmstream_open_stream(vgmstream, sf, dspm->start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ********************************* */

/* .dsp - standard mono dsp as generated by DSPADPCM.exe */
VGMSTREAM* init_vgmstream_ngc_dsp_std(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    dsp_header_t header;
    const size_t header_size = 0x60;
    off_t start_offset;
    int channels;

    /* checks */
    if (!read_dsp_header_be(&header, 0x00, sf))
        return NULL;

    /* .dsp: standard
     * .adp: Dr. Muto/Battalion Wars (GC), Tale of Despereaux (Wii)
     * (extensionless): Tony Hawk's Downhill Jam (Wii)
     * .wav: PDC World Championship Darts 2009 & Pro Tour (Wii) 
     * .dat: The Sims: Bustin' Out (GC) (rarely, most are extensionless)
     * .rsm: Bully: Scholarship Edition (Wii) (Speech.bin) */
    if (!check_extensions(sf, "dsp,adp,,wav,lwav,dat,ldat,rsm"))
        return NULL;

    channels = 1;
    start_offset = header_size;

    if (header.initial_ps != read_u8(start_offset,sf))
        goto fail;

    /* Check for a matching second header. If we find one and it checks
     * out thoroughly, we're probably not dealing with a genuine mono DSP.
     * In many cases these will pass all the other checks, including the
     * predictor/scale check if the first byte is 0 */
    //TODO: maybe this meta should be after others, so they have a better chance to detect >1ch .dsp
    // (but .dsp is the common case, so it'd be slower)
    {
        int ko;
        dsp_header_t header2;

        /* ignore headers one after another */
        ko = !read_dsp_header_be(&header2, header_size, sf);
        if (!ko &&
                header.sample_count == header2.sample_count &&
                header.nibble_count == header2.nibble_count &&
                header.sample_rate == header2.sample_rate &&
                header.loop_flag == header2.loop_flag) {
            goto fail;
        }


        /* ignore headers after interleave [Ultimate Board Collection (Wii)] */
        ko = !read_dsp_header_be(&header2, 0x10000, sf);
        if (!ko &&
                header.sample_count == header2.sample_count &&
                header.nibble_count == header2.nibble_count &&
                header.sample_rate == header2.sample_rate &&
                header.loop_flag == header2.loop_flag) {
            goto fail;
        }

        /* ignore ddsp, that set samples/nibbles counting both channels so can't be detected
         * (could check for .dsp but most files don't need this) */
        if (check_extensions(sf, "adp,")) {
            uint32_t interleave = (get_streamfile_size(sf) / 2);

            ko = !read_dsp_header_be(&header2, interleave, sf);
            if (!ko &&
                    header.sample_count == header2.sample_count &&
                    header.nibble_count == header2.nibble_count &&
                    header.sample_rate == header2.sample_rate &&
                    header.loop_flag == header2.loop_flag) {
                goto fail;
            }
        }
    }

    if (header.loop_flag) {
        off_t loop_off;
        /* check loop predictor/scale */
        loop_off = header.loop_start_offset/16*8;
        if (header.loop_ps != read_u8(start_offset+loop_off,sf)) {
            /* rarely won't match (ex ESPN 2002), not sure if header or calc problem, but doesn't seem to matter
             *  (there may be a "click" when looping, or loop values may be too big and loop disabled anyway) */
            VGM_LOG("DSP (std): bad loop_predictor\n");
            //header.loop_flag = 0;
            //goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, header.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = header.sample_rate;
    vgmstream->num_samples = header.sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(header.loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(header.loop_end_offset)+1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* don't know why, but it does happen */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_STD;
    vgmstream->allow_dual_stereo = true; /* very common in .dsp */
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;

    {
        /* adpcm coeffs/history */
        for (int i = 0; i < 16; i++) {
            vgmstream->ch[0].adpcm_coef[i] = header.coef[i];
        }
        vgmstream->ch[0].adpcm_history1_16 = header.initial_hist1;
        vgmstream->ch[0].adpcm_history2_16 = header.initial_hist2;
    }

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .dsp - little endian dsp, possibly main Switch .dsp [LEGO Worlds (Switch)] */
VGMSTREAM* init_vgmstream_ngc_dsp_std_le(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    dsp_header_t header;
    const size_t header_size = 0x60;
    off_t start_offset;
    int channels;

    /* checks */
    if (!read_dsp_header_le(&header, 0x00, sf))
        return NULL;
    /* .adpcm: LEGO Worlds */
    if (!check_extensions(sf, "adpcm"))
        return NULL;

    channels = 1;
    start_offset = header_size;

    if (header.initial_ps != read_u8(start_offset,sf))
        goto fail;

    /* Check for a matching second header. If we find one and it checks
     * out thoroughly, we're probably not dealing with a genuine mono DSP.
     * In many cases these will pass all the other checks, including the
     * predictor/scale check if the first byte is 0 */
    {
        dsp_header_t header2;
        int ko;

        ko = !read_dsp_header_le(&header2, header_size, sf);

        if (!ko &&
            header.sample_count == header2.sample_count &&
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
        if (header.loop_ps != read_u8(start_offset+loop_off,sf)) {
            goto fail;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, header.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = header.sample_rate;
    vgmstream->num_samples = header.sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(header.loop_start_offset);
    vgmstream->loop_end_sample   = dsp_nibbles_to_samples(header.loop_end_offset) + 1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* don't know why, but it does happen */
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_STD;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_none;
    vgmstream->allow_dual_stereo = true;

    {
        /* adpcm coeffs/history */
        for (int i = 0; i < 16; i++) {
            vgmstream->ch[0].adpcm_coef[i] = header.coef[i];
        }
        vgmstream->ch[0].adpcm_history1_16 = header.initial_hist1;
        vgmstream->ch[0].adpcm_history2_16 = header.initial_hist2;
    }

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* .dsp - standard multi-channel dsp as generated by DSPADPCM.exe (later revisions) */
VGMSTREAM* init_vgmstream_ngc_mdsp_std(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    dsp_header_t header;
    const size_t header_size = 0x60;
    off_t start_offset;
    int channels;


    /* checks */
    if (!read_dsp_header_be(&header, 0x00, sf))
        return NULL;
    if (!check_extensions(sf, "dsp,mdsp"))
        return NULL;

    channels = header.channels==0 ? 1 : header.channels;
    start_offset = header_size * channels;

    if (header.initial_ps != read_u8(start_offset, sf))
        goto fail;

    /* named .dsp and no channels? likely another interleaved dsp */
    if (check_extensions(sf,"dsp") && header.channels == 0)
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, header.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = header.sample_rate;
    vgmstream->num_samples = header.sample_count;
    vgmstream->loop_start_sample = dsp_nibbles_to_samples(header.loop_start_offset);
    vgmstream->loop_end_sample = dsp_nibbles_to_samples(header.loop_end_offset) + 1;
    if (vgmstream->loop_end_sample > vgmstream->num_samples) /* don't know why, but it does happen*/
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_DSP_STD;
    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = channels == 1 ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = header.block_size * 8;
    if (vgmstream->interleave_block_size)
        vgmstream->interleave_last_block_size = (header.nibble_count / 2 % vgmstream->interleave_block_size + 7) / 8 * 8;

    for (int i = 0; i < channels; i++) {
        if (!read_dsp_header_be(&header, header_size * i, sf))
            goto fail;

        /* adpcm coeffs/history */
        for (int c = 0; c < 16; c++) {
            vgmstream->ch[i].adpcm_coef[c] = header.coef[c];
        }
        vgmstream->ch[i].adpcm_history1_16 = header.initial_hist1;
        vgmstream->ch[i].adpcm_history2_16 = header.initial_hist2;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ********************************* */

/* .stm - Intelligent Systems + others (same programmers) full interleaved dsp [Paper Mario TTYD (GC), Fire Emblem: POR (GC), Cubivore (GC)] */
VGMSTREAM* init_vgmstream_ngc_dsp_stm(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (read_u16be(0x00, sf) != 0x0200)
        goto fail;

    /* .lstm/dsp: renamed to avoid hijacking Scream Tracker 2 Modules (not needed) */
    if (!check_extensions(sf, "stm,lstm,dsp"))
        goto fail;
    /* 0x02: sample rate
     * 0x08+: channel sizes/loop offsets? */

    dspm.channels = read_u32be(0x04, sf);
    dspm.max_channels = 2;
    dspm.fix_looping = 1;

    dspm.header_offset =  0x40;
    dspm.header_spacing = 0x60;
    dspm.start_offset = 0x100;
    dspm.interleave = (read_u32be(0x08, sf) + 0x20) / 0x20 * 0x20; /* strange rounding, but works */

    dspm.meta_type = meta_DSP_STM;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* .STE - single header + interleaved dsp [Monopoly Party! (GC)] */
VGMSTREAM* init_vgmstream_ngc_mpdsp(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    /* .ste: real extension
     * .mpdsp: fake/renamed since standard .dsp would catch it otherwise */
    if (!check_extensions(sf, "mpdsp,ste"))
        goto fail;

    /* at 0x48 is extra data that could help differenciating these DSPs, but seems like
     * memory garbage created by the encoder that other games also have */
    /* 0x02(2): sample rate, 0x08+: channel sizes/loop offsets? */

    dspm.channels = 2;
    dspm.max_channels = 2;
    dspm.single_header = true;
    dspm.double_header = true;

    dspm.header_offset =  0x00;
    dspm.header_spacing = 0x00; /* same header for both channels */
    dspm.start_offset = 0x60;
    dspm.interleave = 0xf000;

    dspm.meta_type = meta_DSP_MPDSP;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* various dsp with differing extensions and interleave values */
VGMSTREAM* init_vgmstream_ngc_dsp_std_int(STREAMFILE* sf) {
    dsp_meta dspm = {0};
    char filename[PATH_LIMIT];

    /* checks */
    if (!check_extensions(sf, "dsp,mss,gcm"))
        goto fail;

    dspm.channels = 2;
    dspm.max_channels = 2;
    dspm.fix_looping = 1;

    dspm.header_offset  = 0x00;
    dspm.header_spacing = 0x60;
    dspm.start_offset = 0xc0;

    sf->get_name(sf,filename,sizeof(filename));
    if (strlen(filename) > 7 && !strcasecmp("_lr.dsp",filename+strlen(filename)-7)) { //todo improve
        dspm.interleave = 0x14180;
        dspm.meta_type = meta_DSP_JETTERS; /* Bomberman Jetters (GC) */
    } else if (check_extensions(sf, "mss")) {
        dspm.interleave = 0x1000;
        dspm.meta_type = meta_DSP_MSS; /* Free Radical GC games */
        /* Timesplitters 2 GC's ts2_atom_smasher_44_fx.mss differs slightly in samples but plays ok */
        dspm.ignore_header_agreement = 1;
    } else if (check_extensions(sf, "gcm")) {
        /* older Traveller's Tales games [Lego Star Wars (GC), The Chronicles of Narnia (GC), Sonic R (GC)] */
        dspm.interleave = 0x8000;
        dspm.meta_type = meta_DSP_GCM;
    } else {
        goto fail;
    }

    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* IDSP - Namco header (from NUB/NUS3) + interleaved dsp [SSB4 (3DS), Tekken Tag Tournament 2 (WiiU)] */
VGMSTREAM* init_vgmstream_idsp_namco(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!is_id32be(0x00,sf, "IDSP"))
        return NULL;

    if (!check_extensions(sf, "idsp"))
        return NULL;

    dspm.max_channels = 8;
    /* games do adjust loop_end if bigger than num_samples (only happens in user-created IDSPs) */
    dspm.fix_looping = 1;

    /* 0x04: null */
    dspm.channels = read_s32be(0x08, sf);
    /* 0x0c: sample rate */
    /* 0x10: num_samples */
    /* 0x14: loop start */
    /* 0x18: loop end */
    dspm.interleave = read_u32be(0x1c,sf); /* usually 0x10 */
    dspm.header_offset = read_u32be(0x20,sf);
    dspm.header_spacing = read_u32be(0x24,sf);
    dspm.start_offset = read_u32be(0x28,sf);

    /* SoulCalibur Legends (Wii), Taiko no Tatsujin: Atsumete Tomodachi Daisakusen (WiiU) */
    if (dspm.interleave == 0)  {
        /* half interleave (uncommon), use channel size */
        dspm.interleave = read_u32be(0x2c,sf);
        /* Rarely 2nd channel stars with a padding frame then real 2nd channel with initial_ps. Must be some NUS2 bug
         * when importing DSP data as only happens for some subsongs and offsets/sizes are fine [We Ski (Wii), Go Vacation (Wii)] */
        dspm.ignore_initial_ps = true;
        dspm.ignore_loop_ps = true;
    }

    // rare but valid IDSP [Super Smash Bros. Ultimate (Switch)-vc_kirby.nus3audio]
    dspm.cfg.ignore_null_coefs = true;

    dspm.meta_type = meta_IDSP_NAMCO;
    return init_vgmstream_dsp_common(sf, &dspm);
}


/* sadb - Procyon Studio header + interleaved dsp [Shiren the Wanderer 3 (Wii), Disaster: Day of Crisis (Wii)] */
VGMSTREAM* init_vgmstream_sadb(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!is_id32be(0x00,sf, "sadb"))
        goto fail;

    if (!check_extensions(sf, "sad"))
        goto fail;

    dspm.channels = read_8bit(0x32, sf);
    dspm.max_channels = 2;

    dspm.header_offset =  0x80;
    dspm.header_spacing = 0x60;
    dspm.start_offset = read_32bitBE(0x48,sf);
    dspm.interleave = 0x10;

    dspm.meta_type = meta_DSP_SADB;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* IDSP - Traveller's Tales header + interleaved dsps [Lego Batman (Wii), Lego Dimensions (Wii U)] */
VGMSTREAM* init_vgmstream_idsp_tt(STREAMFILE* sf) {
    dsp_meta dspm = {0};
    int version_main, version_sub;

    /* checks */
    if (!is_id32be(0x00,sf, "IDSP"))
        goto fail;

    /* .gcm: standard
     * .idsp: header id?
     * .wua: Lego Dimensions (Wii U) */
    if (!check_extensions(sf, "gcm,idsp,wua"))
        goto fail;

    version_main = read_u32be(0x04, sf);
    version_sub  = read_u32be(0x08, sf); /* extra check since there are other IDSPs */
    if (version_main == 0x01 && version_sub == 0xc8) {
        /* Transformers: The Game (Wii) */
        dspm.channels = 2;
        dspm.max_channels = 2;
        dspm.header_offset = 0x10;
    }
    else if (version_main == 0x02 && version_sub == 0xd2) {
        /* Lego Batman (Wii)
         * The Chronicles of Narnia: Prince Caspian (Wii)
         * Lego Indiana Jones 2 (Wii)
         * Lego Star Wars: The Complete Saga (Wii)
         * Lego Pirates of the Caribbean (Wii)
         * Lego Harry Potter: Years 1-4 (Wii) */
        dspm.channels = 2;
        dspm.max_channels = 2;
        dspm.header_offset = 0x20;
        /* 0x10+: null */
    }
    else if (version_main == 0x03 && version_sub == 0x12c) {
        /* Lego The Lord of the Rings (Wii) */
        /* Lego Dimensions (Wii U) */
        dspm.channels = read_u32be(0x10, sf);
        dspm.max_channels = 2;
        dspm.header_offset = 0x20;
        /* 0x14+: "I_AM_PADDING" */
    }
    else {
        goto fail;
    }

    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + 0x60 * dspm.channels;
    dspm.interleave = read_u32be(0x0c, sf);

    dspm.meta_type = meta_IDSP_TT;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* IDSP - from Next Level games [Super Mario Strikers (GC), Mario Strikers Charged (Wii), Spider-Man: Friend or Foe (Wii)] */
VGMSTREAM* init_vgmstream_idsp_nl(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!is_id32be(0x00,sf, "IDSP"))
        goto fail;
    if (!check_extensions(sf, "idsp"))
        goto fail;

    dspm.channels = 2;
    dspm.max_channels = 2;

    dspm.header_offset =  0x0c;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing*dspm.channels;
    dspm.interleave = read_32bitBE(0x04,sf);
    /* 0x08: usable channel size */
    {
        size_t stream_size = get_streamfile_size(sf);
        if (read_32bitBE(stream_size - 0x04,sf) == 0x30303030)
            stream_size -= 0x14; /* remove padding */
        stream_size -= dspm.start_offset;

        if (dspm.interleave)
            dspm.interleave_last = (stream_size / dspm.channels) % dspm.interleave;
    }

    dspm.fix_looping = 1;
    dspm.force_loop = 1;
    dspm.force_loop_seconds = 15;

    dspm.meta_type = meta_IDSP_NL;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* .wsd - Custom header + full interleaved dsp [Phantom Brave (Wii)] */
VGMSTREAM* init_vgmstream_wii_wsd(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (read_u32be(0x00,sf) != 0x20)
        goto fail;
    if (!check_extensions(sf, "wsd"))
        goto fail;
    if (read_u32be(0x08,sf) != read_u32be(0x0c,sf)) /* channel sizes */
        goto fail;

    dspm.channels = 2;
    dspm.max_channels = 2;

    dspm.header_offset =  read_u32be(0x00,sf);
    dspm.header_spacing = read_u32be(0x04,sf) - dspm.header_offset;
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_WII_WSD;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* .ddsp - full interleaved dsp [Shark Tale (GC), The Sims series (GC/Wii), Wacky Races: Crash & Dash (Wii)] */
VGMSTREAM* init_vgmstream_dsp_ddsp(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    /* .adp: Tale of Despereaux (Wii) */
    /* .ddsp: fake extension (games have bigfiles without names, but has references to .wav)
     * .wav: Wacky Races: Crash & Dash (Wii)
     * (extensionless): The Sims series (GC/Wii) */
    if (!check_extensions(sf, "adp,ddsp,wav,lwav,"))
        goto fail;

    dspm.channels = 2;
    dspm.max_channels = 2;

    dspm.header_offset = 0x00;
    dspm.header_spacing = (get_streamfile_size(sf) / dspm.channels);
    dspm.start_offset = 0x60;
    dspm.interleave = dspm.header_spacing;

    /* this format has nibbles in both headers matching all data (not just for that channel),
     * and interleave is exact half even for files that aren't aligned to 0x10 */

    dspm.meta_type = meta_DSP_DDSP;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* iSWS - Sumo Digital header + interleaved dsp [DiRT 2 (Wii), F1 2009 (Wii)] */
VGMSTREAM* init_vgmstream_wii_was(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!is_id32be(0x00,sf, "iSWS"))
        goto fail;
    if (!check_extensions(sf, "was,dsp,isws"))
        goto fail;

    dspm.channels = read_32bitBE(0x08,sf);
    dspm.max_channels = 2;

    dspm.header_offset = 0x08 + read_32bitBE(0x04,sf);
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channels*dspm.header_spacing;
    dspm.interleave = read_32bitBE(0x10,sf);

    dspm.meta_type = meta_WII_WAS;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* .str - Infogrames raw interleaved dsp [Micro Machines (GC), Superman: Shadow of Apokolips (GC)] */
VGMSTREAM* init_vgmstream_dsp_str_ig(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(sf, "str"))
        goto fail;

    dspm.channels = 2;
    dspm.max_channels = 2;

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x80;
    dspm.start_offset = 0x800;
    dspm.interleave = 0x4000;

    dspm.meta_type = meta_DSP_STR_IG;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* .dsp - Ubisoft interleaved dsp with bad loop start [Speed Challenge: Jacques Villeneuve's Racing Vision (GC), XIII (GC)] */
VGMSTREAM* init_vgmstream_dsp_xiii(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(sf, "dsp"))
        goto fail;

    dspm.channels = 2;
    dspm.max_channels = 2;
    dspm.fix_loop_start = 1; /* loop flag but strange loop start instead of 0 (maybe shouldn't loop) */

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing * dspm.channels;
    dspm.interleave = 0x08;

    dspm.meta_type = meta_DSP_XIII;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* NPD - Icon Games header + subinterleaved DSPs [Vertigo (Wii), Build n' Race (Wii)] */
VGMSTREAM* init_vgmstream_dsp_ndp(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!is_id32be(0x00,sf, "NDP\0"))
        goto fail;
    /* .nds: standard
     * .ndp: header id */
    if (!check_extensions(sf, "nds,ndp"))
        goto fail;
    if (read_u32le(0x08,sf) + 0x18 != get_streamfile_size(sf))
        goto fail;
    /* 0x0c: sample rate */

    dspm.channels = read_u32le(0x10,sf);
    dspm.max_channels = 2;

    dspm.header_offset = 0x18;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channels*dspm.header_spacing;
    dspm.interleave = 0x04;
    dspm.ignore_loop_ps = 1; /* some files loops from 0 but loop ps is null */

    dspm.meta_type = meta_WII_NDP;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* Cabela's series (Magic Wand dev?) - header + interleaved dsp
 *  [Cabela's Big Game Hunt 2005 Adventures (GC), Cabela's Outdoor Adventures (GC)] */
VGMSTREAM* init_vgmstream_dsp_cabelas(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(sf, "dsp"))
        goto fail;
    /* has extra stuff in the reserved data, without it this meta may catch other DSPs it shouldn't */
    if (read_32bitBE(0x50,sf) == 0 || read_32bitBE(0x54,sf) == 0)
        goto fail;

    /* sfx are mono, but standard dsp will catch them tho */
    dspm.channels = read_32bitBE(0x00,sf) == read_32bitBE(0x60,sf) ? 2 : 1;
    dspm.max_channels = 2;
    dspm.force_loop = (dspm.channels > 1);

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channels*dspm.header_spacing;
    dspm.interleave = 0x10;

    dspm.meta_type = meta_DSP_CABELAS;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* AAAp - Acclaim Austin Audio header + interleaved dsp [Vexx (GC), Turok: Evolution (GC)] */
VGMSTREAM* init_vgmstream_ngc_dsp_aaap(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!is_id32be(0x00,sf, "AAAp"))
        goto fail;
    if (!check_extensions(sf, "dsp"))
        goto fail;


    dspm.interleave = read_u16be(0x04,sf);
    dspm.channels = read_u16be(0x06,sf);
    dspm.max_channels = 2;

    dspm.header_offset = 0x08;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.channels * dspm.header_spacing;

    dspm.meta_type = meta_NGC_DSP_AAAP;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* DSPW - Capcom header + full interleaved DSP [Sengoku Basara 3 (Wii), Monster Hunter 3 Ultimate (WiiU)] */
VGMSTREAM* init_vgmstream_dsp_dspw(STREAMFILE* sf) {
    dsp_meta dspm = {0};
    size_t data_size;

    /* checks */
    if (!is_id32be(0x00,sf, "DSPW"))
        goto fail;
    if (!check_extensions(sf, "dspw"))
        goto fail;

    /* ignore time marker */
    data_size = read_32bitBE(0x08, sf);
    if (is_id32be(data_size - 0x10, sf, "tIME"))
        data_size -= 0x10; /* (ignore, 2 ints in YYYYMMDD hhmmss00) */

    /* some files have a mrkr section with multiple loop regions added at the end (variable size) */
    {
        off_t mrkr_offset = data_size - 0x04;
        off_t max_offset = data_size - 0x1000;
        while (mrkr_offset > max_offset) {
            if (read_32bitBE(mrkr_offset, sf) != 0x6D726B72) { /* "mrkr" */
                mrkr_offset -= 0x04;
            } else {
                data_size = mrkr_offset;
                break;
            }
        }
    }
    data_size -= 0x20; /* header size */
    /* 0x10: loop start, 0x14: loop end, 0x1c: num_samples */

    dspm.channels = read_32bitBE(0x18, sf);
    dspm.max_channels = 6; /* 6ch in Monster Hunter 3 Ultimate */

    dspm.header_offset = 0x20;
    dspm.header_spacing = data_size / dspm.channels;
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = data_size / dspm.channels;

    dspm.meta_type = meta_DSP_DSPW;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* iadp - custom header + interleaved dsp [Dr. Muto (GC)] */
VGMSTREAM* init_vgmstream_ngc_dsp_iadp(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!is_id32be(0x00,sf, "iadp"))
        goto fail;

    /* .adp: actual extension
     * .iadp: header id */
    if (!check_extensions(sf, "adp,iadp"))
        goto fail;

    dspm.channels = read_32bitBE(0x04,sf);
    dspm.max_channels = 2;

    dspm.header_offset = 0x20;
    dspm.header_spacing = 0x60;
    dspm.start_offset = read_32bitBE(0x1C,sf);
    dspm.interleave = read_32bitBE(0x08,sf);

    dspm.meta_type = meta_NGC_DSP_IADP;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* .mcadpcm - Custom header + full interleaved dsp [Skyrim (Switch)] */
VGMSTREAM* init_vgmstream_dsp_mcadpcm(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(sf, "mcadpcm"))
        goto fail;
    /* could validate dsp sizes but only for +1ch, check_dsp_samples will do it anyway */
    //if (read_32bitLE(0x08,sf) != read_32bitLE(0x10,sf))
    //   goto fail;

    dspm.channels = read_32bitLE(0x00,sf);
    dspm.max_channels = 2;
    dspm.little_endian = 1;

    dspm.header_offset =  read_32bitLE(0x04,sf);
    dspm.header_spacing = dspm.channels == 1 ? 0 :
        read_32bitLE(0x0c,sf) - dspm.header_offset; /* channel 2 start, only with Nch */
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_MCADPCM;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* .switch_audio - UE4 standard LE header + full interleaved dsp [Gal Gun 2 (Switch)] */
VGMSTREAM* init_vgmstream_dsp_switch_audio(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    /* .switch_audio: possibly UE4 class name rather than extension
     * .dsp: assumed */
    if (!check_extensions(sf, "switch_audio,dsp"))
        goto fail;

    /* manual double header test */
    //todo improve to read after first header
    if (read_32bitLE(0x00, sf) == read_32bitLE(get_streamfile_size(sf) / 2, sf))
        dspm.channels = 2;
    else
        dspm.channels = 1;
    dspm.max_channels = 2;
    dspm.little_endian = 1;

    dspm.header_offset = 0x00;
    dspm.header_spacing = get_streamfile_size(sf) / dspm.channels;
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_SWITCH_AUDIO;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}

/* .itl - from Chanrinko Hero (GC) */
VGMSTREAM* init_vgmstream_dsp_itl_ch(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!check_extensions(sf, "itl"))
        goto fail;

    dspm.channels = 2;
    dspm.max_channels = 2;

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing * dspm.channels;
    dspm.interleave = 0x23C0;

    dspm.fix_looping = 1;

    dspm.meta_type = meta_DSP_ITL;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* ADPY - AQUASTYLE wrapper [Touhou Genso Wanderer -Reloaded- (Switch)] */
VGMSTREAM* init_vgmstream_dsp_adpy(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!is_id32be(0x00,sf, "ADPY"))
        goto fail;

    if (!check_extensions(sf, "adpcmx"))
        goto fail;

    /* 0x04(2): 1? */
    /* 0x08: some size? */
    /* 0x0c: null */

    dspm.channels = read_u16le(0x06,sf);
    dspm.max_channels = 2;
    dspm.little_endian = 1;

    dspm.header_offset = 0x10;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing*dspm.channels;
    dspm.interleave = 0x08;

    dspm.meta_type = meta_DSP_ADPY;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* ADPX - AQUASTYLE wrapper [Fushigi no Gensokyo: Lotus Labyrinth (Switch)] */
VGMSTREAM* init_vgmstream_dsp_adpx(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!is_id32be(0x00,sf, "ADPX"))
        goto fail;

    if (!check_extensions(sf, "adpcmx"))
        goto fail;

    /* from 0x04 *6 are probably channel sizes, so max would be 6ch; this assumes 2ch */
    if (read_32bitLE(0x04,sf) != read_32bitLE(0x08,sf) &&
        read_32bitLE(0x0c,sf) != 0)
        goto fail;
    dspm.channels = 2;
    dspm.max_channels = 2;
    dspm.little_endian = 1;

    dspm.header_offset = 0x1c;
    dspm.header_spacing = read_32bitLE(0x04,sf);
    dspm.start_offset = dspm.header_offset + 0x60;
    dspm.interleave = dspm.header_spacing;

    dspm.meta_type = meta_DSP_ADPX;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* .ds2 - LucasArts wrapper [Star Wars: Bounty Hunter (GC)] */
VGMSTREAM* init_vgmstream_dsp_lucasarts_ds2(STREAMFILE* sf) {
    dsp_meta dspm = {0};
    size_t file_size, channel_offset;

    /* checks */
    /* .ds2: real extension, dsp: fake/renamed */
    if (!check_extensions(sf, "ds2,dsp"))
        goto fail;
    if (!(read_32bitBE(0x50,sf) == 0 &&
          read_32bitBE(0x54,sf) == 0 &&
          read_32bitBE(0x58,sf) == 0 &&
          read_32bitBE(0x5c,sf) != 0))
        goto fail;

    file_size = get_streamfile_size(sf);
    channel_offset = read_32bitBE(0x5c,sf);  /* absolute offset to 2nd channel */
    if (channel_offset < file_size / 2 || channel_offset > file_size) /* just to make sure */
        goto fail;

    dspm.channels = 2;
    dspm.max_channels = 2;
    dspm.single_header = true;

    dspm.header_offset = 0x00;
    dspm.header_spacing = 0x00;
    dspm.start_offset = 0x60;
    dspm.interleave = channel_offset - dspm.start_offset;

    dspm.meta_type = meta_DSP_DS2;
    return init_vgmstream_dsp_common(sf, &dspm);
fail:
    return NULL;
}


/* .itl - Incinerator Studios interleaved dsp [Cars Race-o-rama (Wii), MX vs ATV Untamed (Wii)] */
VGMSTREAM* init_vgmstream_dsp_itl(STREAMFILE* sf) {
    dsp_meta dspm = {0};
    size_t stream_size;

    /* checks */
    /* .itl: standard
     * .dsp: default to catch a similar file, not sure which devs */
    if (!check_extensions(sf, "itl,dsp"))
        return NULL;

    stream_size = get_streamfile_size(sf);
    dspm.channels = 2;
    dspm.max_channels = 2;

    dspm.start_offset = 0x60;
    dspm.interleave = 0x10000;
    dspm.interleave_first_skip = dspm.start_offset;
    dspm.interleave_first = dspm.interleave - dspm.interleave_first_skip;
    dspm.interleave_last = (stream_size / dspm.channels) % dspm.interleave;
    dspm.header_offset = 0x00;
    dspm.header_spacing = dspm.interleave;

    //todo some files end in half a frame and may click at the very end
    //todo when .dsp should refer to Ultimate Board Collection (Wii), not sure about dev
    dspm.meta_type = meta_DSP_ITL_i;
    return init_vgmstream_dsp_common(sf, &dspm);
}


/* .wav - Square Enix wrapper [Dragon Quest I-III (Switch)] */
VGMSTREAM* init_vgmstream_dsp_sqex(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (read_u32be(0x00,sf) != 0x00000000)
        return NULL;
    if (!check_extensions(sf, "wav,lwav"))
        return NULL;

    dspm.channels = read_u32le(0x04,sf);
    dspm.header_offset = read_u32le(0x08,sf);
    /* 0x0c: channel size */
    dspm.start_offset = dspm.header_offset + 0x60;

    if (dspm.channels > 1) {
        dspm.interleave = read_u32le(0x10,sf) - dspm.header_offset;
        dspm.header_spacing = dspm.interleave;
    }


    dspm.max_channels = 2;
    dspm.little_endian = 1;

    dspm.meta_type = meta_DSP_SQEX;
    return init_vgmstream_dsp_common(sf, &dspm);
}


/* WiiVoice - Koei Tecmo wrapper [Fatal Frame 5 (WiiU)] */
VGMSTREAM* init_vgmstream_dsp_wiivoice(STREAMFILE* sf) {
    dsp_meta dspm = {0};
    /* also see g1l.c for WiiBGM weirder variation */

    /* checks */
    if (!is_id64be(0x00,sf, "WiiVoice"))
        return NULL;
    /* .dsp: assumed */
    if (!check_extensions(sf, "dsp"))
        return NULL;

    dspm.channels = 1;
    dspm.max_channels = 1;

    dspm.header_offset = read_u32be(0x08,sf);
    /* 0x10: file size */
    /* 0x14: data size */
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing*dspm.channels;

    dspm.meta_type = meta_DSP_WIIVOICE;
    return init_vgmstream_dsp_common(sf, &dspm);
}


/* WIIADPCM - Exient wrapper [Need for Speed: Hot Pursuit (Wii), Angry Birds: Star Wars (Wii/WiiU)] */
VGMSTREAM* init_vgmstream_dsp_wiiadpcm(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!is_id64be(0x00,sf, "WIIADPCM"))
        return NULL;
    if (!check_extensions(sf, "adpcm"))
        return NULL;

    // no good flag so use v2's loop+type as other values are easy to mistake
    int test = read_u32be(0x2c,sf);
    if (!(test == 0x00010000 || test == 0x00000000)) {
        // V1 (NFSHP)
        // 08: ch2 offset
        // 0c: real interleave in streams (xN WIIADPCM headers), null in memory audio (xN DSP headers)
        dspm.header_offset = 0x10;
        dspm.max_channels = 2;
    }
    else {
        // V2 (ABSW)
        // 08-18: chN offset
        // 1c: real interleave in streams (xN WIIADPCM headers), null in memory audio (xN DSP headers)
        //     (interleave may be set in mono too)
        dspm.header_offset = 0x20;
        dspm.max_channels = 6;
    }

    dspm.channels = 1;
    for (int i = 0; i < dspm.max_channels - 1; i++) {
        uint32_t offset = read_u32be(0x08 + i * 0x04, sf);
        if (!offset)
            break;
        dspm.channels += 1;
    }

    dspm.interleave = read_u32be(0x08,sf); // use first channel offset as interleave
    if (dspm.interleave)
        dspm.interleave -= dspm.header_offset;
    dspm.interleave_first_skip = 0x60 + dspm.header_offset;
    dspm.interleave_first = dspm.interleave - dspm.interleave_first_skip;

    dspm.header_spacing = dspm.interleave;
    dspm.start_offset = dspm.header_offset + 0x60;

    dspm.meta_type = meta_DSP_WIIADPCM;
    return init_vgmstream_dsp_common(sf, &dspm);
}


/* CWAC - CRI wrapper [Mario & Sonic at the Rio 2016 Olympic Games (WiiU)] */
VGMSTREAM* init_vgmstream_dsp_cwac(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (!is_id32be(0x00,sf, "CWAC"))
        return NULL;

    /* .dsp: assumed */
    if (!check_extensions(sf, "dsp"))
        return NULL;

    dspm.channels       = read_u16be(0x04,sf);
    dspm.header_offset  = read_u32be(0x08,sf);
    dspm.interleave     = read_u32be(0x0c,sf) - dspm.header_offset;

    dspm.max_channels = 2;
    dspm.header_spacing = dspm.interleave;
    dspm.start_offset = dspm.header_offset + 0x60;

    dspm.meta_type = meta_DSP_CWAC;
    return init_vgmstream_dsp_common(sf, &dspm);
}


/* .idsp - interleaved dsp [Harvest Moon: Another Wonderful Life (GC)] */
VGMSTREAM* init_vgmstream_idsp_tose(STREAMFILE* sf) {
    dsp_meta dspm = {0};
    uint32_t blocks;

    /* checks */
    if (read_u32be(0x00,sf) != 0)
        return NULL;
    if (!check_extensions(sf, "idsp"))
        return NULL;

    dspm.max_channels = 4; /* mainly stereo */

    /* 0x04: format? */
    dspm.channels   = read_u16be(0x06,sf);
    dspm.interleave = read_u32be(0x08,sf);
    blocks          = read_u32be(0x0c,sf);

    dspm.header_offset = 0x40;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing * dspm.channels;

    if (dspm.start_offset + dspm.interleave * dspm.channels * blocks != get_streamfile_size(sf))
        return NULL;

    dspm.meta_type = meta_IDSP_TOSE;
    return init_vgmstream_dsp_common(sf, &dspm);
}


/* .KWA - interleaved dsp [Knight Wars prototype (Wii)] */
VGMSTREAM* init_vgmstream_dsp_kwa(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    /* checks */
    if (read_u32be(0x00,sf) != 3)
        return NULL;

    if (!check_extensions(sf, "kwa"))
        return NULL;

    dspm.max_channels   = 4;

    dspm.channels       = read_u32be(0x04,sf);
    dspm.interleave     = read_u32be(0x0c,sf);

    dspm.header_offset  = 0x20;
    dspm.header_spacing = dspm.interleave;
    dspm.start_offset = dspm.header_offset + 0x60;

    dspm.interleave_first_skip = 0x60;
    dspm.interleave_first = dspm.interleave - dspm.interleave_first_skip;

    dspm.ignore_header_agreement = 1; /* Reus_2.kwa has a few more samples in channels 3+4 */

    dspm.meta_type = meta_DSP_KWA;
    return init_vgmstream_dsp_common(sf, &dspm);
}


/* APEX - interleaved dsp [Ninja Gaiden 3 Razor's Edge (WiiU)] */
VGMSTREAM* init_vgmstream_dsp_apex(STREAMFILE* sf) {
    dsp_meta dspm = {0};
    uint32_t stream_size;

    /* checks */
    if (!is_id32be(0x00,sf, "APEX"))
        return NULL;

    /* .dsp: assumed */
    if (!check_extensions(sf, "dsp"))
        return NULL;

    dspm.max_channels   = 2;
    stream_size         = read_u32be(0x04,sf);
    /* 0x08: 1? */
    dspm.channels       = read_u16be(0x0a,sf);
    /* 0x0c: channel size? */

    dspm.interleave     = 0x08;
    dspm.header_offset  = 0x20;
    dspm.header_spacing = 0x60;
    dspm.start_offset = dspm.header_offset + dspm.header_spacing * 2;
    /* second DSP header exists even for mono files, but has no coefs */

    dspm.interleave_last = (stream_size / dspm.channels) % dspm.interleave;

    dspm.meta_type = meta_DSP_APEX;
    return init_vgmstream_dsp_common(sf, &dspm);
}


/* DSP - Rebellion Developments (Asura engine) games */
VGMSTREAM* init_vgmstream_dsp_asura(STREAMFILE* sf) {
    dsp_meta dspm = {0};
    off_t start_offset;
    size_t data_size;
    uint8_t flag;

    /* checks */
    /* "DSP\x00" (GC), "DSP\x01" (GC/Wii/WiiU), "DSP\x02" (WiiU) */
    if ((read_u32be(0x00, sf) & 0xFFFFFF00) != get_id32be("DSP\0"))
        return NULL;
    if (read_u8(0x03, sf) < 0x00 || read_u8(0x03, sf) > 0x02)
        return NULL;

    /* .dsp: Judge Dredd (GC)
     * .wav: Judge Dredd (GC), The Simpsons Game (Wii), Sniper Elite V2 (WiiU) */
    if (!check_extensions(sf, "dsp,wav,lwav"))
        return NULL;

    /* flag set to 0x00 so far only seen in Judge Dredd, which also uses 0x01.
     * at first assumed being 0 means it has a stream name at 0x48 (unlikely) */
    /* flag set to 0x02 means it's ddsp-like stereo */
    flag = read_u8(0x03, sf);
    /* GC/Wii games are all just standard DSP with an id string */
    /* Sniper Elite V2 (WiiU) added a filesize value in the header 
     * and has extra garbage 0xCD bytes at the end for alignment */
    start_offset = 0x04;

    data_size = read_u32be(start_offset, sf);
    /* stereo flag should only occur on the WiiU, Wii uses .ds2 or .sfx (ngc_dsp_asura) */
    if (align_size_to_block(data_size + 0x08, 0x04) == get_streamfile_size(sf) || (flag == 0x02 &&
        align_size_to_block(data_size * 2 + 0x0C, 0x04) == get_streamfile_size(sf)))
        start_offset = 0x08;

    dspm.channels = 1;
    dspm.max_channels = 1;

    if (flag == 0x02) { /* channels are not aligned */
        if (read_u32be(data_size + 0x08, sf) != data_size)
            return NULL; /* size should match */

        dspm.channels = 2;
        dspm.max_channels = 2;
        dspm.header_spacing = data_size + 0x04;
        dspm.interleave = dspm.header_spacing;
    }

    dspm.header_offset = start_offset + 0x00;
    dspm.start_offset = start_offset + 0x60;

    dspm.meta_type = meta_DSP_ASURA;
    return init_vgmstream_dsp_common(sf, &dspm);
}


/* .ds2 - Rebellion (Asura engine) [PDC World Championship Darts 2009 & Pro Tour (Wii)] */
VGMSTREAM* init_vgmstream_dsp_asura_ds2(STREAMFILE* sf) {
    dsp_meta dspm = {0};

    if (!check_extensions(sf, "ds2"))
        return NULL;

    dspm.channels = 2;
    dspm.max_channels = 2;
    dspm.interleave = 0x8000;

    dspm.header_offset = 0x00;
    dspm.start_offset = 0x60;

    dspm.header_spacing = dspm.interleave;
    dspm.interleave_first_skip = dspm.start_offset;
    dspm.interleave_first = dspm.interleave - dspm.interleave_first_skip;

    dspm.meta_type = meta_DSP_ASURA;
    return init_vgmstream_dsp_common(sf, &dspm);
}


/* TTSS - Rebellion (Asura engine) [Sniper Elite series (NSW)] */
VGMSTREAM* init_vgmstream_dsp_asura_ttss(STREAMFILE* sf) {
    dsp_meta dspm = {0};
    size_t header_size = 0x0C;
    size_t ch1_size, ch2_size;

    /* checks */
    if (!is_id32be(0x00, sf, "TTSS"))
        return NULL;

    /* .adpcm: Sniper Elite V2 Remaster (NSW), Sniper Elite 4 (NSW)
     * .wav: Sniper Elite V2 Remaster (NSW), Sniper Elite 3 (NSW), Sniper Elite 4 (NSW) */
    if (!check_extensions(sf, "adpcm,wav,lwav"))
        return NULL;

    /* ch2_size is 0 if mono, otherwise they should match */
    ch1_size = read_u32le(0x04, sf);
    ch2_size = read_u32le(0x08, sf);

    /* as with WiiU Asura DSPx, files are (sometimes) aligned to 0x04 with garbage 0xCD bytes */
    if (header_size + ch1_size + ch2_size != get_streamfile_size(sf) &&
        align_size_to_block(header_size + ch1_size + ch2_size, 0x04) != get_streamfile_size(sf))
        return NULL;

    dspm.channels = 1;
    dspm.max_channels = 1;
    dspm.little_endian = 1;

    if (ch2_size != 0x00) {
        if (ch1_size != ch2_size)
            return NULL;

        dspm.channels = 2;
        dspm.max_channels = 2;
        dspm.header_spacing = ch1_size;
        dspm.interleave = dspm.header_spacing;
    }

    dspm.header_offset = header_size + 0x00;
    dspm.start_offset = header_size + 0x60;

    dspm.meta_type = meta_DSP_ASURA;
    return init_vgmstream_dsp_common(sf, &dspm);
}
