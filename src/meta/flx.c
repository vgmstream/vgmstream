#include "meta.h"
#include "../coding/coding.h"

/* .FLX - from Ultima IX (PC) */
VGMSTREAM* init_vgmstream_flx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, stream_offset = 0;
    size_t data_size;
    int loop_flag, channels, codec;
    int total_subsongs = 0, target_subsong = sf->stream_index;
    size_t stream_size = 0;


    /* checks */
    /* .flx: name of archive (filenames inside don't have extensions) */
    if (!check_extensions(sf,"flx,"))
        return NULL;

    // .FLX an archive format with sometimes sound data, let's support both anyway
    // all spaces up to 0x50 = archive FLX
    if (is_id32be(0x00,sf, "    ") && is_id32be(0x40,sf, "    ")) {
        int entries = read_s32le(0x50,sf);
        if (read_u32le(0x54,sf) != 0x02)
            return NULL;
        if (read_u32le(0x58,sf) != get_streamfile_size(sf))
            return NULL;

        if (target_subsong == 0) target_subsong = 1;

        uint32_t offset = 0x80;
        for (int i = 0; i < entries; i++) {
            off_t entry_offset  = read_u32le(offset + 0x00, sf);
            size_t entry_size   = read_u32le(offset + 0x04, sf);
            offset += 0x08;

            if (entry_offset != 0x00)
                total_subsongs++; /* many entries are empty */
            if (total_subsongs == target_subsong && stream_offset == 0) {
                stream_offset = entry_offset; /* found but let's keep adding total_streams */
                stream_size = entry_size;
            }
        }
        if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
        if (stream_offset == 0x00)
            return NULL;
    }
    else {
        stream_offset = 0x00;
        stream_size = get_streamfile_size(sf);
    }

    // file ID, can be a bit higher in sfx packs
    if (read_u32le(stream_offset + 0x00,sf) >= 0x10000)
        return NULL;
    // 04: filename
    if (read_u32le(stream_offset + 0x30,sf) != 0x10)
        return NULL;
    data_size = read_u32le(stream_offset + 0x28,sf);
    channels = read_s32le(stream_offset + 0x34,sf);
    codec = read_u32le(stream_offset + 0x38,sf);
    loop_flag = (channels > 1); /* full seamless repeats in music */
    start_offset = stream_offset + 0x3c;
    /* 0x00: id */

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(stream_offset + 0x2c,sf);
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_PC_FLX;

    switch(codec) {
        case 0x00:  /* PCM (sfx) */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channels, 16);
            break;

        case 0x01:  /* EA-XA (music, sfx) */
            vgmstream->coding_type = channels > 1 ? coding_EA_XA : coding_EA_XA_int;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = read_32bitLE(stream_offset + 0x28,sf) / 0x0f*channels * 28; /* ea_xa_bytes_to_samples */
            vgmstream->loop_start_sample = 0;
            vgmstream->loop_end_sample = vgmstream->num_samples;
            break;

        case 0x02:  /* EA-MT (voices) */
            vgmstream->coding_type = coding_EA_MT;
            vgmstream->layout_type = layout_none;
            vgmstream->codec_data = init_ea_mt(vgmstream->channels, 0);
            if (!vgmstream->codec_data) goto fail;

            vgmstream->num_samples = read_32bitLE(start_offset,sf);
            start_offset += 0x04;
            break;

        default:
            VGM_LOG("FLX: unknown codec 0x%x\n", codec);
            goto fail;
    }

    read_string(vgmstream->stream_name,0x20+1, stream_offset + 0x04,sf);


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
