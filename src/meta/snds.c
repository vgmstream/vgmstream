#include "meta.h"
#include "../coding/coding.h"
#include "../util/chunks.h"


/* SSDD - Sony/SCE's SNDS lib format (cousin of SGXD/SNDX) */
VGMSTREAM* init_vgmstream_snds(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t stream_offset, stream_size;
    int loop_flag, channels, codec, sample_rate;
    int32_t num_samples, loop_start, loop_end, encoder_delay;
    uint32_t at9_config = 0;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00, sf, "SSDD"))
        return NULL;

    if (read_u32le(0x04, sf) != get_streamfile_size(sf))
        return NULL;
    /* 0x10: file name */

    /* (extensionless): no apparent extension in debug strings, though comparing other SCE libs possibly ".ssd" */
    if (!check_extensions(sf,""))
        return NULL;

    /* from debug info seems to have free chunks but known files always use the same and 1 subsong */
    off_t base_offset = 0x60, wavs_offset = 0;
    if (!find_chunk_le(sf, get_id32be("WAVS"),base_offset,0, &wavs_offset,NULL))
        return NULL;

    if (read_u16le(wavs_offset + 0x00, sf) != 0x2c) /* entry size? */
        return NULL;

    total_subsongs = read_s16le(wavs_offset + 0x02, sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) return NULL;
    if (total_subsongs != 1) return NULL; /* not seen */

    /* read stream header */
    {
        uint32_t head_offset = wavs_offset + 0x04 + 0x2c * (target_subsong - 1);
        /* 0x00: null/flags? */
        /* 0x04: null/offset? */
        /* 0x0c: null/offset? */
        codec           =    read_u8(head_offset + 0x0c, sf);
        channels        = read_u16le(head_offset + 0x0d, sf);
        /* 0x0e: null? */
        sample_rate     = read_u32le(head_offset + 0x10, sf);
        at9_config      = read_u32le(head_offset + 0x14, sf); /* !!! (only known use of this lib is Android/iOS) */
        num_samples     = read_s32le(head_offset + 0x18, sf);
        loop_start      = read_s32le(head_offset + 0x1c, sf);
        loop_end        = read_s32le(head_offset + 0x20, sf);
        encoder_delay   = read_s32le(head_offset + 0x24, sf);
        stream_size     = read_u32le(head_offset + 0x28, sf);

        loop_flag = loop_end > 0;
    }

    /* CUES chunk: cues (various fields, also names) */
    /* CUNS chunk: cue names (flags + hash + offset, then names) */

    off_t wavd_offset = 0;
    if (!find_chunk_le(sf, get_id32be("WAVD"),wavs_offset - 0x08,0, &wavd_offset,NULL))
        return NULL;
    stream_offset = wavd_offset + 0x08;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SNDS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    switch (codec) {
#ifdef VGM_USE_ATRAC9
        case 0x41: {
            atrac9_config cfg = {0};

            cfg.channels = channels;
            cfg.config_data = at9_config;
            cfg.encoder_delay = encoder_delay;

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif
        default:
            VGM_LOG("SNDS: unknown codec 0x%x\n", codec);
            goto fail;
    }

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, sf, stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
