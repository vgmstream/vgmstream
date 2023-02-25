#include "meta.h"
#include "../util/endianness.h"
#include "../coding/coding.h"

/* .WAVE - WayForward "EngineBlack" games [Mighty Switch Force! (3DS), Adventure Time: Hey Ice King! Why'd You Steal Our Garbage?! (3DS)] */
VGMSTREAM* init_vgmstream_wave(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset, extradata_offset, interleave;
    int channels, loop_flag, sample_rate, codec;
    int32_t num_samples, loop_start, loop_end;
    int big_endian;
    read_u32_t read_u32;
    read_s32_t read_s32;
    read_f32_t read_f32;


    /* checks */
    if (!is_id32be(0x00,sf, "VAW3") && /* Happy Feet Two (3DS) */
        read_u32le(0x00,sf) != 0xE5B7ECFE &&  /* common LE (hashed something?)  */
        read_u32be(0x00,sf) != 0xE5B7ECFE &&
        read_u32be(0x00,sf) != 0xC9FB0C03)  /* NDS [Lalaloopsy, Adventure Time: HIKWYSOG (DS)] */
        goto fail;
    /* 0x04: version? common=0, VAW3=2 */

    if (!check_extensions(sf, "wave"))
        goto fail;

    /* assumed */
    big_endian = read_u32be(0x00,sf) == 0xE5B7ECFE;
    if (big_endian) {
        read_u32 = read_u32be;
        read_s32 = read_s32be;
        read_f32 = read_f32be;
    } else {
        read_u32 = read_u32le;
        read_s32 = read_s32le;
        read_f32 = read_f32le;
    }

    if (read_u32(0x08,sf) != get_streamfile_size(sf))
        goto fail;

    sample_rate = (int)read_f32(0x0c, sf); /* WHY */
    num_samples = read_s32(0x10, sf);
    loop_start  = read_s32(0x14, sf);
    loop_end    = read_s32(0x18, sf);

    codec       =  read_u8(0x1c, sf);
    channels    =  read_u8(0x1d, sf);
    if (read_u8(0x1e, sf) != 0x00) goto fail; /* unknown */
    if (read_u8(0x1f, sf) != 0x00) goto fail; /* unknown */

    start_offset = read_u32(0x20, sf);
    interleave   = read_u32(0x24, sf); /* typically half data_size */
    extradata_offset = read_u32(0x28, sf); /* always 0x2c */

    loop_flag = (loop_start > 0);
    /* some songs (ex. Adventure Time's m_candykingdom_overworld.wave) do full loops, but there is no way
     * to tell them apart from sfx/voices, so we try to detect if it's long enough. */
    if(!loop_flag
            && loop_start == 0 && loop_end == num_samples /* full loop */
            && (channels > 1 || (channels == 1 && start_offset <= 0x40))
            && num_samples > 30*sample_rate) { /* in seconds */
        loop_flag = 1;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->meta_type = meta_WAVE;

    /* not sure if there are other codecs but anyway (based also see wave-segmented) */
    switch(codec) {
        case 0x02:
            /* DS games use IMA, no apparent flag (could also test ID) */
            if (start_offset <= 0x40) {
                vgmstream->coding_type = coding_IMA_int;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = interleave;

                /* extradata: 
                 * 0x00: base hist? (only seen 0)
                 * 0x02: base step? (only seen 0)
                 * 0x04: loop hist?
                 * 0x06: loop step?
                 */
            }
            else {
                vgmstream->coding_type = coding_NGC_DSP;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = interleave;

                /* ADPCM setup: 0x20 coefs + 0x06 initial ps/hist1/hist2 + 0x06 loop ps/hist1/hist2, per channel */
                dsp_read_coefs(vgmstream, sf, extradata_offset+0x00, 0x2c, big_endian);
                dsp_read_hist(vgmstream, sf, extradata_offset+0x22, 0x2c, big_endian);
            }
            break;
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
