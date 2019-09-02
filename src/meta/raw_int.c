#include "meta.h"
#include "../coding/coding.h"

/* raw PCM file assumed by extension [PaRappa The Rapper 2 (PS2)? , Amplitude (PS2)?] */
VGMSTREAM * init_vgmstream_raw_int(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count;

    /* checks */
    if (!check_extensions(streamFile, "int,wp2"))
        goto fail;

    if (check_extensions(streamFile, "int"))
        channel_count = 2;
    else
        channel_count = 4;

    /* ignore .int PS-ADPCM */
    if (ps_check_format(streamFile, 0x00, 0x10000))
        goto fail;

    start_offset = 0x00;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,0);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_RAW_INT;
    vgmstream->sample_rate = 48000;
    vgmstream->num_samples = pcm_bytes_to_samples(get_streamfile_size(streamFile), vgmstream->channels, 16);
    vgmstream->coding_type = coding_PCM16LE;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x200;

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
