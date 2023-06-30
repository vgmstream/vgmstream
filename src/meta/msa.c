#include "meta.h"
#include "../coding/coding.h"

/* MSA - from Sucess games [Psyvariar -Complete Edition- (PS2), Konohana Pack: 3tsu no Jikenbo (PS2)]*/
VGMSTREAM* init_vgmstream_msa(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, data_size, channel_size, file_size;
    int loop_flag, channels;


    /* checks */
    if (read_u32be(0x00,sf) != 0x00000000 || read_u32be(0x08,sf) != 0x00000000)
        return NULL;
    if (!check_extensions(sf, "msa"))
        return NULL;

    loop_flag = 0;
    channels = 2;
    start_offset = 0x14;

    file_size = get_streamfile_size(sf);
    data_size = read_u32le(0x04,sf); /* wrong, see below */
    channel_size = read_u32le(0x0c,sf); /* also wrong like data_size */

    if (!ps_check_format(sf, start_offset, 0x100))
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MSA;
    vgmstream->sample_rate = read_s32le(0x10,sf);
    if (vgmstream->sample_rate == 0) /* ex. Psyvariar's AME.MSA */
        vgmstream->sample_rate = 44100;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;

    if (channel_size) /* Konohana Pack */
        vgmstream->interleave_block_size = 0x6000;
    else /* Psyvariar */
        vgmstream->interleave_block_size = 0x4000;

    /* MSAs are strangely truncated, so manually calculate samples.
     * Data after last usable block is always silence or garbage. */
    if (data_size > file_size) {
        uint32_t usable_size = file_size - start_offset;
        usable_size -= usable_size % (vgmstream->interleave_block_size * channels); /* block-aligned */
        vgmstream->num_samples = ps_bytes_to_samples(usable_size, channels);
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
