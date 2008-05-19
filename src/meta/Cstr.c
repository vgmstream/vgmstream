#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* .dsp w/ Cstr header, seen in Star Fox Assault and Donkey Konga */

VGMSTREAM * init_vgmstream_Cstr(const char * const filename) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * infile = NULL;

    int loop_flag;
    off_t start_offset;
    off_t first_data;
    off_t loop_offset;
    size_t interleave;
    int loop_adjust;
    int double_loop_end = 0;

    /* check extension, case insensitive */
    if (strcasecmp("dsp",filename_extension(filename))) goto fail;

    /* try to open the file for header reading */
    infile = open_streamfile(filename);
    if (!infile) goto fail;

    /* check header */
    if ((uint32_t)read_32bitBE(0,infile)!=0x43737472)   /* "Cstr" */
        goto fail;
#ifdef DEBUG
    fprintf(stderr,"header ok\n");
#endif

    if (read_8bit(0x1b,infile)==1) {
        /* mono version, much simpler to handle */
        /* Only seen in R Racing Evolution radio sfx */

        start_offset = 0x80;
        loop_flag = read_16bitBE(0x2c,infile);

        /* check initial predictor/scale */
        if (read_16bitBE(0x5e,infile) != (uint8_t)read_8bit(start_offset,infile))
            goto fail;

        /* check type==0 and gain==0 */
        if (read_16bitBE(0x2e,infile) || read_16bitBE(0x5c,infile))
            goto fail;

        loop_offset = start_offset+read_32bitBE(0x10,infile);
        if (loop_flag) {
            if (read_16bitBE(0x64,infile) != (uint8_t)read_8bit(loop_offset,infile)) goto fail;
        }

        /* build the VGMSTREAM */

        vgmstream = allocate_vgmstream(1,loop_flag);
        if (!vgmstream) goto fail;

        /* fill in the vital statistics */
        vgmstream->sample_rate = read_32bitBE(0x28,infile);
        vgmstream->num_samples = read_32bitBE(0x20,infile);

        if (loop_flag) {
        vgmstream->loop_start_sample = dsp_nibbles_to_samples(
                read_32bitBE(0x30,infile));
        vgmstream->loop_end_sample =  dsp_nibbles_to_samples(
                read_32bitBE(0x34,infile))+1;
        }

        vgmstream->coding_type = coding_NGC_DSP;
        vgmstream->layout_type = layout_none;
        vgmstream->meta_type = meta_DSP_CSTR;

        {
            int i;
            for (i=0;i<16;i++)
                vgmstream->ch[0].adpcm_coef[i]=read_16bitBE(0x3c+i*2,infile);
        }

        close_streamfile(infile); infile=NULL;

        /* open the file for reading by each channel */
        vgmstream->ch[0].streamfile = open_streamfile(filename);

        if (!vgmstream->ch[0].streamfile) goto fail;

        vgmstream->ch[0].channel_start_offset=
            vgmstream->ch[0].offset=
            start_offset;

        return vgmstream;
    }   /* end mono */

    interleave = read_16bitBE(0x06,infile);
    start_offset = 0xe0; 
    first_data = start_offset+read_32bitBE(0x0c,infile);
    loop_flag = read_16bitBE(0x2c,infile);

    if (!loop_flag) {
        /* Nonlooped tracks seem to follow no discernable pattern
         * with where they actually start.
         * But! with the magic of initial p/s redundancy, we can guess.
         */
        while (first_data<start_offset+0x800 &&
                (read_16bitBE(0x5e,infile) != (uint8_t)read_8bit(first_data,infile) ||
                read_16bitBE(0xbe,infile) != (uint8_t)read_8bit(first_data+interleave,infile)))
            first_data+=8;
#ifdef DEBUG
        fprintf(stderr,"guessed first_data at %#x\n",first_data);
#endif
    }

    /* check initial predictor/scale */
    if (read_16bitBE(0x5e,infile) != (uint8_t)read_8bit(first_data,infile))
        goto fail;
    if (read_16bitBE(0xbe,infile) != (uint8_t)read_8bit(first_data+interleave,infile))
        goto fail;

#ifdef DEBUG
    fprintf(stderr,"p/s ok\n");
#endif

    /* check type==0 and gain==0 */
    if (read_16bitBE(0x2e,infile) || read_16bitBE(0x5c,infile))
        goto fail;
    if (read_16bitBE(0x8e,infile) || read_16bitBE(0xbc,infile))
        goto fail;

#ifdef DEBUG
    fprintf(stderr,"type & gain ok\n");
#endif

    /* check for loop flag agreement */
    if (read_16bitBE(0x2c,infile) != read_16bitBE(0x8c,infile))
        goto fail;

#ifdef DEBUG
    fprintf(stderr,"loop flags agree\n");
#endif

    loop_offset = start_offset+read_32bitBE(0x10,infile)*2;
    if (loop_flag) {
        int loops_ok=0;
        /* check loop predictor/scale */
        /* some fuzz allowed */
        for (loop_adjust=0;loop_adjust>=-0x10;loop_adjust-=8) {
#ifdef DEBUG
            fprintf(stderr,"looking for loop p/s at %#x,%#x\n",loop_offset-interleave+loop_adjust,loop_offset+loop_adjust);
#endif
            if (read_16bitBE(0x64,infile) == (uint8_t)read_8bit(loop_offset-interleave+loop_adjust,infile) &&
                    read_16bitBE(0xc4,infile) == (uint8_t)read_8bit(loop_offset+loop_adjust,infile)) {
                loops_ok=1;
                break;
            }
        }
        if (!loops_ok)
            for (loop_adjust=interleave;loop_adjust<=interleave+0x10;loop_adjust+=8) {
#ifdef DEBUG
                fprintf(stderr,"looking for loop p/s at %#x,%#x\n",loop_offset-interleave+loop_adjust,loop_offset+loop_adjust);
#endif
                if (read_16bitBE(0x64,infile) == (uint8_t)read_8bit(loop_offset-interleave+loop_adjust,infile) &&
                        read_16bitBE(0xc4,infile) == (uint8_t)read_8bit(loop_offset+loop_adjust,infile)) {
                    loops_ok=1;
                    break;
                }
            }

        if (!loops_ok) goto fail;
#ifdef DEBUG
        fprintf(stderr,"loop p/s ok (with %#4x adjust)\n",loop_adjust);
#endif

        /* check for agreement */
        /* loop end (channel 1 & 2 headers) */
        if (read_32bitBE(0x34,infile) != read_32bitBE(0x94,infile))
            goto fail;
        
        /* Mr. Driller oddity */
        if (dsp_nibbles_to_samples(read_32bitBE(0x34,infile)*2)+1 <= read_32bitBE(0x20,infile)) {
#ifdef DEBUG
            fprintf(stderr,"loop end <= half total samples, should be doubled\n");
#endif
            double_loop_end = 1;
        }

        /* loop start (Cstr header and channel 1 header) */
        if (read_32bitBE(0x30,infile) != read_32bitBE(0x10,infile)
#if 0
                /* this particular glitch only true for SFA, though it
                 * seems like something similar happens in Donkey Konga */
                /* loop start (Cstr, channel 1 & 2 headers) */
                || (read_32bitBE(0x0c,infile)+read_32bitLE(0x30,infile)) !=
                read_32bitBE(0x90,infile)
#endif
           )
            /* alternatively (Donkey Konga) the header loop is 0x0c+0x10 */
            if (
                    /* loop start (Cstr header and channel 1 header) */
                    read_32bitBE(0x30,infile) != read_32bitBE(0x10,infile)+
                    read_32bitBE(0x0c,infile))
                /* further alternatively (Donkey Konga), if we loop back to
                 * the very first frame 0x30 might be 0x00000002 (which
                 * is a *valid* std dsp loop start, imagine that) while 0x10
                 * is 0x00000000 */
                if (!(read_32bitBE(0x30,infile) == 2 &&
                            read_32bitBE(0x10,infile) == 0))
                    /* lest there be too few alternatives, in Mr. Driller we
                     * find that [0x30] + [0x0c] + 8 = [0x10]*2 */
                    if (!(double_loop_end &&
                            read_32bitBE(0x30,infile) +
                            read_32bitBE(0x0c,infile) + 8 ==
                            read_32bitBE(0x10,infile)*2))
                        goto fail;

#ifdef DEBUG
        fprintf(stderr,"loop points agree\n");
#endif
    }

    /* assure that sample counts, sample rates agree */
    if (
            /* sample count (channel 1 & 2 headers) */
            read_32bitBE(0x20,infile) != read_32bitBE(0x80,infile) ||
            /* sample rate (channel 1 & 2 headers) */
            read_32bitBE(0x28,infile) != read_32bitBE(0x88,infile) ||
            /* sample count (Cstr header and channel 1 header) */
            read_32bitLE(0x14,infile) != read_32bitBE(0x20,infile) ||
            /* sample rate (Cstr header and channel 1 header) */
            (uint16_t)read_16bitLE(0x18,infile) != read_32bitBE(0x28,infile))
        goto fail;

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(2,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->sample_rate = read_32bitBE(0x28,infile);
    /* This is a slight hack to counteract their hack.
     * All the data is ofset by first_data so that the loop
     * point occurs at a block boundary. However, I always begin decoding
     * right after the header, as that is the start of the first block and
     * my interleave code relies on starting at the beginning of a block.
     * So we decode a few silent samples at the beginning, and here we make up
     * for it by lengthening the track by that much.
     */
    vgmstream->num_samples = read_32bitBE(0x20,infile) +
        (first_data-start_offset)/8*14;

    if (loop_flag) {
        off_t loop_start_bytes = loop_offset-start_offset-interleave;
        vgmstream->loop_start_sample = dsp_nibbles_to_samples((loop_start_bytes/(2*interleave)*interleave+loop_start_bytes%(interleave*2))*2);
        /*dsp_nibbles_to_samples(loop_start_bytes);*/
        /*dsp_nibbles_to_samples(read_32bitBE(0x30,infile)*2-inter);*/
        vgmstream->loop_end_sample =  dsp_nibbles_to_samples(
                read_32bitBE(0x34,infile))+1;

        if (double_loop_end)
            vgmstream->loop_end_sample =
                dsp_nibbles_to_samples(read_32bitBE(0x34,infile)*2)+1;

        if (vgmstream->loop_end_sample > vgmstream->num_samples) {
#ifdef DEBUG
            fprintf(stderr,"loop_end_sample > num_samples, adjusting\n");
#endif
            vgmstream->loop_end_sample = vgmstream->num_samples;
        }
    }

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_DSP_CSTR;

    {
        int i;
        for (i=0;i<16;i++)
            vgmstream->ch[0].adpcm_coef[i]=read_16bitBE(0x3c+i*2,infile);
        for (i=0;i<16;i++)
            vgmstream->ch[1].adpcm_coef[i]=read_16bitBE(0x9c+i*2,infile);
    }
#ifdef DEBUG
    vgmstream->ch[0].loop_history1 = read_16bitBE(0x66,infile);
    vgmstream->ch[0].loop_history2 = read_16bitBE(0x68,infile);
    vgmstream->ch[1].loop_history1 = read_16bitBE(0xc6,infile);
    vgmstream->ch[1].loop_history2 = read_16bitBE(0xc8,infile);
#endif

    close_streamfile(infile); infile=NULL;

    /* open the file for reading by each channel */
    {
        int i;
        for (i=0;i<2;i++) {
            vgmstream->ch[i].streamfile = open_streamfile_buffer(filename,interleave);

            if (!vgmstream->ch[i].streamfile) goto fail;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=
                start_offset+interleave*i;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (infile) close_streamfile(infile);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
