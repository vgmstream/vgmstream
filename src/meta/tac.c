#include "meta.h"
#include "../coding/coding.h"

/* tri-Ace codec file [Star Ocean 3 (PS2), Valkyrie Profile 2 (PS2), Radiata Stories (PS2)] */
VGMSTREAM* init_vgmstream_tac(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag, channel_count;
    uint16_t loop_frame, frame_count, loop_discard, frame_last;
    uint32_t info_offset, loop_offset, stream_size, file_size;
    off_t start_offset;


    /* checks */
    /* (extensionless): bigfiles have no known names (libs calls mention "St*" and "Sac*" though)
     * .aac: fake for convenience given it's a tri-Ace AAC's grandpa (but don't use unless you must)
     * .pk3/.20: extremely ugly fake extensions randomly given by an old extractor, *DON'T* */
    if (!check_extensions(sf, ",aac,laac"))
        goto fail;
    /* file is validated on decoder init, early catch of simple errors (see tac_decoder_lib.h for full header) */
    info_offset = read_u32le(0x00,sf);
    if (info_offset > 0x4E000 || info_offset < 0x20) /* offset points to value inside first "block" */
        goto fail;
    loop_frame      = read_u16le(0x08,sf);
    loop_discard    = read_u16le(0x0a,sf);
    frame_count     = read_u16le(0x0c,sf);
    frame_last      = read_u16le(0x0e,sf);
    loop_offset     = read_u32le(0x10,sf);
    stream_size     = read_u32le(0x14,sf);
    if (stream_size % 0x4E000 != 0) /* multiple of blocks */
        goto fail;

    /* actual file can truncate last block */
    file_size = get_streamfile_size(sf);
    if (file_size > stream_size || file_size < stream_size - 0x4E000)
        goto fail;

    channel_count = 2; /* always stereo */
    loop_flag = (loop_offset != stream_size); /* actual check may be loop_frame > 0? */
    start_offset = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_TAC;
    vgmstream->sample_rate = 48000;

    /* Frame at count/loop outputs less than full 1024 samples (thus loop or count-1 + extra).
     * A few files may pop when looping, but this seems to match game/emulator. */
    vgmstream->num_samples = (frame_count - 1) * 1024 + (frame_last + 1);
    vgmstream->loop_start_sample = (loop_frame - 1) * 1024 + loop_discard;
    vgmstream->loop_end_sample = vgmstream->num_samples;

    {
        vgmstream->codec_data = init_tac(sf);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_TAC;
        vgmstream->layout_type = layout_none;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
