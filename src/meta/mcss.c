#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

/* Guerrilla's MSS - Found in ShellShock Nam '67 (PS2/Xbox), Killzone (PS2) */
VGMSTREAM* init_vgmstream_mcss(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, data_size;
    int loop_flag = 0, channels;

    /* checks */
    if (!is_id32be(0x00,sf, "MCSS"))
        return NULL;
    if (!check_extensions(sf, "mss"))
        return NULL;

    loop_flag = 0;

    // 04: version? (always 0x00000100 LE)
    start_offset        = read_u32le(0x08,sf);
    data_size           = read_u32le(0x0c,sf);
    int sample_rate     = read_s32le(0x10,sf);
    // 14(1): 1/2/3/4 if 2/4/6/8ch
    // 15(1): 0/1?
    channels            = read_u16le(0x16,sf);
    int interleave      = read_u32le(0x18,sf);
    uint32_t chan_size  = read_u32le(0x1C,sf); //without padding
    // 20: "Guerrilla MSS"

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MCSS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    // no other way to know
    if (vgmstream->interleave_block_size == 0x4800) {
        vgmstream->coding_type = coding_XBOX_IMA;

        /* in stereo multichannel this value is distance between 2ch pair, but we need
         * interleave*ch = full block (2ch 0x4800 + 2ch 0x4800 = 4ch, 0x4800+4800 / 4 = 0x2400) */
        vgmstream->interleave_block_size = vgmstream->interleave_block_size / 2;
        if (vgmstream->channels > 2 && vgmstream->channels % 2 != 0)
            goto fail; // only 2ch+..+2ch layout is known

        /* header values are somehow off? */
        data_size = get_streamfile_size(sf) - start_offset;
        vgmstream->num_samples = xbox_ima_bytes_to_samples(data_size, vgmstream->channels);
    }
    else {
        // 0x800 interleave
        vgmstream->coding_type = coding_PSX;
        vgmstream->num_samples = ps_bytes_to_samples(chan_size, 1);
    }

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
