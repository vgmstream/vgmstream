#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

#ifdef VGM_USE_MPEG

/* AHX is a CRI format which contains an MPEG-2 Layer 2 audio stream.
 * Although the MPEG frame headers are incorrect... */
VGMSTREAM * init_vgmstream_ahx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int channel_count = 1;
    int loop_flag = 0;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile, "ahx") ) goto fail;

    /* check first 2 bytes */
    if ((uint16_t)read_16bitBE(0,streamFile)!=0x8000) goto fail;

    /* get stream offset, check for CRI signature just before */
    start_offset = (uint16_t)read_16bitBE(2,streamFile) + 4;
    if ((uint16_t)read_16bitBE(start_offset-6,streamFile)!=0x2863 ||/* "(c" */
        (uint32_t)read_32bitBE(start_offset-4,streamFile)!=0x29435249 /* ")CRI" */
       ) goto fail;

    /* check for encoding type */
    /* 2 is for some unknown fixed filter, 3 is standard ADX, 4 is
     * ADX with exponential scale, 0x11 is AHX */
    /* Sappharad reports that old AHXs (Sonic Adventure 2) don't have this flag.
     * When I see one perhaps I can add an exception for those. */
    if (read_8bit(4,streamFile) != 0x11) goto fail;

    /* check for frame size (0 for AHX) */
    if (read_8bit(5,streamFile) != 0) goto fail;

    /* check for bits per sample? (0 for AHX) */
    if (read_8bit(6,streamFile) != 0) goto fail;

    /* check channel count (only mono AHXs are known) */
    if (read_8bit(7,streamFile) != 1) goto fail;

    /* At this point we almost certainly have an AHX file,
     * so let's build the VGMSTREAM. */

    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = read_32bitBE(0xc,streamFile);
    /* This is the One True Samplerate, the MPEG headers lie. */
    vgmstream->sample_rate = read_32bitBE(8,streamFile);

    vgmstream->coding_type = coding_fake_MPEG2_L2;
    vgmstream->layout_type = layout_fake_mpeg;
    vgmstream->meta_type = meta_AHX;
    vgmstream->codec_data = init_mpeg_codec_data_ahx(streamFile, start_offset, channel_count);

    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#endif
