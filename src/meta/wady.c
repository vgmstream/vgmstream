#include "meta.h"
#include "../coding/coding.h"

/* WADY - from Marble engine games [Eien no Owari ni (PC), Elf no Futagohime (PC)] */
VGMSTREAM* init_vgmstream_wady(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate, scale, num_samples;
    size_t codec_size;


    /* checks */
    /* .way/extensionless: found in some bigfiles */
    if (!check_extensions(sf, "way,"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x57414459) /* "WADY" */
        goto fail;

    /* 0x04: none */
    scale = read_u8(0x05,sf);
    channels = read_u16le(0x06,sf);
    sample_rate = read_s32le(0x08,sf);
    codec_size = read_u32le(0x0c,sf);
    num_samples = read_s32le(0x10,sf);
    /* 0x14: PCM size? */
    /* 0x18/1c: null */
    /* 0x20: fmt-like subheader (codec/channels/srate/bitrate/bps/spf) */

    start_offset = 0x30;
    loop_flag  = 0;

    //todo implement
    /* codec variation used in SFX */
    if (codec_size + start_offset != get_streamfile_size(sf))
        goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_WADY;
    vgmstream->sample_rate = sample_rate;
    vgmstream->coding_type = coding_WADY;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x01;
    vgmstream->num_samples = num_samples;

    {
        int i;
        for (i = 0; i < channels; i++) {
           vgmstream->ch[i].adpcm_scale = scale;
        }
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
