#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"
#include <string.h>

/* .s14 and .sss - headerless siren14 stream (The Idolm@ster DS, Korogashi Puzzle Katamari Damacy DS) */
VGMSTREAM * init_vgmstream_s14_sss(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0;
    int channel_count, loop_flag = 0, interleave;


    /* check extension */
    if (check_extensions(streamFile,"sss")) {
        channel_count = 2;
    } else if (check_extensions(streamFile,"s14")) {
        channel_count = 1; //todo missing dual _0ch.s14 _1ch.s14, but dual_ext thing doesn't work properly with siren14 decoder
    } else {
        goto fail;
    }

    /* raw siren comes in 3 frame sizes, try to guess the correct one
     * (should try to decode and check the error flag but it isn't currently reported) */
    {
        char filename[PATH_LIMIT];
        streamFile->get_name(streamFile,filename,sizeof(filename));

        /* horrid but I ain't losing sleep over it (besides the header is often incrusted in-code as some tracks loop) */
        if (strstr(filename,"S037")==filename || strstr(filename,"b06")==filename || /* Korogashi Puzzle Katamari Damacy */
            strstr(filename,"_48kbps")!=NULL) /* Taiko no Tatsujin DS 1/2 */
            interleave = 0x78;
        else if (strstr(filename,"32700")==filename || /* Hottarake no Shima - Kanata to Nijiiro no Kagami */
                 strstr(filename,"b0")==filename || strstr(filename,"puzzle")==filename || strstr(filename,"M09")==filename || /* Korogashi Puzzle Katamari Damacy */
                 strstr(filename,"_32kbps")!=NULL) /* Taiko no Tatsujin DS 1/2 */
            interleave = 0x50;
        else
            interleave = 0x3c; /* The Idolm@ster - Dearly Stars */
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;
    vgmstream->num_samples = get_streamfile_size(streamFile) / (interleave * channel_count) * (32000/50);
    vgmstream->sample_rate = 32768; /* maybe 32700? */

    vgmstream->meta_type = channel_count==1 ? meta_S14 : meta_SSS;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    {
#ifdef VGM_USE_G7221
        vgmstream->coding_type = coding_G7221C;
        vgmstream->codec_data = init_g7221(vgmstream->channels, vgmstream->interleave_block_size);
        if (!vgmstream->codec_data) goto fail;
#else
    goto fail;
#endif
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
