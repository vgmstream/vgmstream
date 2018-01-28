#include "meta.h"
#include "../coding/coding.h"

/* VXN - from Gameloft mobile games */
VGMSTREAM * init_vgmstream_vxn(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int loop_flag = 0, channel_count, codec, sample_rate, block_align, bits, num_samples;
    off_t start_offset, stream_offset, chunk_offset, first_offset = 0x00;
    size_t stream_size;
    int total_subsongs, target_subsong = streamFile->stream_index;

    /* check extensions */
    if (!check_extensions(streamFile,"vxn"))
        goto fail;

    /* check header/version chunk (RIFF-like format with many custom chunks) */
    if (read_32bitBE(0x00,streamFile) != 0x566F784E) /* "VoxN" */
        goto fail;
    if (read_32bitLE(0x10,streamFile) != get_streamfile_size(streamFile) )
        goto fail;

    if (!find_chunk_le(streamFile, 0x41666D74,first_offset,0, &chunk_offset,NULL)) /* "Afmt" */
        goto fail;
    codec = (uint16_t)read_16bitLE(chunk_offset+0x00, streamFile);
    channel_count = (uint16_t)read_16bitLE(chunk_offset+0x02, streamFile);
    sample_rate = read_32bitLE(chunk_offset+0x04, streamFile);
    block_align = (uint16_t)read_16bitLE(chunk_offset+0x08, streamFile);
    bits = (uint16_t)read_16bitLE(chunk_offset+0x0a, streamFile);

    /* files are divided into segment subsongs, often a leadout and loop in that order
     * (the "Plst" and "Rule" chunks may have order info) */
    if (!find_chunk_le(streamFile, 0x5365676D,first_offset,0, &chunk_offset,NULL))  /* "Segm" */
        goto fail;
    total_subsongs = read_32bitLE(chunk_offset+0x00, streamFile);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    stream_offset = read_32bitLE(chunk_offset+0x04 + (target_subsong-1)*0x18 + 0x00, streamFile);
    stream_size   = read_32bitLE(chunk_offset+0x04 + (target_subsong-1)*0x18 + 0x04, streamFile);
    num_samples   = read_32bitLE(chunk_offset+0x04 + (target_subsong-1)*0x18 + 0x08, streamFile);

    if (!find_chunk_le(streamFile, 0x44617461,first_offset,0, &chunk_offset,NULL)) /* "Data" */
        goto fail;
    start_offset = chunk_offset + stream_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_VXN;

    switch (codec) {
        case 0x0001:    /* PCM */
            if (bits != 16) goto fail;

            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->interleave_block_size = block_align;
            vgmstream->layout_type = layout_interleave;
            break;

        case 0x0002:    /* MSADPCM (ex. Asphalt 7) */
            if (bits != 4) goto fail;

            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->interleave_block_size = block_align;
            vgmstream->layout_type = layout_none;
            break;

        case 0x0011:    /* MS-IMA (ex. Asphalt 6) */
            if (bits != 16) goto fail;

            vgmstream->coding_type = coding_MS_IMA;
            vgmstream->interleave_block_size = block_align;
            vgmstream->layout_type = layout_none;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0800: {  /* Musepack (ex. Asphalt Xtreme) */
            ffmpeg_codec_data * ffmpeg_data = NULL;
            if (bits != 0xFFFF) goto fail;

            ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset,stream_size);
            if (!ffmpeg_data) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        default:
            VGM_LOG("VXN: unknown codec 0x%02x\n", codec);
            goto fail;
    }

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
