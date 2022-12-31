#include "meta.h"
#include "../coding/coding.h"


/* Regions seem mostly for in-game purposes and are not very listenable on its own.
 * Also, sample start is slightly off since vgmstream can't start in the middle of a frame ATM.
 * Otherwise this kinda works, but for now it's just a test. */
#define BFSTM_ENABLE_REGION_SUBSONGS 1

#if BFSTM_ENABLE_REGION_SUBSONGS
static off_t bfstm_set_regions(STREAMFILE* sf, VGMSTREAM* vgmstream, int region_count, off_t regn_offset, int codec, int big_endian);
#endif


/* BFSTM - Nintendo Wii U/Switch format */
VGMSTREAM* init_vgmstream_bfstm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    off_t info_offset = 0, data_offset = 0;
    int channels, loop_flag, codec;
    int big_endian;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
#if BFSTM_ENABLE_REGION_SUBSONGS
    off_t regn_offset = 0;
    int  region_count;
#endif


    /* checks */
    if (!is_id32be(0x00,sf, "FSTM"))
        goto fail;
    if (!check_extensions(sf,"bfstm"))
        goto fail;
    /* 0x06(2): header size (0x40)
     * 0x08: version (0x00000400)
     * 0x0c: file size */

    /* check BOM */
    if (read_u16be(0x04, sf) == 0xFEFF) { /* Wii U games */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        big_endian = 1;
    } else if (read_u16be(0x04, sf) == 0xFFFE) { /* Switch games */
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        big_endian = 0;
    } else {
        goto fail;
    }

    /* get sections (should always appear in the same order) */
    {
        int i;
        int section_count = read_16bit(0x10, sf);
        for (i = 0; i < section_count; i++) {
            /* 0x00: id, 0x02(2): padding, 0x04(4): offset, 0x08(4): size */
            uint16_t section_id = read_16bit(0x14 + i*0xc+0x00, sf);
            switch(section_id) {
                case 0x4000: info_offset = read_32bit(0x14+i*0x0c+0x04, sf); break;
                case 0x4001: /* seek_offset = read_32bit(0x14+i*0x0c+0x04, sf); */ break;
                case 0x4002: data_offset = read_32bit(0x14+i*0x0c+0x04, sf); break;
#if BFSTM_ENABLE_REGION_SUBSONGS
                case 0x4003: regn_offset = read_32bit(0x14+i*0x0c+0x04, sf); break;
#endif
                case 0x4004: /* pdat_offset = read_32bit(0x14+i*0x0c+0x04, sf); */ break; /* prefetch data */
                default:
                    break;
            }
        }

        if (info_offset == 0 || data_offset == 0)
            goto fail;
    }

    /* INFO section */
    if (read_32bitBE(info_offset, sf) != 0x494E464F) /* "INFO" */
        goto fail;
    codec = read_8bit(info_offset + 0x20, sf);
    loop_flag = read_8bit(info_offset + 0x21, sf);
    channels = read_8bit(info_offset + 0x22, sf);
#if BFSTM_ENABLE_REGION_SUBSONGS
    region_count = read_8bit(info_offset + 0x23, sf);
#endif


    start_offset = data_offset + 0x20;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bit(info_offset + 0x24, sf);
    vgmstream->num_samples = read_32bit(info_offset + 0x2c, sf);
    vgmstream->loop_start_sample = read_32bit(info_offset + 0x28, sf);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->meta_type = meta_FSTM;
    vgmstream->layout_type = (channels == 1) ? layout_none : layout_interleave;
    vgmstream->interleave_block_size = read_32bit(info_offset + 0x34, sf);
    vgmstream->interleave_last_block_size = read_32bit(info_offset + 0x44, sf);

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

                channel_indexes = info_offset+0x08 + read_32bit(info_offset + 0x1C, sf);
                for (i = 0; i < vgmstream->channels; i++) {
                    channel_info_offset = channel_indexes + read_32bit(channel_indexes+0x04+(i*0x08)+0x04, sf);
                    coefs_offset = channel_info_offset + read_32bit(channel_info_offset+0x04, sf);

                    for (c = 0; c < 16; c++) {
                        vgmstream->ch[i].adpcm_coef[c] = read_16bit(coefs_offset + c*2, sf);
                    }
                }
            }
            break;

        default: /* 0x03: IMA? */
            goto fail;
    }


#if BFSTM_ENABLE_REGION_SUBSONGS
    start_offset += bfstm_set_regions(sf, vgmstream, region_count, regn_offset, codec, big_endian);
#endif

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
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
static off_t bfstm_set_regions(STREAMFILE* sf, VGMSTREAM* vgmstream, int region_count, off_t regn_offset, int codec, int big_endian) {
    off_t start_offset;
    size_t stream_size;
    int total_subsongs, target_subsong = sf->stream_index;
    int32_t (*read_s32)(off_t,STREAMFILE*) = big_endian ? read_s32be : read_s32le;
    int16_t (*read_s16)(off_t,STREAMFILE*) = big_endian ? read_s16be : read_s16le;

    if (region_count <= 0 && regn_offset == 0 && codec != 0x02)
        goto fail;
    if (!is_id32be(regn_offset, sf, "REGN"))
        goto fail;

    /* pretend each region is a subsong, but use first subsong as the whole file,
     * since regions may not map all samples */
    total_subsongs = region_count + 1;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    if (target_subsong > 1) {
        int ch;
        off_t region_start, region_end, block_size;
        /* target region info */
        int32_t sample_start = read_s32(regn_offset + 0x20 + (target_subsong-2)*0x100 + 0x00, sf);
        int32_t sample_end   = read_s32(regn_offset + 0x20 + (target_subsong-2)*0x100 + 0x04, sf) + 1;
        off_t adpcm_offset  = regn_offset + 0x20 + (target_subsong-2)*0x100 + 0x08;
        /* rest is padding up to 0x100 */


        /* samples-to-bytes, approximate since regions' samples can land in the middle of a 0x08 frame */
        if (sample_start % 14) {
            /* can't decode in the middle of a nibble ATM so align to frame */
            //VGM_LOG("BFSTM: sample align %i, %i\n", sample_start % 14, sample_end % 14);
            sample_start -= sample_start % 14;
            sample_end -= sample_start % 14;
        }
        region_start = sample_start / 14 * vgmstream->channels * 0x08;
        region_end = sample_end / 14 * vgmstream->channels * 0x08;
        stream_size = region_end - region_start;
        //;VGM_LOG("BFSTM: region offset start=%lx, end=%lx\n", region_start, region_end);

        /* adjust region to closest block + use interleave first to correctly skip to first sample */
        block_size = (vgmstream->interleave_block_size * vgmstream->channels);
        if (region_start % block_size) {
            off_t region_skip = (region_start % block_size);
            //;VGM_LOG("BFSTM: new region start=%lx - %lx\n", region_start, region_skip);

            /* use interleave first to skip to correct offset */
            vgmstream->interleave_first_block_size = (block_size - region_skip) / vgmstream->channels;
            vgmstream->interleave_first_skip = (region_skip) / vgmstream->channels;
            region_start = region_start - region_skip + vgmstream->interleave_first_skip; /* now aligned */
        }

        //;VGM_LOG("BFSTM: region=%lx, adpcm=%lx, start=%i, end=%i\n", region_start, adpcm_offset, sample_start, sample_end);
        start_offset = region_start;

        /* sample_end doesn't fall in last block, interleave last doesn't apply */
        {
            int32_t block_samples = dsp_bytes_to_samples(block_size, vgmstream->channels);
            int32_t samples_align = vgmstream->num_samples / block_samples * block_samples;
            if (sample_end < samples_align)
                vgmstream->interleave_last_block_size = 0;
        }

        vgmstream->num_samples = /*sample_skip +*/ (sample_end - sample_start);
#if 0
        /* this makes sense in SM3D World, but not in Zelda BotW */
        vgmstream->loop_start_sample = 0;;
        vgmstream->loop_end_sample = vgmstream->num_samples;
        vgmstream_force_loop(vgmstream, 1, vgmstream->loop_start_sample, vgmstream->loop_end_sample);
#endif
        /* maybe loops should be disabled with some regions? */

        /* region_start points to correct frame (when compared to ADPCM predictor), but not sure if hist
         * is for exact nibble rather than first (sounds ok though) */
        for (ch = 0; ch < vgmstream->channels; ch++) {
            /* 0x00: ADPCM predictor */
            vgmstream->ch[ch].adpcm_history1_16 = read_s16(adpcm_offset + 0x02 + 0x06*ch, sf);
            vgmstream->ch[ch].adpcm_history2_16 = read_s16(adpcm_offset + 0x04 + 0x06*ch, sf);
        }
    }
    else {
        start_offset = 0;
        stream_size = get_streamfile_size(sf);
    }

    /* for now don't show subsongs since some regions are a bit strange */
    //vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    return start_offset;
fail:
    return 0;
}
#endif
