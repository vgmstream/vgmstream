#include "meta.h"
#include "../coding/coding.h"


/* EXST - from Sony games [Shadow of the Colossus (PS2), Gacha Mecha Stadium Saru Battle (PS2)] */
VGMSTREAM* init_vgmstream_exst(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;
    int32_t interleave, num_samples, loop_start, loop_end;
    size_t data_size;
    int is_cp3 = 0;


    /* checks */
    /* .sts+int: standard [Shadow of the Colossus (PS2)] (some fake .sts have manually joined header+body)
     * .x: header+body [Ape Escape 3 (PS2)]
     * .sts_cp3+int_cp3: Shadow of the Colossus (PS3) */
    if (!check_extensions(sf, "sts,sts_cp3,x"))
        goto fail;
    if (!is_id32be(0x00,sf, "EXST"))
        goto fail;

    /* also detectable since PS2 .sts uses blocks and PS3 offsets */
    is_cp3 = check_extensions(sf, "sts_cp3");

    if (is_cp3)
        sf_body = open_streamfile_by_ext(sf,"int_cp3");
    else
        sf_body = open_streamfile_by_ext(sf,"int");

    if (sf_body) {
        /* separate header+body (header is 0x78) */
        start_offset = 0x00;
        data_size = get_streamfile_size(sf_body);
    }
    else {
        /* joint header+body (.x and assumed .sts) */
        start_offset = 0x78;
        data_size = get_streamfile_size(sf);
        /* Gacharoku 2 has header+data but padded header (ELF has pointers + size to SOUND.PCK, and
         * treats them as single files, no extension but there are Sg2ExStAdpcm* calls in the ELF) */
        if ((data_size % 0x10) == 0)
            start_offset = 0x80;

        if (data_size <= start_offset)
            goto fail;

        data_size = data_size - start_offset;
    }

    channels    = read_u16le(0x06,sf);
    sample_rate = read_u32le(0x08,sf);
    loop_flag   = read_u32le(0x0C,sf);
    loop_start  = read_u32le(0x10,sf);
    loop_end    = read_u32le(0x14,sf);
    /* 0x18: 0x24 config per channel? (volume+panning+etc?) */
    /* rest is padding up to 0x78 */

    if (!is_cp3) {
        interleave = 0x400;
        loop_flag = (loop_flag == 1);

        num_samples = ps_bytes_to_samples(data_size, channels); /* same or very close to loop end */
        loop_start  = ps_bytes_to_samples(loop_start * interleave * channels, channels); /* blocks */
        loop_end    = ps_bytes_to_samples(loop_end * interleave * channels, channels); /* blocks */
    }
    else {
        interleave = 0x10;
        loop_flag = !(loop_start == 0 && loop_end == data_size); /* flag always set even for jingles */

        num_samples = ps_bytes_to_samples(data_size, channels); /* not same as loop end */
        loop_start  = ps_bytes_to_samples(loop_start, channels); /* offset */
        loop_end    = ps_bytes_to_samples(loop_end, channels); /* offset */
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_EXST;
    vgmstream->sample_rate = sample_rate;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    if (!vgmstream_open_stream(vgmstream, sf_body ? sf_body : sf, start_offset))
        goto fail;
    close_streamfile(sf_body);
    return vgmstream;

fail:
    close_streamfile(sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}
