#include "meta.h"
#include "../coding/coding.h"

/* .WAVE - WayForward "EngineBlack" games [Mighty Switch Force! (3DS), Adventure Time: Hey Ice King! Why'd You Steal Our Garbage?! (3DS)] */
VGMSTREAM * init_vgmstream_wave(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, extradata_offset;
    int loop_flag = 0, channel_count, sample_rate, codec;
    int32_t num_samples, loop_start = 0, loop_end = 0;
    size_t interleave;
    int big_endian;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    float (*read_f32)(off_t,STREAMFILE*) = NULL;

    /* checks */
    if (!check_extensions(streamFile, "wave"))
        goto fail;

    if (read_32bitLE(0x00,streamFile) != 0xE5B7ECFE &&  /* header id */
        read_32bitBE(0x00,streamFile) != 0xE5B7ECFE)
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x00) /* version? */
        goto fail;

    /* assumed */
    big_endian = read_32bitBE(0x00,streamFile) == 0xE5B7ECFE;
    if (big_endian) {
        read_32bit = read_32bitBE;
        read_f32 = read_f32be;
    } else {
        read_32bit = read_32bitLE;
        read_f32 = read_f32le;
    }

    channel_count = read_8bit(0x05,streamFile);

    if (read_32bit(0x08,streamFile) != get_streamfile_size(streamFile))
        goto fail;
    if (read_8bit(0x0c,streamFile) != 0x00) /* ? */
        goto fail;

    sample_rate = (int)read_f32(0x0c, streamFile); /* sample rate in 32b float (WHY?) */
    num_samples = read_32bit(0x10, streamFile);
    loop_start  = read_32bit(0x14, streamFile);
    loop_end    = read_32bit(0x18, streamFile);

    codec         = read_8bit(0x1c, streamFile);
    channel_count = read_8bit(0x1d, streamFile);
    if (read_8bit(0x1e, streamFile) != 0x00) goto fail; /* unknown */
    if (read_8bit(0x1f, streamFile) != 0x00) goto fail; /* unknown */

    start_offset = read_32bit(0x20, streamFile);
    interleave = read_32bit(0x24, streamFile); /* typically half data_size */
    extradata_offset = read_32bit(0x28, streamFile); /* OR: extradata size (0x2c) */

    loop_flag = (loop_start > 0);
    /* some songs (ex. Adventure Time's m_candykingdom_overworld.wave) do full loops, but there is no way
     * to tell them apart from sfx/voices, so we try to detect if it's long enough. */
    if(!loop_flag
            && loop_start == 0 && loop_end == num_samples /* full loop */
            && channel_count > 1
            && num_samples > 20*sample_rate) { /* in seconds */
        loop_flag = 1;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->meta_type = meta_WAVE;
    /* not sure if there are other codecs but anyway */
    switch(codec) {
        case 0x02:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            /* ADPCM setup: 0x20 coefs + 0x06 initial ps/hist1/hist2 + 0x06 loop ps/hist1/hist2, per channel */
            dsp_read_coefs(vgmstream, streamFile, extradata_offset+0x00, 0x2c, big_endian);
            dsp_read_hist(vgmstream, streamFile, extradata_offset+0x22, 0x2c, big_endian);
            break;
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
