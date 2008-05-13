#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "vgmstream.h"
#include "meta/meta.h"
#include "layout/layout.h"
#include "coding/coding.h"


/*
 * List of functions that will recognize files. These should correspond pretty
 * directly to the metadata types
 */
#define INIT_VGMSTREAM_FCNS 20
VGMSTREAM * (*init_vgmstream_fcns[INIT_VGMSTREAM_FCNS])(const char * const) = {
    init_vgmstream_adx,             /* 0 */
    init_vgmstream_brstm,           /* 1 */
    init_vgmstream_nds_strm,        /* 2 */
    init_vgmstream_agsc,            /* 3 */
    init_vgmstream_ngc_adpdtk,      /* 4 */
    init_vgmstream_rsf,             /* 5 */
    init_vgmstream_afc,             /* 6 */
    init_vgmstream_ast,             /* 7 */
    init_vgmstream_halpst,          /* 8 */
    init_vgmstream_rs03,            /* 9 */
    init_vgmstream_ngc_dsp_std,     /* 10 */
    init_vgmstream_Cstr,            /* 11 */
    init_vgmstream_gcsw,            /* 12 */
    init_vgmstream_ps2_ads,         /* 13 */
	init_vgmstream_ps2_npsf,        /* 14 */
    init_vgmstream_rwsd,            /* 15 */
	init_vgmstream_cdxa,            /* 16 */
	init_vgmstream_ps2_rxw,         /* 17 */
	init_vgmstream_ps2_int,         /* 18 */
    init_vgmstream_ngc_dsp_stm,     /* 19 */
};


/* format detection and VGMSTREAM setup, uses default parameters */
VGMSTREAM * init_vgmstream(const char * const filename) {
    return init_vgmstream_internal(filename,
            1   /* do dual file detection */
            );
}

/* internal version with all parameters */
VGMSTREAM * init_vgmstream_internal(const char * const filename, int do_dfs) {
    int i;

    /* try a series of formats, see which works */
    for (i=0;i<INIT_VGMSTREAM_FCNS;i++) {
        VGMSTREAM * vgmstream = (init_vgmstream_fcns[i])(filename);
        if (vgmstream) {
            /* these are little hacky checks */

            /* everything should have a reasonable sample rate
             * (a verification of the metadata) */
            if (!check_sample_rate(vgmstream->sample_rate)) {
                close_vgmstream(vgmstream);
                continue;
            }

            /* dual file stereo */
            if (do_dfs && vgmstream->meta_type == meta_DSP_STD && vgmstream->channels == 1) {
                try_dual_file_stereo(vgmstream, filename);
            }

            /* save start things so we can restart for seeking */
            /* TODO: we may need to save other things here */
            memcpy(vgmstream->start_ch,vgmstream->ch,sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
            vgmstream->start_block_offset = vgmstream->current_block_offset;
            return vgmstream;
        }
    }

    return NULL;
}

/* simply allocate memory for the VGMSTREAM and its channels */
VGMSTREAM * allocate_vgmstream(int channel_count, int looped) {
    VGMSTREAM * vgmstream;
    VGMSTREAMCHANNEL * channels;
    VGMSTREAMCHANNEL * start_channels;
    VGMSTREAMCHANNEL * loop_channels;

    if (channel_count <= 0) return NULL;

    vgmstream = calloc(1,sizeof(VGMSTREAM));
    if (!vgmstream) return NULL;

    channels = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
    if (!channels) {
        free(vgmstream);
        return NULL;
    }
    vgmstream->ch = channels;
    vgmstream->channels = channel_count;

    start_channels = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
    if (!start_channels) {
        free(vgmstream);
        free(channels);
        return NULL;
    }
    vgmstream->start_ch = start_channels;

    if (looped) {
        loop_channels = calloc(channel_count,sizeof(VGMSTREAMCHANNEL));
        if (!loop_channels) {
            free(vgmstream);
            free(channels);
            free(start_channels);
            return NULL;
        }
        vgmstream->loop_ch = loop_channels;
    }

    vgmstream->loop_flag = looped;

    return vgmstream;
}

void close_vgmstream(VGMSTREAM * vgmstream) {
    int i;
    if (!vgmstream) return;

    for (i=0;i<vgmstream->channels;i++)
        if (vgmstream->ch[i].streamfile)
            close_streamfile(vgmstream->ch[i].streamfile);

    if (vgmstream->loop_ch) free(vgmstream->loop_ch);
    if (vgmstream->start_ch) free(vgmstream->start_ch);
    if (vgmstream->ch) free(vgmstream->ch);

    free(vgmstream);
}

int32_t get_vgmstream_play_samples(double looptimes, double fadetime, VGMSTREAM * vgmstream) {
    if (vgmstream->loop_flag) {
        return vgmstream->loop_start_sample+(vgmstream->loop_end_sample-vgmstream->loop_start_sample)*looptimes+fadetime*vgmstream->sample_rate;
    } else return vgmstream->num_samples;
}

void render_vgmstream(sample * buffer, int32_t sample_count, VGMSTREAM * vgmstream) {
    switch (vgmstream->layout_type) {
        case layout_interleave:
        case layout_interleave_shortblock:
            render_vgmstream_interleave(buffer,sample_count,vgmstream);
            break;
        case layout_dtk_interleave:
        case layout_none:
            render_vgmstream_nolayout(buffer,sample_count,vgmstream);
            break;
        case layout_ast_blocked:
        case layout_halpst_blocked:
		case layout_xa_blocked:
            render_vgmstream_blocked(buffer,sample_count,vgmstream);
            break;
    }
}

int get_vgmstream_samples_per_frame(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
            return 32;
        case coding_NGC_DSP:
            return 14;
        case coding_PCM16LE:
        case coding_PCM16BE:
        case coding_PCM8:
            return 1;
        case coding_NDS_IMA:
            return (vgmstream->interleave_block_size-4)*2;
        case coding_NGC_DTK:
            return 28;
        case coding_G721:
            return 1;
        case coding_NGC_AFC:
            return 16;
        case coding_PSX:
		case coding_XA:
            return 28;
        default:
            return 0;
    }
}

int get_vgmstream_samples_per_shortframe(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_NDS_IMA:
            return (vgmstream->interleave_smallblock_size-4)*2;
        default:
            return get_vgmstream_samples_per_frame(vgmstream);
    }
}

int get_vgmstream_frame_size(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
            return 18;
        case coding_NGC_DSP:
            return 8;
        case coding_PCM16LE:
        case coding_PCM16BE:
            return 2;
        case coding_PCM8:
            return 1;
        case coding_NDS_IMA:
            return vgmstream->interleave_block_size;
        case coding_NGC_DTK:
            return 32;
        case coding_G721:
            return 0;
        case coding_NGC_AFC:
            return 9;
        case coding_PSX:
            return 16;
		case coding_XA:
			return 14*vgmstream->channels;
        default:
            return 0;
    }
}

int get_vgmstream_shortframe_size(VGMSTREAM * vgmstream) {
    switch (vgmstream->coding_type) {
        case coding_NDS_IMA:
            return vgmstream->interleave_smallblock_size;
        default:
            return get_vgmstream_frame_size(vgmstream);
    }
}

void decode_vgmstream(VGMSTREAM * vgmstream, int samples_written, int samples_to_do, sample * buffer) {
    int chan;

    switch (vgmstream->coding_type) {
        case coding_CRI_ADX:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_adx(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }

            break;
        case coding_NGC_DSP:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ngc_dsp(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM16LE:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm16LE(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM16BE:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm16BE(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PCM8:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_pcm8(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_NDS_IMA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_nds_ima(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_NGC_DTK:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ngc_dtk(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do,chan);
            }
            break;
        case coding_G721:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_g721(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_NGC_AFC:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_ngc_afc(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
        case coding_PSX:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_psx(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
		case coding_XA:
            for (chan=0;chan<vgmstream->channels;chan++) {
                decode_xa(&vgmstream->ch[chan],buffer+samples_written*vgmstream->channels+chan,
                        vgmstream->channels,vgmstream->samples_into_block,
                        samples_to_do);
            }
            break;
    }
}

int vgmstream_samples_to_do(int samples_this_block, int samples_per_frame, VGMSTREAM * vgmstream) {
    int samples_to_do;
    int samples_left_this_block;

    samples_left_this_block = samples_this_block - vgmstream->samples_into_block;
    samples_to_do = samples_left_this_block;

    /* fun loopy crap */
    /* Why did I think this would be any simpler? */
    if (vgmstream->loop_flag) {
        /* are we going to hit the loop end during this block? */
        if (vgmstream->current_sample+samples_left_this_block > vgmstream->loop_end_sample) {
            /* only do to just before it */
            samples_to_do = vgmstream->loop_end_sample-vgmstream->current_sample;
        }

        /* are we going to hit the loop start during this block? */
        if (!vgmstream->hit_loop && vgmstream->current_sample+samples_left_this_block > vgmstream->loop_start_sample) {
            /* only do to just before it */
            samples_to_do = vgmstream->loop_start_sample-vgmstream->current_sample;
        }

    }

    /* if it's a framed encoding don't do more than one frame */
    if (samples_per_frame>1 && (vgmstream->samples_into_block%samples_per_frame)+samples_to_do>samples_per_frame) samples_to_do=samples_per_frame-(vgmstream->samples_into_block%samples_per_frame);

    return samples_to_do;
}

/* return 1 if we just looped */
int vgmstream_do_loop(VGMSTREAM * vgmstream) {
/*    if (vgmstream->loop_flag) {*/
        /* is this the loop end? */
        if (vgmstream->current_sample==vgmstream->loop_end_sample) {
            /* against everything I hold sacred, preserve adpcm
             * history through loop for certain types */
            if (vgmstream->meta_type == meta_DSP_STD ||
                    vgmstream->meta_type == meta_DSP_RS03 ||
                    vgmstream->meta_type == meta_DSP_CSTR || 
					vgmstream->coding_type == coding_PSX) {
                int i;
                for (i=0;i<vgmstream->channels;i++) {
                    vgmstream->loop_ch[i].adpcm_history1_16 = vgmstream->ch[i].adpcm_history1_16;
                    vgmstream->loop_ch[i].adpcm_history2_16 = vgmstream->ch[i].adpcm_history2_16;
                    vgmstream->loop_ch[i].adpcm_history1_32 = vgmstream->ch[i].adpcm_history1_32;
                    vgmstream->loop_ch[i].adpcm_history2_32 = vgmstream->ch[i].adpcm_history2_32;
                }
            }
#if DEBUG
            {
               int i;
               for (i=0;i<vgmstream->channels;i++) {
                   fprintf(stderr,"ch%d hist: %04x %04x loop hist: %04x %04x\n",i,
                           vgmstream->ch[i].adpcm_history1_16,vgmstream->ch[i].adpcm_history2_16,
                           vgmstream->loop_ch[i].adpcm_history1_16,vgmstream->loop_ch[i].adpcm_history2_16);
                   fprintf(stderr,"ch%d offset: %x loop offset: %x\n",i,
                           vgmstream->ch[i].offset,
                           vgmstream->loop_ch[i].offset);
               }
            }
#endif
            /* restore! */
            memcpy(vgmstream->ch,vgmstream->loop_ch,sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);
            vgmstream->current_sample=vgmstream->loop_sample;
            vgmstream->samples_into_block=vgmstream->loop_samples_into_block;
            vgmstream->current_block_size=vgmstream->loop_block_size;
            vgmstream->current_block_offset=vgmstream->loop_block_offset;
            vgmstream->next_block_offset=vgmstream->loop_next_block_offset;

            return 1;
        }


        /* is this the loop start? */
        if (!vgmstream->hit_loop && vgmstream->current_sample==vgmstream->loop_start_sample) {
            /* save! */
            memcpy(vgmstream->loop_ch,vgmstream->ch,sizeof(VGMSTREAMCHANNEL)*vgmstream->channels);

            vgmstream->loop_sample=vgmstream->current_sample;
            vgmstream->loop_samples_into_block=vgmstream->samples_into_block;
            vgmstream->loop_block_size=vgmstream->current_block_size;
            vgmstream->loop_block_offset=vgmstream->current_block_offset;
            vgmstream->loop_next_block_offset=vgmstream->next_block_offset;
            vgmstream->hit_loop=1;
        }
        /*}*/
        return 0;
}

/* build a descriptive string */
void describe_vgmstream(VGMSTREAM * vgmstream, char * desc, int length) {
#define TEMPSIZE 256
    char temp[TEMPSIZE];

    if (!vgmstream) {
        snprintf(temp,TEMPSIZE,"NULL VGMSTREAM");
        concatn(length,desc,temp);
        return;
    }

    snprintf(temp,TEMPSIZE,"sample rate %d Hz\n"
            "channels: %d\n",
            vgmstream->sample_rate,vgmstream->channels);
    concatn(length,desc,temp);

    if (vgmstream->loop_flag) {
        snprintf(temp,TEMPSIZE,"loop start: %d samples (%.2lf seconds)\n"
                "loop end: %d samples (%.2lf seconds)\n",
                vgmstream->loop_start_sample,
                (double)vgmstream->loop_start_sample/vgmstream->sample_rate,
                vgmstream->loop_end_sample,
                (double)vgmstream->loop_end_sample/vgmstream->sample_rate);
        concatn(length,desc,temp);
    }

    snprintf(temp,TEMPSIZE,"stream total samples: %d (%.2lf seconds)\n",
            vgmstream->num_samples,
            (double)vgmstream->num_samples/vgmstream->sample_rate);
    concatn(length,desc,temp);

    snprintf(temp,TEMPSIZE,"encoding: ");
    concatn(length,desc,temp);

    switch (vgmstream->coding_type) {
        case coding_PCM16BE:
            snprintf(temp,TEMPSIZE,"Big Endian 16-bit PCM");
            break;
        case coding_PCM16LE:
            snprintf(temp,TEMPSIZE,"Little Endian 16-bit PCM");
            break;
        case coding_PCM8:
            snprintf(temp,TEMPSIZE,"8-bit PCM");
            break;
        case coding_NGC_DSP:
            snprintf(temp,TEMPSIZE,"Gamecube \"DSP\" 4-bit ADPCM");
            break;
        case coding_CRI_ADX:
            snprintf(temp,TEMPSIZE,"CRI ADX 4-bit ADPCM");
            break;
        case coding_NDS_IMA:
            snprintf(temp,TEMPSIZE,"NDS-style 4-bit IMA ADPCM");
            break;
        case coding_NGC_DTK:
            snprintf(temp,TEMPSIZE,"Gamecube \"ADP\"/\"DTK\" 4-bit ADPCM");
            break;
        case coding_G721:
            snprintf(temp,TEMPSIZE,"CCITT G.721 4-bit ADPCM");
            break;
        case coding_NGC_AFC:
            snprintf(temp,TEMPSIZE,"Gamecube \"AFC\" 4-bit ADPCM");
            break;
        case coding_PSX:
            snprintf(temp,TEMPSIZE,"Playstation 4-bit ADPCM");
            break;
        case coding_XA:
            snprintf(temp,TEMPSIZE,"CD-ROM XA 4-bit ADPCM");
            break;
        default:
            snprintf(temp,TEMPSIZE,"CANNOT DECODE");
    }
    concatn(length,desc,temp);

    snprintf(temp,TEMPSIZE,"\nlayout: ");
    concatn(length,desc,temp);

    switch (vgmstream->layout_type) {
        case layout_none:
            snprintf(temp,TEMPSIZE,"flat (no layout)");
            break;
        case layout_interleave:
            snprintf(temp,TEMPSIZE,"interleave");
            break;
        case layout_interleave_shortblock:
            snprintf(temp,TEMPSIZE,"interleave with short last block");
            break;
        case layout_dtk_interleave:
            snprintf(temp,TEMPSIZE,"ADP/DTK nibble interleave");
            break;
        case layout_ast_blocked:
            snprintf(temp,TEMPSIZE,"AST blocked");
            break;
        case layout_halpst_blocked:
            snprintf(temp,TEMPSIZE,"HALPST blocked");
            break;
		case layout_xa_blocked:
            snprintf(temp,TEMPSIZE,"CD-ROM XA");
            break;
        default:
            snprintf(temp,TEMPSIZE,"INCONCEIVABLE");
    }
    concatn(length,desc,temp);

    snprintf(temp,TEMPSIZE,"\n");
    concatn(length,desc,temp);

    if (vgmstream->layout_type == layout_interleave || vgmstream->layout_type == layout_interleave_shortblock) {
        snprintf(temp,TEMPSIZE,"interleave: %#x bytes\n",
                vgmstream->interleave_block_size);
        concatn(length,desc,temp);

        if (vgmstream->layout_type == layout_interleave_shortblock) {
            snprintf(temp,TEMPSIZE,"last block interleave: %#x bytes\n",
                    vgmstream->interleave_smallblock_size);
            concatn(length,desc,temp);
        }
    }

    snprintf(temp,TEMPSIZE,"metadata from: ");
    concatn(length,desc,temp);

    switch (vgmstream->meta_type) {
        case meta_RSTM:
            snprintf(temp,TEMPSIZE,"Nintendo RSTM header");
            break;
        case meta_STRM:
            snprintf(temp,TEMPSIZE,"Nintendo STRM header");
            break;
        case meta_ADX_03:
            snprintf(temp,TEMPSIZE,"CRI ADX header type 03");
            break;
        case meta_ADX_04:
            snprintf(temp,TEMPSIZE,"CRI ADX header type 04");
            break;
        case meta_ADX_05:
            snprintf(temp,TEMPSIZE,"CRI ADX header type 05");
            break;
        case meta_DSP_AGSC:
            snprintf(temp,TEMPSIZE,"Retro Studios AGSC header");
            break;
        case meta_NGC_ADPDTK:
            snprintf(temp,TEMPSIZE,"assumed Nintendo ADP by .adp extension and valid first frame");
            break;
        case meta_RSF:
            snprintf(temp,TEMPSIZE,"assumed Retro Studios RSF by .rsf extension and valid first bytes");
            break;
        case meta_AFC:
            snprintf(temp,TEMPSIZE,"Nintendo AFC header");
            break;
        case meta_AST:
            snprintf(temp,TEMPSIZE,"Nintendo AST header");
            break;
        case meta_HALPST:
            snprintf(temp,TEMPSIZE,"HAL Laboratory HALPST header");
            break;
        case meta_DSP_RS03:
            snprintf(temp,TEMPSIZE,"Retro Studios RS03 header");
            break;
        case meta_DSP_STD:
            snprintf(temp,TEMPSIZE,"Standard Nintendo DSP header");
            break;
        case meta_DSP_CSTR:
            snprintf(temp,TEMPSIZE,"Namco Cstr header");
            break;
        case meta_GCSW:
            snprintf(temp,TEMPSIZE,"GCSW header");
            break;
        case meta_PS2_SShd:
            snprintf(temp,TEMPSIZE,"ADS File (with SShd header)");
            break;
		case meta_PS2_NPSF:
            snprintf(temp,TEMPSIZE,"Namco Production Sound File (NPSF)");
            break;
        case meta_RWSD:
            snprintf(temp,TEMPSIZE,"Nintendo RWSD header (single stream)");
            break;
        case meta_PSX_XA:
            snprintf(temp,TEMPSIZE,"RIFF/CDXA Header");
            break;
		case meta_PS2_RXW:
            snprintf(temp,TEMPSIZE,"RXWS File (Arc The Lad)");
            break;
		case meta_PS2_RAW:
            snprintf(temp,TEMPSIZE,"assumed RAW Interleaved PCM by .int extension");
            break;
        case meta_DSP_STM:
            snprintf(temp,TEMPSIZE,"Nintendo STM header");
            break;
        case meta_PS2_EXST:
            snprintf(temp,TEMPSIZE,"EXST File (Shadow of the Colossus)");
            break;
        default:
            snprintf(temp,TEMPSIZE,"THEY SHOULD HAVE SENT A POET");
    }
    concatn(length,desc,temp);
}

/* */
#define DFS_PAIR_COUNT 4
const char * const dfs_pairs[DFS_PAIR_COUNT][2] = {
    {"L","R"},
    {"l","r"},
    {"_0","_1"},
    {"left","right"},
};

void try_dual_file_stereo(VGMSTREAM * opened_stream, const char * const filename) {
    char * filename2;
    char * ext;
    int dfs_name= -1; /*-1=no stereo, 0=opened_stream is left, 1=opened_stream is right */
    VGMSTREAM * new_stream = NULL;
    int i,j;

    if (opened_stream->channels != 1) return;

    /* we need at least a base and a name ending to replace */
    if (strlen(filename)<2) return;

    /* one extra for terminator, one for possible extra character (left>=right) */
    filename2 = malloc(strlen(filename)+2); 

    if (!filename2) return;

    strcpy(filename2,filename);

    /* look relative to the extension; */
    ext = (char *)filename_extension(filename2);

    /* we treat the . as part of the extension */
    if (ext-filename2 >= 1 && ext[-1]=='.') ext--;

    for (i=0; dfs_name==-1 && i<DFS_PAIR_COUNT; i++) {
        for (j=0; dfs_name==-1 && j<2; j++) {
            /* find a postfix on the name */
            if (!memcmp(ext-strlen(dfs_pairs[i][j]),
                        dfs_pairs[i][j],
                        strlen(dfs_pairs[i][j]))) {
                int other_name=j^1;
                int moveby;
                dfs_name=j;

                /* move the extension */
                moveby = strlen(dfs_pairs[i][other_name]) -
                    strlen(dfs_pairs[i][dfs_name]);
                memmove(ext+moveby,ext,strlen(ext)+1); /* terminator, too */

                /* make the new name */
                memcpy(ext+moveby-strlen(dfs_pairs[i][other_name]),dfs_pairs[i][other_name],strlen(dfs_pairs[i][other_name]));
            }
        }
    }

    /* did we find a name for the other file? */
    if (dfs_name==-1) goto fail;

#if 0
    printf("input is:            %s\n"
            "other file would be: %s\n",
            filename,filename2);
#endif

    new_stream = init_vgmstream_internal(filename2,
            0   /* don't do dual file on this, to prevent recursion */
            );

    /* see if we were able to open the file, and if everything matched nicely */
    if (new_stream &&
            new_stream->channels == 1 &&
            /* we have seen legitimate pairs where these are off by one... */
            /* but leaving it commented out until I can find those and recheck */
            /* abs(new_stream->num_samples-opened_stream->num_samples <= 1) && */
            new_stream->num_samples == opened_stream->num_samples &&
            new_stream->sample_rate == opened_stream->sample_rate &&
            new_stream->meta_type == opened_stream->meta_type &&
            new_stream->coding_type == opened_stream->coding_type &&
            new_stream->layout_type == opened_stream->layout_type &&
            new_stream->loop_flag == opened_stream->loop_flag &&
            /* check these even if there is no loop, because they should then
             * be zero in both */
            new_stream->loop_start_sample == opened_stream->loop_start_sample &&
            new_stream->loop_end_sample == opened_stream->loop_end_sample &&
            /* check even if the layout doesn't use them, because it is
             * difficult to determine when it does, and they should be zero
             * otherwise, anyway */
            new_stream->interleave_block_size == opened_stream->interleave_block_size &&
            new_stream->interleave_smallblock_size == opened_stream->interleave_smallblock_size &&
            new_stream->start_block_offset == opened_stream->start_block_offset) {
        /* We seem to have a usable, matching file. Merge in the second channel. */
        VGMSTREAMCHANNEL * new_chans;
        VGMSTREAMCHANNEL * new_loop_chans = NULL;
        VGMSTREAMCHANNEL * new_start_chans = NULL;

        /* build the channels */
        new_chans = calloc(2,sizeof(VGMSTREAMCHANNEL));
        if (!new_chans) goto fail;

        memcpy(&new_chans[dfs_name],&opened_stream->ch[0],sizeof(VGMSTREAMCHANNEL));
        memcpy(&new_chans[dfs_name^1],&new_stream->ch[0],sizeof(VGMSTREAMCHANNEL));

        /* loop and start will be initialized later, we just need to
         * allocate them here */
        new_start_chans = calloc(2,sizeof(VGMSTREAMCHANNEL));
        if (!new_start_chans) {
            free(new_chans);
            goto fail;
        }

        if (opened_stream->loop_ch) {
            new_loop_chans = calloc(2,sizeof(VGMSTREAMCHANNEL));
            if (!new_loop_chans) {
                free(new_chans);
                free(new_start_chans);
                goto fail;
            }
        }

        /* remove the existing structures */
        /* not using close_vgmstream as that would close the file */
        free(opened_stream->ch);
        free(new_stream->ch);

        free(opened_stream->start_ch);
        free(new_stream->start_ch);

        if (opened_stream->loop_ch) {
            free(opened_stream->loop_ch);
            free(new_stream->loop_ch);
        }

        /* fill in the new structures */
        opened_stream->ch = new_chans;
        opened_stream->start_ch = new_start_chans;
        opened_stream->loop_ch = new_loop_chans;

        /* stereo! */
        opened_stream->channels = 2;

        /* discard the second VGMSTREAM */
        free(new_stream);
    }

    if (filename2) free(filename2);
    return;

fail:
    if (filename2) free(filename2);
}
