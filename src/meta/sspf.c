#include "meta.h"


/* SSPF - Konami/KCET banks [Metal Gear Solid 4 (PS3)] */
VGMSTREAM* init_vgmstream_sspf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate;
    int32_t num_samples, loop_start;
    int total_subsongs, target_subsong = sf->stream_index;
    uint32_t file_size, pad_size, offset, bwav_offset, iwav_offset, ssw2_offset, stream_size;
    uint32_t codec;


    /* checks */
    if (!is_id32be(0x00,sf, "SSPF"))
        goto fail;
    if (!check_extensions(sf, "ssp"))
        goto fail;

    /* extra check to ignore .spc, that are a RAM pack of .ssp with a ~0x800 table at the end */
    file_size = read_u32be(0x08, sf) + 0x08; /* without padding */
    pad_size = 0;
    if (file_size % 0x800) /* add padding */
        pad_size = 0x800 - (file_size % 0x800);
    if (file_size != get_streamfile_size(sf) && file_size + pad_size != get_streamfile_size(sf))
        goto fail;
    /* 0x0c: "loadBank"? (always 2? MTA2 is always 1) */

    /* read chunks (fixed order) */
    bwav_offset = read_u32be(0x04, sf) + 0x08;
    if (!is_id32be(bwav_offset,sf, "BWAV"))
        goto fail;

    iwav_offset = read_u32be(bwav_offset + 0x04, sf) + 0x08 + bwav_offset;

    if (!is_id32be(iwav_offset,sf, "IWAV"))
        goto fail;
    /* past IWAV are some more chunks then padding (variable? some are defined in debug structs only, not seen) */

    total_subsongs = read_u32be(iwav_offset + 0x08,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    offset = iwav_offset + 0x10 + (target_subsong - 1) * 0x20;

    /* IWAV entry supposedly contains more info but seems only offset and some ID at 0x14, rest is 0 */
    ssw2_offset = read_u32be(offset + 0x00,sf) + bwav_offset;
    if (is_id32be(ssw2_offset,sf, "SSWF")) {
        /* 
        04 kType (always 0x01)
        05 nChannels
        06 freq
        08 lpStart
        0C nSamples
        */

        /* data is some unknown codec that seems to be ADPCM header + byte (simplified MTA2 with only 1 group?) */
        vgm_logi("SSPF: unsupported SSWF variant at %x\n", ssw2_offset);
        goto fail;
    }
    else if (is_id32be(ssw2_offset,sf, "SSW2")) {
        stream_size = read_u32be(ssw2_offset + 0x04,sf);
        /* 08 version? (always 0) */
        num_samples = read_s32be(ssw2_offset + 0x0c,sf);
        codec = read_u32be(ssw2_offset + 0x10,sf); /* kType (always 0x21) */
        if (read_u32be(ssw2_offset + 0x10,sf) != 0x21)
            goto fail;
        if (read_u8(ssw2_offset + 0x14,sf) != 0x08) /* nBlocks? */
            goto fail;
        if (read_u8(ssw2_offset + 0x15,sf) != 0x01) /* nChannels? */
            goto fail;

        channels = 1;
        sample_rate = read_u16be(ssw2_offset + 0x16,sf);
        loop_start = read_s32be(ssw2_offset + 0x18,sf);
        /* 0x1c: lpStartAddr (0xFFFFFFFF is none) */

        loop_flag = loop_start != 0x7FFFFFFF;
        start_offset = ssw2_offset + 0x20;
    }
    else {
        vgm_logi("SSPF: unknown variant at %x\n", ssw2_offset);
        goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SSPF;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = num_samples;

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    switch (codec) {
        case 0x21:
            vgmstream->coding_type = coding_MTA2;
            vgmstream->codec_config = 1;
            vgmstream->layout_type = layout_none;
            break;
        
        default:
            vgm_logi("SSPF: unknown codec %x\n", codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
