#include "meta.h"
#include "../coding/coding.h"

/* XA30 - found in Reflections games [Driver: Parallel Lines (PC), Driver 3 (PC)] */
VGMSTREAM * init_vgmstream_xa_xa30(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, codec;
    size_t stream_size;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* check extension, case insensitive */
    /* ".xa30/e4x" is just the ID, the real filename should be .XA */
    if (!check_extensions(streamFile,"xa,xa30,e4x"))
        goto fail;

    /* check header */
    if (read_32bitBE(0x00,streamFile) != 0x58413330 &&   /* "XA30" [Driver: Parallel Lines (PC)]*/
        read_32bitBE(0x00,streamFile) != 0x65347892)     /* "e4x\92" [Driver 3 (PC)]*/
        goto fail;
    if (read_32bitLE(0x04,streamFile) != 2) /* channels?, also extra check to avoid PS2/PC XA30 mixup */
        goto fail;

    total_subsongs = read_32bitLE(0x14,streamFile) != 0 ? 2 : 1; /* second stream offset (only in Driver 3) */
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    loop_flag = 0;
    channel_count = 2; /* 0x04: channels? (always 2 in practice) */
    codec = read_32bitLE(0x0c,streamFile);
    start_offset = read_32bitLE(0x10 + 0x04*(target_subsong-1),streamFile);
    stream_size  = read_32bitLE(0x18 + 0x04*(target_subsong-1),streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(0x08,streamFile);
    /* 0x20: always IMA=00016000, PCM=00056000 PCM?, rest of the header is null */
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_XA_XA30;

    switch(codec) {
        case 0x00:   /* PCM (rare, seen in Driver 3) */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = read_32bitLE(0x24,streamFile) / 2;
            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, channel_count, 16);
            break;

        case 0x01:   /* MS-IMA variation */
            vgmstream->coding_type = coding_REF_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = read_32bitLE(0x24,streamFile);
            vgmstream->num_samples = ms_ima_bytes_to_samples(stream_size, vgmstream->interleave_block_size, channel_count);
            break;

        default:
           goto fail;
    }


    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
