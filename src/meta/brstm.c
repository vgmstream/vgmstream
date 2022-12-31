#include "meta.h"
#include "../coding/coding.h"

VGMSTREAM* init_vgmstream_brstm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, head_size, head_offset, info_offset;
    int channels, loop_flag, codec, version;

    /* checks */
    if (!is_id32be(0x00,sf, "RSTM"))
        goto fail;

    /* .brstm: standard
     * .brstmspm: fake hack */
    if (!check_extensions(sf,"brstm,brstmspm"))
        goto fail;

    if (read_u16be(0x04, sf) != 0xFEFF) /* BE BOM for all Wii games */
        goto fail;

    version = read_u16be(0x06, sf); /* 0.1 (Trauma Center), 1.0 (all others) */
    if (read_u32be(0x08, sf) != get_streamfile_size(sf))
        goto fail;

    head_size = read_u16be(0x0c, sf);
    /* 0x0e: chunk count */

    if (version == 0x0001) {
        /* smaller simpler header found in some (beta?) files */
        head_offset = head_size;
        info_offset = head_offset + 0x08;
    }
    else {
        /* chunk table: offset + sixe x N chunks */
        head_offset = read_u32be(0x10,sf); /* in practice same as head_size */
        info_offset = head_offset + 0x20;
        /* HEAD starts with a sub-chunk table (info, )*/
    }

    if (!is_id32be(head_offset,sf, "HEAD"))
        goto fail;
    /* 0x04: chunk size (set to 0x8 in v0.1) */

    codec = read_u8(info_offset+0x00,sf);
    loop_flag = read_u8(info_offset+0x01,sf);
    channels = read_u8(info_offset+0x02,sf);

    start_offset = read_u32be(info_offset+0x10,sf); /* inside DATA chunk */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RSTM;

    vgmstream->sample_rate = read_u16be(info_offset+0x04,sf);
    vgmstream->loop_start_sample = read_s32be(info_offset+0x08,sf);
    vgmstream->num_samples = read_s32be(info_offset+0x0c,sf);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->interleave_block_size = read_u32be(info_offset+0x18,sf);
    vgmstream->interleave_last_block_size = read_u32be(info_offset+0x28,sf);

    /* many Super Paper Mario tracks have a 44.1KHz sample rate in the header,
     * but they should be played at 22.05KHz; detect with fake extension */
    if (vgmstream->sample_rate == 44100 && check_extensions(sf, "brstmspm")) //TODO remove
        vgmstream->sample_rate = 22050;

    vgmstream->layout_type = (channels == 1) ? layout_none : layout_interleave;
    switch(codec) {
        case 0:
            vgmstream->coding_type = coding_PCM8;
            break;
        case 1:
            vgmstream->coding_type = coding_PCM16BE;
            break;
        case 2:
            vgmstream->coding_type = coding_NGC_DSP;
            break;
        default:
            goto fail;
    }

    // TODO read hist
    if (vgmstream->coding_type == coding_NGC_DSP) {
        off_t coef_offset;
        off_t adpcm_header_offset;
        int i, ch;

        if (version == 0x0001) {
            /* standard */
            VGM_LOG("ss=%x\n", head_offset + 0x38);
            dsp_read_coefs_be(vgmstream, sf, head_offset + 0x38, 0x30);
        }
        else {
            uint32_t head_part3_offset = read_32bitBE(head_offset + 0x1c, sf);

            /* read from offset table */
            for (ch = 0; ch < vgmstream->channels; ch++) {
                adpcm_header_offset = head_offset + 0x08
                    + head_part3_offset + 0x04 /* skip over HEAD part 3 */
                    + ch * 0x08 /* skip to channel's ADPCM offset table */
                    + 0x04; /* ADPCM header offset field */

                coef_offset = head_offset + 0x08
                    + read_u32be(adpcm_header_offset, sf)
                    + 0x08; /* coeffs field */

                for (i = 0; i < 16; i++) {
                    vgmstream->ch[ch].adpcm_coef[i] = read_s16be(coef_offset + i * 2, sf);
                }
            }
        }
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
