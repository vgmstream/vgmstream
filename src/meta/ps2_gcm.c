#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* .GCM - from select Namco games released in around the PS2 era [Gunvari Collection + Time Crisis (PS2), NamCollection (PS2)] */
VGMSTREAM* init_vgmstream_ps2_gcm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;

    uint32_t vagp_l_offset, vagp_r_offset, gcm_samples, data_size, gcm_sample_rate, gcm_channels;

    /* checks */
    /* .gcm: actual extension. */
    if (!check_extensions(sf, "gcm"))
        goto fail;

    /* check header */
    if (read_u32be(0x00,sf) != 0x4D434700) /* "MCG" */
        goto fail;

    /* GCM format as Namco outlined it is a hacked VAGp stereo format that can hold 6-channel audio at will.
     * so, first step is to let's deal with whatever quirks the format might have */
    vagp_l_offset = read_u32le(0x04, sf); /* "left" VAGp header pointer. */
    if (vagp_l_offset != 0x20)
        goto fail;
    vagp_r_offset = read_u32le(0x08, sf); /* "right" VAGp header pointer. */
    if (vagp_r_offset != 0x50)
        goto fail;
    start_offset = read_u32le(0x0c, sf); /* raw "stereo VAG" data pointer. */
    if (start_offset != 0x80)
        goto fail;
    gcm_samples = read_u32le(0x10, sf); /* size of raw "stereo VAG" data, not correct for multi - channel files as we'll see below. */

    /* second step is to check actual GCM size and decide channels value from there.
     * not helping matters is there's nothing in the GCM header that indicates that this file is multi-channel or not.
     * meaning we have to manually support those Klonoa multi-channel files (KLN3182M.GCM, KLN3191M.GCM, etc). */
    data_size = get_streamfile_size(sf) - start_offset;
    if (data_size == (gcm_samples * 3)) {
        /* 6-channel GCM file. */
        gcm_channels = 6;
    }
    else if (data_size == gcm_samples) {
        /* stereo GCM file. */
        gcm_channels = 2;
    }
    else {
        /* there is only one known GCM "multi-channel" setup and it's hacky. */
        goto fail;
    }
    /* what follows are two VAGp headers and raw data, check VAGp header values and see if they match */

    /* VAGp signature field, always present */
    uint32_t vagp_l_sig, vagp_r_sig, vagp_sig;
    vagp_l_sig = read_u32be(vagp_l_offset, sf);
    vagp_r_sig = read_u32be(vagp_r_offset, sf);
    if (vagp_l_sig != vagp_r_sig) {
        /* check two identical values against each other */
        goto fail;
    }
    else {
        /* check for "VAGp" value */
        vagp_sig = vagp_r_sig;
        if (vagp_sig != 0x56414770) goto fail; /* "VAGp" */
    }
    /* VAGp version field, always 4 */
    uint32_t vagp_l_ver, vagp_r_ver, vagp_ver;
    vagp_l_ver = read_u32be(vagp_l_offset + 4, sf);
    vagp_r_ver = read_u32be(vagp_r_offset + 4, sf);
    if (vagp_l_ver != vagp_r_ver) {
        /* check two identical values against each other */
        goto fail;
    }
    else {
        /* check for "version 4" value */
        vagp_ver = vagp_r_ver;
        if (vagp_ver != 4) goto fail;
    }
    /* VAGp size field, usually separate per channel and does not cover blank frames at all */
    uint32_t vagp_l_size, vagp_r_size, vagp_samples;
    vagp_l_size = read_u32be(vagp_l_offset + 12, sf);
    vagp_r_size = read_u32be(vagp_r_offset + 12, sf);
    if (vagp_l_size != vagp_r_size) {
        /* check two identical values against each other */
        goto fail;
    }
    else {
        /* assign one of the two existing "vag size" values to an entirely new "vag size" variable to be used later */
        vagp_samples = vagp_r_size;
    }
    /* VAGp sample rate field, usually separate per channel */
    uint32_t vagp_l_sample_rate, vagp_r_sample_rate;
    vagp_l_sample_rate = read_u32be(vagp_l_offset + 16, sf);
    vagp_r_sample_rate = read_u32be(vagp_r_offset + 16, sf);
    if (vagp_l_sample_rate != vagp_r_sample_rate) {
        /* check two identical values against each other */
        goto fail;
    }
    else {
        /* assign one of the two existing sample rate values to an entirely new sample rate variable to be used later */
        gcm_sample_rate = vagp_r_sample_rate;
    }
    
    /* build the VGMSTREAM by allocating it to total number of channels and a single loop flag. 
     * though loop flag is set to 0. GCM files don't really "loop" by themselves. */
    vgmstream = allocate_vgmstream(gcm_channels,0);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->meta_type = meta_PS2_GCM;
    vgmstream->sample_rate = gcm_sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(vagp_samples, 1);
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = read_u32le(0x14, sf);

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

    /* clean up anything we may have opened */
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
