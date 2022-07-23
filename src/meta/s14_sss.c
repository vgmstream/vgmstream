#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"
#include <string.h>

static int test_interleave(STREAMFILE* sf, int channels, int interleave);

/* .s14/.sss - headerless siren14 stream [The Idolm@ster (DS), Korogashi Puzzle Katamari Damacy (DS), Taiko no Tatsujin DS 1/2 (DS)] */
VGMSTREAM* init_vgmstream_s14_sss(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset = 0;
    int channels, loop_flag = 0, interleave;


    /* check extension */
    if (check_extensions(sf,"sss")) {
        channels = 2;
    } else if (check_extensions(sf,"s14")) {
        channels = 1; /* may have dual _0ch.s14 + _1ch.s14, needs .txtp */
    } else {
        goto fail;
    }

    /* raw siren comes in 3 frame sizes, try to guess the correct one */
    {
        /* horrid but ain't losing sleep over it (besides the header is often incrusted in-code as some tracks loop)
         * Katamari, Taiko = 0x78/0x50, idolmaster=0x3c (usually but can be any) */
        if (test_interleave(sf, channels, 0x78))
            interleave = 0x78;
        else if (test_interleave(sf, channels, 0x50))
            interleave = 0x50; 
        else if (test_interleave(sf, channels, 0x3c))
            interleave = 0x3c;
        else
            goto fail;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;
    vgmstream->num_samples = get_streamfile_size(sf) / (interleave * channels) * (32000/50);
    vgmstream->sample_rate = 32768;

    vgmstream->meta_type = channels==1 ? meta_S14 : meta_SSS;
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

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* pretty gross (should use TXTH), but codec info seems to be in hard-to-locate places/exe
 * and varies per file, so for now autodetect possible types. could also check if data_size matches interleave */
static int test_interleave(STREAMFILE* sf, int channels, int interleave) {
#ifdef VGM_USE_G7221
    int res;
    g7221_codec_data* data = init_g7221(channels, interleave);
    if (!data) goto fail;

    set_key_g7221(data, NULL); /* force test key */

    /* though this is mainly for key testing, with no key can be used to test frames too */
    res = test_key_g7221(data, 0x00, sf);
    if (res <= 0) goto fail;

    free_g7221(data);
    return 1;
fail:
    free_g7221(data);
    return 0;
#else
    return 0;
#endif
}
