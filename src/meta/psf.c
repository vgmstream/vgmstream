#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* PSF single - Pivotal games single segment (external in some PC/Xbox or inside bigfiles) [The Great Escape, Conflict series] */
VGMSTREAM * init_vgmstream_psf_single(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate, rate_value, interleave;
    uint32_t psf_config;
    uint8_t flags;
    size_t data_size;
    coding_t codec;


    /* checks */
    /* .psf: actual extension
     * .swd: bigfile extension */
    if (!check_extensions(streamFile, "psf,swd"))
        goto fail;
    if ((read_32bitBE(0x00,streamFile) & 0xFFFFFF00) != 0x50534600) /* "PSF\00" */
        goto fail;

    flags = read_8bit(0x03,streamFile);
    switch(flags) {
        case 0xC0: /* [The Great Escape (PS2), Conflict: Desert Storm (PS2)] */
        case 0xA1: /* [Conflict: Desert Storm 2 (PS2)] */
        case 0x21: /* [Conflict: Desert Storm 2 (PS2), Conflict: Global Storm (PS2)] */
      //case 0x22: /* [Conflict: Vietman (PS2)] */ //todo weird size value, stereo, only one found
            channel_count = 2;
            if (flags == 0x21)
                channel_count = 1;
            interleave = 0x10;
            codec = coding_PSX;
            start_offset = 0x08;
            break;

        case 0x80: /* [The Great Escape (PC/Xbox), Conflict: Desert Storm (Xbox/GC), Conflict: Desert Storm 2 (Xbox)] */
        case 0x81: /* [Conflict: Desert Storm 2 (Xbox), Conflict: Vietnam (Xbox)] */
        case 0x01: /* [Conflict: Global Storm (Xbox)] */
            channel_count = 2;
            if (flags == 0x01)
                channel_count = 1;
            interleave = 0x10;
            codec = coding_PSX_pivotal;
            start_offset = 0x08;
            break;

        case 0xD1: /* [Conflict: Desert Storm 2 (GC)] */
            channel_count = 2;
            interleave = 0x08;
            codec = coding_NGC_DSP;
            start_offset = 0x08 + 0x60 * channel_count;
            break;

        default:
            goto fail;
    }

    loop_flag = 0;

    psf_config = read_32bitLE(0x04, streamFile);

    /* pitch/cents? */
    rate_value = (psf_config >> 20) & 0xFFF;
    switch(rate_value) {
      //case 0xEB5:
      //case 0xEB4:
        case 0xEB3: sample_rate = 44100; break;
        case 0x555: sample_rate = 16000; break;
        case 0x355: sample_rate = 11050; break;
        case 0x1d5: sample_rate = 6000;  break; /* ? */
        case 0x1cc: sample_rate = 5000;  break;
        default:
            VGM_LOG("PSF: unknown rate value %x\n", rate_value);
            goto fail;
    }

    data_size = (psf_config & 0xFFFFF) * (interleave * channel_count); /* in blocks */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PSF;
    vgmstream->sample_rate = sample_rate;

    switch(codec) {
        case coding_PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);
            break;

        case coding_PSX_pivotal:
            vgmstream->coding_type = coding_PSX_pivotal;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_cfg_bytes_to_samples(data_size, 0x10, channel_count);
            break;

        case coding_NGC_DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            /* has standard DSP headers at 0x08 */
            dsp_read_coefs_be(vgmstream,streamFile,0x08+0x1c,0x60);
            dsp_read_hist_be (vgmstream,streamFile,0x08+0x40,0x60);

            vgmstream->num_samples = read_32bitBE(0x08, streamFile);//dsp_bytes_to_samples(data_size, channel_count);
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* PSF segmented - Pivotal games multiple segments (external in some PC/Xbox or inside bigfiles) [The Great Escape, Conflict series] */
VGMSTREAM * init_vgmstream_psf_segmented(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE* temp_streamFile = NULL;
    segmented_layout_data *data = NULL;
    int i, segment_count, loop_flag = 0, loop_start = 0, loop_end = 0;
    off_t offset;


    /* checks */
    /* .psf: actual extension
     * .swd: bigfile extension */
    if (!check_extensions(streamFile, "psf,swd"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x50534660 &&  /* "PSF\60" [The Great Escape (PC/Xbox/PS2), Conflict: Desert Storm (Xbox/GC)] */
        read_32bitBE(0x00,streamFile) != 0x50534631)    /* "PSF\31" [Conflict: Desert Storm 2 (Xbox/GC/PS2)] */
        goto fail;

    segment_count = read_32bitLE(0x04, streamFile);
    loop_flag = 0;

    offset = 0x08;
    offset += 0x0c; /* first segment points to itself? */
    segment_count--;

    /* build segments */
    data = init_layout_segmented(segment_count);
    if (!data) goto fail;

    for (i = 0; i < segment_count; i++) {
        off_t psf_offset;
        size_t psf_size;
        uint32_t psf_id;

        /* mini table */
        psf_offset = read_32bitLE(offset + 0x00, streamFile);
        /* 0x04-0c: 0x02*4 transition segments (possibly to 4 song variations) */

        /* use last section transition as loop */
        if (i + 1 == segment_count) {
            loop_flag = 1;
            loop_start = read_16bitLE(offset + 0x0a, streamFile) - 1; /* also ignore first segment */
            loop_end = i;
        }

        /* multiple segment  can point to the same PSF offset (for repeated song sections) */
        //todo reuse repeated VGMSTREAMs to improve memory a bit

        psf_id = read_32bitBE(psf_offset + 0x00, streamFile);
        psf_size = read_32bitLE(psf_offset + 0x04, streamFile);
        if (psf_id == 0x505346D1) //todo improve
            psf_size = (psf_size & 0xFFFFF) * 0x10;
        else
            psf_size = (psf_size & 0xFFFFF) * 0x20;
        //;VGM_LOG("PSF: offset=%lx, size=%x\n", psf_offset, psf_size);

        temp_streamFile = setup_subfile_streamfile(streamFile, psf_offset, psf_size, "psf");
        if (!temp_streamFile) goto fail;

        data->segments[i] = init_vgmstream_psf_single(temp_streamFile);
        if (!data->segments[i]) goto fail;

        offset += 0x0c;
    }

    /* setup VGMSTREAMs */
    if (!setup_layout_segmented(data))
        goto fail;
    vgmstream = allocate_segmented_vgmstream(data,loop_flag, loop_start, loop_end);
    if (!vgmstream) goto fail;

    vgmstream->stream_size = get_streamfile_size(streamFile);

    return vgmstream;
fail:
    if (!vgmstream) free_layout_segmented(data);
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}

#if 0
VGMSTREAM * init_vgmstream_sch(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;


    /* checks */
    if (!check_extensions(streamFile, "sch"))
        goto fail;

    /* chunked format (id+size, GC pads to 0x20 and uses BE/inverted ids):
     * - SCH\0: start
     * - IMUS: points to a external .psf + segment table (same as in .psf, TGE only?)
     * - BANK: volume/etc info? points to something?
     * - PFSM: single .psf-like file (larger header)
     * - PFST: points to single PSF offset (.psf in TGE, or STREAMS.SWD); may be chained to next PFST?
     *
     * no other info so total subsongs would be count of usable chunks
     * in later games, segmented .psf seems to be removed and PFST is used instead
     */

    return vgmstream;
fail:
    if (!vgmstream) free_layout_layered(data);
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
#endif
