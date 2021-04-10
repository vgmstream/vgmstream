#include "meta.h"
#include "../coding/coding.h"


/* EXST - from Sony games [Shadow of the Colossus (PS2), Gacha Mecha Stadium Saru Battle (PS2)] */
VGMSTREAM* init_vgmstream_ps2_exst(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;
    size_t block_size, num_blocks, loop_start_block;


    /* checks */
    /* .sts+int: standard [Shadow of the Colossus (PS2)] (some fake .sts have manually joined header+body)
     * .x: header+body [Ape Escape 3 (PS2)] */
    if (!check_extensions(sf, "sts,x"))
        goto fail;
    if (!is_id32be(0x00,sf, "EXST"))
        goto fail;

    sf_body = open_streamfile_by_ext(sf,"int");
    if (sf_body) {
        /* separate header+body (header is 0x78) */
        start_offset = 0x00;
    }
    else {
        /* joint header+body */
        start_offset = 0x78;
        /* Gacharoku 2 has header+data but padded header (ELF has pointers + size to SOUND.PCK, and
         * treats them as single files, no extension but there are Sg2ExStAdpcm* calls in the ELF) */
        if ((get_streamfile_size(sf) % 0x10) == 0)
            start_offset = 0x80;

        if (get_streamfile_size(sf) < start_offset)
            goto fail;
    }

    channels = read_u16le(0x06,sf);
    sample_rate = read_u32le(0x08,sf);
    loop_flag = read_u32le(0x0C,sf) == 1;
    loop_start_block = read_u32le(0x10,sf);
    num_blocks = read_u32le(0x14,sf);
    /* 0x18: 0x24 config per channel? (volume+panning+etc?) */
    /* rest is padding up to 0x78 */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PS2_EXST;
    vgmstream->sample_rate = sample_rate;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x400;

    block_size = vgmstream->interleave_block_size * vgmstream->channels;
    vgmstream->num_samples = ps_bytes_to_samples(num_blocks * block_size, channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start_block * block_size, channels);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    if (!vgmstream_open_stream(vgmstream, sf_body ? sf_body : sf, start_offset))
        goto fail;
    close_streamfile(sf_body);
    return vgmstream;

fail:
    close_streamfile(sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}
