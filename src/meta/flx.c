#include "meta.h"
#include "../coding/coding.h"

/* FLX - from Ultima IX (.FLX is actually an archive format with sometimes sound data, let's support both anyway) */
VGMSTREAM * init_vgmstream_flx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, stream_offset = 0;
    size_t data_size;
    int loop_flag, channel_count, codec;
    int total_subsongs = 0, target_subsong = streamFile->stream_index;
    size_t stream_size = 0;


    /* check extensions (.flx: name of archive, files inside don't have extensions) */
    if (!check_extensions(streamFile,"flx"))
        goto fail;

    /* all spaces up to 0x50 = archive FLX */
    if (read_32bitBE(0x00,streamFile) == 0x20202020 && read_32bitBE(0x40,streamFile) == 0x20202020) {
        int i;
        int entries = read_32bitLE(0x50,streamFile);
        off_t offset = 0x80;

        if (read_32bitLE(0x54,streamFile) != 0x02
                || read_32bitLE(0x58,streamFile) != get_streamfile_size(streamFile))
            goto fail;

        if (target_subsong == 0) target_subsong = 1;

        for (i = 0; i < entries; i++) {
            off_t entry_offset = read_32bitLE(offset + 0x00, streamFile);
            size_t entry_size = read_32bitLE(offset + 0x04, streamFile);
            offset += 0x08;

            if (entry_offset != 0x00)
                total_subsongs++; /* many entries are empty */
            if (total_subsongs == target_subsong && stream_offset == 0) {
                stream_offset = entry_offset; /* found but let's keep adding total_streams */
                stream_size = entry_size;
            }
        }
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
        if (stream_offset == 0x00) goto fail;
    }
    else {
        stream_offset = 0x00;
        stream_size = get_streamfile_size(streamFile);
    }

    if (read_32bitLE(stream_offset + 0x30,streamFile) != 0x10)
        goto fail;
    data_size = read_32bitLE(stream_offset + 0x28,streamFile);
    channel_count = read_32bitLE(stream_offset + 0x34,streamFile);
    codec = read_32bitLE(stream_offset + 0x38,streamFile);
    loop_flag = (channel_count > 1); /* full seamless repeats in music */
    start_offset = stream_offset + 0x3c;
    /* 0x00: id */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(stream_offset + 0x2c,streamFile);
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_PC_FLX;

    switch(codec) {
        case 0x00:  /* PCM (sfx) */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channel_count, 16);
            break;

        case 0x01:  /* EA-XA (music, sfx) */
            vgmstream->coding_type = channel_count > 1 ? coding_EA_XA : coding_EA_XA_int;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = read_32bitLE(stream_offset + 0x28,streamFile) / 0x0f*channel_count * 28; /* ea_xa_bytes_to_samples */
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

        case 0x02:  /* EA-MT (voices) */
            vgmstream->coding_type = coding_EA_MT;
            vgmstream->layout_type = layout_none;
            vgmstream->codec_data = init_ea_mt(vgmstream->channels, 0);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->num_samples = read_32bitLE(start_offset,streamFile);
            start_offset += 0x04;
            break;

        default:
            VGM_LOG("FLX: unknown codec 0x%x\n", codec);
            goto fail;
    }

    read_string(vgmstream->stream_name,0x20+1, stream_offset + 0x04,streamFile);


    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
