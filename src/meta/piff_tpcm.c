#include "meta.h"
#include "../coding/coding.h"


/* PIFF TADH - from Tantalus games [House of the Dead (SAT)] */
VGMSTREAM* init_vgmstream_piff_tpcm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, header_offset, data_size;
    int loop_flag, channels, sample_rate;


    /* checks */
    /* .tad: from internal filenames */
    if (!check_extensions(sf, "tad"))
        goto fail;
    /* Tantalus also has PIFF without this */
    if (!is_id32be(0x00,sf, "PIFF") || !is_id32be(0x08,sf, "TPCM") || !is_id32be(0x0c,sf, "TADH"))
        goto fail;

    header_offset = 0x14;
    /* 0x00: 1? */
    /* 0x01: 1? */
    channels = read_u16le(header_offset + 0x02,sf);
    sample_rate = read_s32le(header_offset + 0x04,sf);
    /* 0x08+: ? (mostly fixed, maybe related to ADPCM?) */
    loop_flag  = 0;

    if (!is_id32be(0x38,sf, "BODY"))
        goto fail;
    start_offset = 0x40;
    data_size = read_u32le(0x3c,sf);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_PIFF_TPCM;
    vgmstream->sample_rate = sample_rate;

    vgmstream->coding_type = coding_TANTALUS;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x10;

    vgmstream->num_samples = tantalus_bytes_to_samples(data_size, channels);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
