#include "meta.h"
#include "../coding/coding.h"
#include "vsv_streamfile.h"


/* .VSV - from Square Enix games [Dawn of Mana: Seiken Densetsu 4 (PS2), Kingdom Hearts Re:Chain of Memories (PS2)] */
VGMSTREAM * init_vgmstream_vsv(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t start_offset;
    int loop_flag, channel_count, flags, sample_rate, is_rs;
    size_t loop_start, adjust, data_size, interleave;


    /* checks */
    /* .vsv: extension from internal filenames [KH Re:CoM (PS2), DoM (PS2), KH HD I.5 + II.5 ReMIX (PS4)]
     * .psh: fake */
    if (!check_extensions(streamFile, "vsv,psh"))
        goto fail;

    /* 0x00(1x4): flags/config? */
    if ((uint8_t)read_8bit(0x03,streamFile) > 0x64) /* possibly volume */
        goto fail;
    if ((uint8_t)read_8bit(0x0a,streamFile) != 0) /* not seen */
        goto fail;

    /* Romancing SaGa (PS2) uses an earlier? version, this seems to work */
    is_rs = ((uint16_t)read_16bitLE(0x00,streamFile) == 0);

    start_offset = 0x00; /* correct, but needs some tricks to fix sound (see below) */
    interleave = 0x800;

    adjust      = (uint16_t)read_16bitLE(0x04,streamFile);
    loop_start = ((uint16_t)read_16bitLE(0x06,streamFile) & 0x7FFF) * interleave;
    loop_flag   = (uint16_t)read_16bitLE(0x06,streamFile) & 0x8000; /* loop_start != 0 works too, no files loop from beginning to end */
    sample_rate = (uint16_t)read_16bitLE(0x08,streamFile);
    flags       =  (uint8_t)read_8bit   (0x0b,streamFile); /* values: 0x01=stereo, 0x10=mono */
    data_size   = (uint16_t)read_16bitLE(0x0c,streamFile) * interleave;
    /* 0x0e: ? (may be a low-ish value) */

    channel_count = (flags & 1) ? 2 : 1;

    /* must discard to avoid wrong loops and unwanted data (easier to see in voices) */
    if (!is_rs) { /* RS doesn't do this */
        /* adjust & 0xF800 is unknown (values=0x0000|0x0800|0xF800, can be mono/stereo, loop/no, adjust/no) */
        size_t discard = adjust & 0x07FF;
        /* at (file_end - 0x800 + discard) is a 0x03 PS flag to check this (adjust 0 does discard full block) */
        data_size -= (0x800 - discard) * channel_count;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VSV;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(data_size, channel_count);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channel_count);
    vgmstream->loop_end_sample = vgmstream->num_samples;

    /* these loops are odd, but comparing the audio wave with the OSTs values seem correct */
    if (is_rs) {
        vgmstream->loop_start_sample -= ps_bytes_to_samples(channel_count*interleave,channel_count); /* maybe *before* loop block? */
        vgmstream->loop_start_sample -= ps_bytes_to_samples(0x200*channel_count,channel_count); /* maybe default adjust? */
    }

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    temp_streamFile = setup_vsv_streamfile(streamFile);
    if (!temp_streamFile) goto fail;

    if (!vgmstream_open_stream(vgmstream, temp_streamFile, start_offset))
        goto fail;

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
