#include "meta.h"
#include "../coding/coding.h"

/* .PCM - KCE Japan East PS2 games (Ephemeral Fantasia, Yu-Gi-Oh! The Duelists of the Roses, 7 Blades) */
VGMSTREAM * init_vgmstream_ps2_pcm(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;

    /* check extension */
    if ( !check_extensions(streamFile,"pcm") )
        goto fail;

    /* check header (data_size vs num_samples) */
    if (pcm_bytes_to_samples(read_32bitLE(0x00,streamFile), 2, 16) != read_32bitLE(0x04,streamFile))
        goto fail;
    /* should work too */
    //if (read_32bitLE(0x00,streamFile)+0x800 != get_streamfile_size(streamFile))
    //    goto fail;

    loop_flag = (read_32bitLE(0x0C,streamFile) != 0x00);
    channel_count = 2;
    start_offset = 0x800;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 24000;
    vgmstream->num_samples = read_32bitLE(0x04,streamFile);
    vgmstream->loop_start_sample = read_32bitLE(0x08,streamFile);
    vgmstream->loop_end_sample = read_32bitLE(0x0C,streamFile);

    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x2;
    vgmstream->meta_type = meta_PS2_PCM;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
