#include "meta.h"
#include "../coding/coding.h"

/* ASD - found Naxat (Spiel/Mesa) games [Miss Moonlight (DC), Yoshia no Oka de Nekoronde... (DC)] */
VGMSTREAM* init_vgmstream_asd_naxat(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int loop_flag, channels, sample_rate;


    /* checks */
    uint32_t data_size = read_u32le(0x00, sf); /* padded and slightly less than file size */
    if (data_size == 0 || data_size >= get_streamfile_size(sf) || data_size + 0x20 + 0x10 < get_streamfile_size(sf))
        return NULL;
    if (data_size != read_u32le(0x04,sf)) /* repeated size */
        return NULL;
    /* extension of the audio bigfiles (there are N offsets to these subfiles) */
    if (!check_extensions(sf, "asd"))
        return NULL;

    /* fmt chunk, extra checks since format is simple */
    if (read_u16le(0x08,sf) != 0x01) /* format*/
        return NULL;
    channels = read_u16le(0x0a,sf);
    sample_rate = read_s32le(0x0c,sf);

    if (channels < 1 || channels > 2)
        return NULL;
    if (sample_rate != 22050)
        return NULL;
    if (sample_rate * channels * sizeof(int16_t) != read_u32le(0x10,sf)) /* bitrate */
        return NULL;
    /* 04: block size, bps */
    if (read_u32le(0x18,sf) != 0x00 )
        return NULL;
    if (read_u32le(0x1c,sf) != 0x00 )
        return NULL;

    start_offset = 0x20;
    loop_flag = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ASD_NAXAT;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm16_bytes_to_samples(data_size, channels);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x02;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
