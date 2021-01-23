#include "meta.h"
#include "../coding/coding.h"


/* .CWAV - from CompressWave lib, found in few Japanese (doujin?) games around 1995-2002 [RADIO ZONDE (PC), GEO ~The Sword Millennia~ (PC)] */
VGMSTREAM* init_vgmstream_compresswave(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;


    /* checks */
    if (!check_extensions(sf, "cwav"))
        goto fail;

    if (!is_id64be(0x00,sf, "CmpWave\0"))
        goto fail;

    channels = 2; /* always, header channels is internal config */
    start_offset = 0x00;
    loop_flag = 1; //read_u8(0x430, sf) != 0; /* wrong count, see below */
    /* codec allows to use a cipher value, not seen */
    /* there is also title and artist, but default to "UnTitled" / "NoName" */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_COMPRESSWAVE;
    vgmstream->sample_rate = 44100; /* always, header rate is internal config */
    /* in PCM bytes */
    vgmstream->num_samples       = read_u64le(0x418, sf) / sizeof(int16_t) / channels;
    /* known files have wrong loop values and just repeat */
    vgmstream->loop_start_sample = 0; //read_u64le(0x420, sf) / sizeof(int16_t) / channels;
    vgmstream->loop_end_sample   = vgmstream->num_samples; //read_u64le(0x428, sf) / sizeof(int16_t) / channels;

    vgmstream->codec_data = init_compresswave(sf);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_COMPRESSWAVE;
    vgmstream->layout_type = layout_none;

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
