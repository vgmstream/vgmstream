#include "meta.h"
#include "../coding/coding.h"


/* .MIO - Entis's 'Music Interleaved and Orthogonal transformed' [HAYABUSA (PC), Rakuen no Kantai (PC/Android)] */
VGMSTREAM* init_vgmstream_mio(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset; 

    /* checks */
    if (!is_id64be(0x00,sf, "Entis\x1a\x00\x00"))
        return NULL;
    // all of Entis's formats have long text descriptors, will be re-checked during codec init
    if (!is_id64be(0x10,sf, "Music In")) 
        return NULL;

    if (!check_extensions(sf,"mio"))
        return NULL;


    // get info (abridged), could use lib though
    int channels = read_s32le(0x90,sf);
    int sample_rate = 44100; // there is input rate at 0x94 put output is fixed
    int32_t num_samples = read_s32le(0xA0,sf);

    // loops are in UTF16 tags, kinda annoying to read so get from lib below
    int32_t loop_start = 0;
    bool loop_flag = 1; //(loop_start > 0);

    start_offset = 0x00;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MIO;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    // .mio has multiple modes (lapped/huffman/lossless) but not that interesting to print as info
    vgmstream->codec_data = init_mio(sf, &loop_start);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_MIO;
    vgmstream->layout_type = layout_none;

    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = num_samples;
    vgmstream->loop_flag = (loop_start >= 0);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
