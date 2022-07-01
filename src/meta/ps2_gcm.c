#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* .GCM - from select Namco games released in around the PS2 era [Gunvari Collection + Time Crisis (PS2), NamCollection (PS2)] */
VGMSTREAM* init_vgmstream_ps2_gcm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;

    int channels;
    uint32_t vagp_l_offset, vagp_r_offset, gcm_samples, data_size, gcm_sample_rate;

    /* checks */
    /* .gcm: actual extension. */
    if (!check_extensions(sf, "gcm"))
        goto fail;

    /* check header */
    if (read_u32be(0x00,sf) != 0x4D434700) /* "MCG" */
        goto fail;

    /* GCM format as Namco outlined it is a hacked VAGp stereo format that can hold 6-channel audio at will.
     * so, let's deal with whatever quirks the format might have first. */
    vagp_l_offset = read_u32le(0x04, sf); // "left" VAGp header pointer.
    if (vagp_l_offset != 0x20)
        goto fail;
    vagp_r_offset = read_u32le(0x08, sf); // "right" VAGp header pointer.
    if (vagp_r_offset != 0x50)
        goto fail;
    start_offset = read_u32le(0x0c, sf); // raw "stereo VAG" data pointer.
    if (start_offset != 0x80)
        goto fail;
    gcm_samples = read_u32le(0x10, sf); // total number of samples... in stereo (not multi-channel as we'll see below).

    /* second step is to check actual GCM size and decide channels value from there.
     * ugly hack, i know, but it's needed for Klonoa multi-channel files (KLN3182M.GCM, KLN3191M.GCM). */
    data_size = get_streamfile_size(sf) - start_offset;
    if (data_size == (gcm_samples * 3)) { // 6-channel GCM file.
        channels = 6;
    }
    else if (data_size == gcm_samples) { // stereo GCM file.
        channels = 2;
    }
    else { // there is only one known GCM "multi-channel" setup and it's hacky.
        goto fail;
    }
    /* what follows are two VAGp headers and raw data, check VAGp header values and see if they match */

    /* VAGp version field, always 4 */
    uint32_t vagp_l_ver, vagp_r_ver;
    vagp_l_ver = read_u32be(vagp_l_offset + 4, sf);
    vagp_r_ver = read_u32be(vagp_r_offset + 4, sf);
    if (vagp_l_ver != vagp_r_ver)
        goto fail;
    /* VAGp size field, usually separate per channel and does not cover blank frames at all */
    uint32_t vagp_l_size, vagp_r_size;
    vagp_l_size = read_u32be(vagp_l_offset + 12, sf);
    vagp_r_size = read_u32be(vagp_r_offset + 12, sf);
    if (vagp_l_size != vagp_r_size)
        goto fail;
    /* VAGp sample rate field, usually separate per channel */
    uint32_t vagp_l_sample_rate, vagp_r_sample_rate;
    vagp_l_sample_rate = read_u32be(vagp_l_offset + 16, sf);
    vagp_r_sample_rate = read_u32be(vagp_r_offset + 16, sf);
    if (vagp_l_sample_rate != vagp_r_sample_rate) {
        goto fail;
    }
    else {
        /* either vagp_l_sample_rate or vagp_r_sample_rate is viable for assigning gcm_sample_rate a value from one of the two. */
        gcm_sample_rate = vagp_r_sample_rate;
    }
    
    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,0); // GCM files don't "loop" by themselves.
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->meta_type = meta_PS2_GCM;
    vgmstream->sample_rate = gcm_sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(gcm_samples, 2);
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
