#include "meta.h"
#include "../coding/coding.h"


/* DCS+WAV - from In Utero games [Evil Twin: Cyprien's Chronicles (DC)] */
VGMSTREAM * init_vgmstream_dcs_wav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    int loop_flag, channel_count, sample_rate;
    off_t start_offset, fmt_offset;


    /* checks */
    if (!check_extensions(streamFile,"dcs"))
        goto fail;

    streamHeader = open_streamfile_by_ext(streamFile, "wav");
    if (!streamHeader) goto fail;

    /* a slightly funny RIFF */
    if (read_u32be(0x00,streamHeader) != 0x52494646 || /* "RIFF" */
        read_u32be(0x08,streamHeader) != 0x57415645 || /* "WAVE" */
        read_u32be(0x0C,streamHeader) != 0x34582E76 || /* "4X.v" */
        read_u32be(0x3C,streamHeader) != 0x406E616D)   /* "@nam" */
        goto fail;

    fmt_offset = 0x44 + read_32bitLE(0x40,streamHeader); /* skip @nam */
    if (fmt_offset % 2) fmt_offset += 1;
    if (read_u32be(fmt_offset,streamHeader) != 0x666D7420) goto fail; /* "fmt " */
    fmt_offset += 0x04+0x04;

    if (read_u16le(fmt_offset+0x00,streamHeader) != 0x0005) goto fail; /* unofficial format */
    channel_count = read_u16le(fmt_offset+0x02,streamHeader);
    sample_rate   = read_u32le(fmt_offset+0x04,streamHeader);
    loop_flag = 0;
    start_offset = 0x00;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_DCS_WAV;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = yamaha_bytes_to_samples(get_streamfile_size(streamFile), channel_count);
    vgmstream->coding_type = coding_AICA_int;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x4000;
    
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    close_streamfile(streamHeader);
    return vgmstream;

fail:
    close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}
