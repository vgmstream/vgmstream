#include "meta.h"
#include "../coding/coding.h"


/* Regions seem mostly for in-game purposes and are not very listenable on its own.
 * Also, sample start is slightly off since vgmstream can't start in the middle of block ATM.
 * Otherwise this kinda works, but for now it's just a test. */
#define BFSTM_ENABLE_REGION_SUBSONGS 0
#define BFSTM_ENABLE_REGION_FORCE_LOOPS 0 /* this makes sense in SM3D World, but not in Zelda BotW) */

#if BFSTM_ENABLE_REGION_SUBSONGS
static off_t bfstm_set_regions(STREAMFILE *streamFile, VGMSTREAM *vgmstream, int region_count, off_t regn_offset, int codec, int big_endian);
#endif


/* BFSTM - Nintendo Wii U format */
VGMSTREAM * init_vgmstream_bfstm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    off_t info_offset = 0, data_offset = 0;
    int channel_count, loop_flag, codec;
    int big_endian;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
#if BFSTM_ENABLE_REGION_SUBSONGS
    off_t regn_offset = 0;
    int  region_count;
#endif


    /* checks */
    if ( !check_extensions(streamFile,"bfstm") )
        goto fail;

    /* FSTM header */
    if (read_32bitBE(0x00, streamFile) != 0x4653544D) /* "FSTM" */
        goto fail;
    /* 0x06(2): header size (0x40), 0x08: version (0x00000400), 0x0c: file size */

    /* check BOM */
    if ((uint16_t)read_16bitBE(0x04, streamFile) == 0xFEFF) { /* Wii U games */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        big_endian = 1;
    } else if ((uint16_t)read_16bitBE(0x04, streamFile) == 0xFFFE) { /* Switch games */
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        big_endian = 0;
    } else {
        goto fail;
    }

    /* get sections (should always appear in the same order) */
    {
        int i;
        int section_count = read_16bit(0x10, streamFile);
        for (i = 0; i < section_count; i++) {
            /* 0x00: id, 0x02(2): padding, 0x04(4): offset, 0x08(4): size */
            uint16_t section_id = read_16bit(0x14 + i*0xc+0x00, streamFile);
            switch(section_id) {
                case 0x4000: info_offset = read_32bit(0x14+i*0x0c+0x04, streamFile); break;
                case 0x4001: /* seek_offset = read_32bit(0x14+i*0x0c+0x04, streamFile); */ break;
                case 0x4002: data_offset = read_32bit(0x14+i*0x0c+0x04, streamFile); break;
#if BFSTM_ENABLE_REGION_SUBSONGS
                case 0x4003: regn_offset = read_32bit(0x14+i*0x0c+0x04, streamFile); break;
#endif
                case 0x4004: /* pdat_offset = read_32bit(0x14+i*0x0c+0x04, streamFile); */ break; /* prefetch data */
                default:
                    break;
            }
        }

        if (info_offset == 0 || data_offset == 0)
            goto fail;
    }

    /* INFO section */
    if (read_32bitBE(info_offset, streamFile) != 0x494E464F) /* "INFO" */
        goto fail;
    codec = read_8bit(info_offset + 0x20, streamFile);
    loop_flag = read_8bit(info_offset + 0x21, streamFile);
    channel_count = read_8bit(info_offset + 0x22, streamFile);
#if BFSTM_ENABLE_REGION_SUBSONGS
    region_count = read_8bit(info_offset + 0x23, streamFile);
#endif


    start_offset = data_offset + 0x20;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bit(info_offset + 0x24, streamFile);
    vgmstream->num_samples = read_32bit(info_offset + 0x2c, streamFile);
    vgmstream->loop_start_sample = read_32bit(info_offset + 0x28, streamFile);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_FSTM;
    vgmstream->layout_type = (channel_count == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = read_32bit(info_offset + 0x34, streamFile);
    vgmstream->interleave_last_block_size = read_32bit(info_offset + 0x44, streamFile);

    switch(codec) {
        case 0x00:
            vgmstream->coding_type = coding_PCM8;
            break;
        case 0x01:
            vgmstream->coding_type = big_endian ? coding_PCM16BE : coding_PCM16LE;
            break;
        case 0x02:
            vgmstream->coding_type = coding_NGC_DSP;

            {
                int i, c;
                off_t channel_indexes, channel_info_offset, coefs_offset;

                channel_indexes = info_offset+0x08 + read_32bit(info_offset + 0x1C, streamFile);
                for (i = 0; i < vgmstream->channels; i++) {
                    channel_info_offset = channel_indexes + read_32bit(channel_indexes+0x04+(i*0x08)+0x04, streamFile);
                    coefs_offset = channel_info_offset + read_32bit(channel_info_offset+0x04, streamFile);

                    for (c = 0; c < 16; c++) {
                        vgmstream->ch[i].adpcm_coef[c] = read_16bit(coefs_offset + c*2, streamFile);
                    }
                }
            }
            break;

        default: /* 0x03: IMA? */
            goto fail;
    }


#if BFSTM_ENABLE_REGION_SUBSONGS
    start_offset += bfstm_set_regions(streamFile, vgmstream, region_count, regn_offset, codec, big_endian);
#endif

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


#if BFSTM_ENABLE_REGION_SUBSONGS
/* Newer .bfstm may have multiple regions, that are sample sections of some meaning,
 *  like loop parts (Super Mario 3D World), or dynamic subsongs (Zelda: BotW)
 * We'll hack them in as subsongs (though seem mostly activated by game events) */
static off_t bfstm_set_regions(STREAMFILE *streamFile, VGMSTREAM *vgmstream, int region_count, off_t regn_offset, int codec, int big_endian) {
    off_t start_offset;
    size_t stream_size;
    int total_subsongs, target_subsong = streamFile->stream_index;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = big_endian ? read_16bitLE : read_16bitLE;


    if (region_count <= 0 && regn_offset == 0 && codec != 0x02)
        goto fail;
    if (read_32bitBE(regn_offset, streamFile) != 0x5245474E) /* "REGN" */
        goto fail;

    /* pretend each region is a subsong, but use first subsong as the whole file,
     * since regions may not map all samples */
    total_subsongs = region_count + 1;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    if (target_subsong > 1) {
        int sample_aligned = 0, sample_skip = 0;
        int i;
        off_t region_start, region_end;
        size_t block_size;
        size_t sample_start = read_32bit(regn_offset + 0x20 + (target_subsong-2)*0x100+0x00, streamFile);
        size_t sample_end   = read_32bit(regn_offset + 0x20 + (target_subsong-2)*0x100+0x04, streamFile) + 1;
        off_t adpcm_offset  = regn_offset + 0x20 + (target_subsong-2)*0x100+0x08;
        /* rest is padding up to 0x100 */

        /* samples-to-bytes, approximate since samples could land in the middle of a 0x08 frame */
        region_start = sample_start / 14 * vgmstream->channels * 0x08;
        region_end = sample_end / 14 * vgmstream->channels * 0x08;
        stream_size = region_end - region_start;
        //;VGM_LOG("BFSTM: region offset start=%lx, end=%lx\n", region_start, region_end);

        /* align to block start or interleave causes funny sounds, but the bigger the interleave
         * the less accurate this is (with 0x2000 align can be off by ~4600 samples per channel) */
        //todo could be fixed with interleave_first_block
        block_size = (vgmstream->interleave_block_size*vgmstream->channels);
        if (region_start % block_size) {
            region_start -= region_start % block_size; /* now aligned */
            //;VGM_LOG("BFSTM: new region start=%lx\n", region_start);

            /* get position of our block (close but smaller than sample_start) */
            sample_aligned = dsp_bytes_to_samples(region_start, vgmstream->channels);
            /* and how many samples to skip until actual sample_start */
            sample_skip = (sample_start - sample_aligned);
        }

        //;VGM_LOG("BFSTM: region align=%i, skip=%i, start=%i, end=%i\n", sample_aligned, sample_skip, sample_start, sample_end);
        start_offset = region_start;

        if (sample_end != vgmstream->num_samples) /* not exact but... */
            vgmstream->interleave_last_block_size = 0;

        vgmstream->num_samples = sample_skip + (sample_end - sample_start);
        vgmstream->loop_start_sample = sample_skip;
        vgmstream->loop_end_sample = vgmstream->num_samples;
#if BFSTM_ENABLE_REGION_FORCE_LOOPS
        vgmstream_force_loop(vgmstream, 1, vgmstream->loop_start_sample, vgmstream->loop_end_sample);
#endif
        /* maybe loops should be disabled with some regions? */

        /* this won't make sense after aligning, whatevs, doesn't sound too bad */
        for (i = 0; i < vgmstream->channels; i++) {
            vgmstream->ch[i].adpcm_history1_16 = read_16bit(adpcm_offset+0x02+0x00, streamFile);
            vgmstream->ch[i].adpcm_history2_16 = read_16bit(adpcm_offset+0x02+0x02, streamFile);
        }
    }
    else {
        start_offset = 0;
        stream_size = get_streamfile_size(streamFile);
    }

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    return start_offset;
fail:
    return 0;
}
#endif
