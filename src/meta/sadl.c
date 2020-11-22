#include "meta.h"
#include "../coding/coding.h"


/* sadl - from DS games with Procyon Studio audio driver [Professor Layton (DS), Soma Bringer (DS)] */
VGMSTREAM* init_vgmstream_sadl(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int channels, loop_flag;
    off_t start_offset;
    uint8_t flags;
    uint32_t loop_start, data_size;


    /* checks */
    if (!check_extensions(sf, "sad"))
        goto fail;

    if (read_u32be(0x00,sf) != 0x7361646c) /* "sadl" */
        goto fail;
    /* 04: null */
    /* 08: data size, or null in later files */
    /* 0c: version? (x0410=Luminous Arc, 0x0411=Layton, 0x0415=rest) */
    /* 0e: file id (for .sad packed in .spd) */
    /* 14: name related? */
    /* 20: short filename (may be null or nor match full filename) */

    /* 30: flags? (0/1/2) */
    loop_flag = read_u8(0x31,sf);
    channels = read_u8(0x32,sf);
    flags = read_u8(0x33,sf);
    /* 34: flags? */
    /* 38: flags? */
    /* 3c: null? */
    data_size = read_u32le(0x40,sf); //?
    start_offset = read_u32le(0x48,sf); /* usually 0x100, 0xc0 in LA */
    /* 4c: start offset again or 0x40 in LA */
    /* 50: size or samples? */
    loop_start = read_u32le(0x54,sf); //?
    /* others: sizes/samples/flags? */

    data_size -= start_offset;
    loop_start -= start_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SADL;

    switch (flags & 6) { /* possibly > 1? (0/1/2) */
        case 4:
            vgmstream->sample_rate = 32728;
            break;
        case 2: /* Layton */
        case 0: /* Luminous Arc (DS) */
            vgmstream->sample_rate = 16364;
            break;
        default:
            goto fail;
    }

    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;

    switch(flags & 0xf0) { /* possibly >> 6? (0/1/2) */
        case 0x00: /* Luminous Arc (DS) (non-int IMA? all files are mono though) */
        case 0x70: /* Ni no Kuni (DS), Professor Layton and the Curious Village (DS), Soma Bringer (DS) */
            vgmstream->coding_type = coding_IMA_int;

            vgmstream->num_samples = ima_bytes_to_samples(data_size, channels);
            vgmstream->loop_start_sample = ima_bytes_to_samples(loop_start, channels);
            vgmstream->loop_end_sample = vgmstream->num_samples;

            {
                int i;
                for (i = 0; i < channels; i++) {
                    vgmstream->ch[i].adpcm_history1_32 = read_s16le(0x80 + i*0x04 + 0x00, sf);
                    vgmstream->ch[i].adpcm_step_index = read_s16le(0x80 + i*0x04 + 0x02, sf);
                }
            }
            break;

        //TODO: Luminous Arc 2 uses a variation of this, but value 0x70
        case 0xb0: /* Soma Bringer (DS), Rekishi Taisen Gettenka (DS) */
            vgmstream->coding_type = coding_NDS_PROCYON;

            vgmstream->num_samples = data_size / channels / 16 * 30;
            vgmstream->loop_start_sample = loop_start / channels / 16 *30;
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
