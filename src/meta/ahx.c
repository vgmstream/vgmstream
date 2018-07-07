#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* AHX - CRI format mainly for voices, contains MPEG-2 Layer 2 audio with lying frame headers */
VGMSTREAM * init_vgmstream_ahx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count = 1, loop_flag = 0, type;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile, "ahx") ) goto fail;

    /* check first 2 bytes */
    if ((uint16_t)read_16bitBE(0,streamFile)!=0x8000) goto fail;

    /* get stream offset, check for CRI signature just before */
    start_offset = (uint16_t)read_16bitBE(0x02,streamFile) + 0x04;

    if ((uint16_t)read_16bitBE(start_offset-0x06,streamFile)!=0x2863 ||   /* "(c" */
        (uint32_t)read_32bitBE(start_offset-0x04,streamFile)!=0x29435249) /* ")CRI" */
       goto fail;

    /* check for encoding type (0x10 is AHX for DC with bigger frames, 0x11 is AHX, 0x0N are ADX) */
    type = read_8bit(0x04,streamFile);
    if (type != 0x10 && type != 0x11) goto fail;

    /* check for frame size (0 for AHX) */
    if (read_8bit(0x05,streamFile) != 0) goto fail;

    /* check for bits per sample? (0 for AHX) */
    if (read_8bit(0x06,streamFile) != 0) goto fail;

    /* check channel count (only mono AHXs can be created by the encoder) */
    if (read_8bit(0x07,streamFile) != 1) goto fail;

    /* check version signature */
    if (read_8bit(0x12,streamFile) != 0x06) goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitBE(0x08,streamFile); /* real sample rate */
    vgmstream->num_samples = read_32bitBE(0x0c,streamFile); /* doesn't include encoder_delay (handled in decoder) */

    vgmstream->meta_type = meta_AHX;

    {
#ifdef VGM_USE_MPEG
        mpeg_custom_config cfg = {0};

        cfg.encryption = read_8bit(0x13,streamFile); /* 0x08 = keyword encryption */
        cfg.cri_type = type;

        if (cfg.encryption) {
            uint8_t keybuf[6];
            if (read_key_file(keybuf, 6, streamFile) == 6) {
                cfg.cri_key1 = get_16bitBE(keybuf+0);
                cfg.cri_key2 = get_16bitBE(keybuf+2);
                cfg.cri_key3 = get_16bitBE(keybuf+4);
            }
        }

        vgmstream->layout_type = layout_none;
        vgmstream->codec_data = init_mpeg_custom(streamFile, start_offset, &vgmstream->coding_type, channel_count, MPEG_AHX, &cfg);
        if (!vgmstream->codec_data) goto fail;
#else
        goto fail;
#endif
    }


    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
