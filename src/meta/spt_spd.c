#include "meta.h"
#include "../coding/coding.h"


/* .SPT+SPD - Nintendo bank (bgm or sfx) [Bloodrayne (GC), 4x4 EVO 2 (GC), Table Tennis (Wii)] */
VGMSTREAM* init_vgmstream_spt_spd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    int channels, loop_flag, sample_rate;
    off_t header_offset, extra_offset, start_offset;
    int32_t loop_start, loop_end, stream_start, stream_end;
    size_t stream_size;
    uint32_t flags;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    total_subsongs = read_s32be(0x00, sf);
    // ignores alt .spt+spd that start with 0xA20C [Spyro: Enter the Dragonfly (GC)]
    if (total_subsongs < 0 || total_subsongs > 0xFFFF)
        return NULL;

    if (!check_extensions(sf, "spt"))
        return NULL;

    sb = open_streamfile_by_ext(sf, "spd");
    if (!sb) return NULL;

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    header_offset = 0x04 + 0x1c * (target_subsong-1);
    extra_offset  = 0x04 + 0x1c * total_subsongs + 0x2e * (target_subsong - 1);

    flags           = read_u32be(header_offset + 0x00, sf);
    sample_rate     = read_s32be(header_offset + 0x04, sf);
    loop_start      = read_s32be(header_offset + 0x08, sf);
    loop_end        = read_s32be(header_offset + 0x0c, sf);
    stream_end      = read_s32be(header_offset + 0x10, sf);
    stream_start    = read_s32be(header_offset + 0x14, sf);
    // 0x18: null

    channels = 1;
    loop_flag = (flags & 1);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SPT_SPD;
    vgmstream->allow_dual_stereo = 1;
    vgmstream->sample_rate = sample_rate;
    vgmstream->layout_type = layout_none;

    vgmstream->num_streams = total_subsongs;

    switch(flags & (~1)) { /* bitflags */
        case 0: /* common */
            /* values in file nibbles? */
            start_offset = (stream_start / 2 - 1);
            stream_size = (stream_end / 2 + 1) - (stream_start / 2 - 1);
            if (loop_flag) {
                loop_start = (loop_start / 2 - 1) - start_offset;
                loop_end = (loop_end / 2 + 1) - start_offset;
            }

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->num_samples = dsp_bytes_to_samples(stream_size, channels);
            if (loop_flag) {
                vgmstream->loop_start_sample = dsp_bytes_to_samples(loop_start, channels);
                vgmstream->loop_end_sample = dsp_bytes_to_samples(loop_end, channels);
            }

            dsp_read_coefs_be(vgmstream, sf, extra_offset + 0x00, 0x00);
            dsp_read_hist_be (vgmstream, sf, extra_offset + 0x24, 0x00);
            break;

        case 2: /* rare [Monster Jam: Maximum Destruction (GC)] */
            /* values in samples? */
            start_offset = (stream_start * 2);
            stream_size = (stream_end * 2) - (stream_start * 2);
            if (loop_flag) {
                loop_start = (loop_start * 2) - start_offset;
                loop_end = (loop_end * 2) - start_offset;
            }

            vgmstream->coding_type = coding_PCM16BE;
            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, channels, 16);
            if (loop_flag) {
                vgmstream->loop_start_sample = loop_start;
                vgmstream->loop_end_sample = loop_end;
            }
            break;

        case 4: /* supposedly PCM8 */
        default:
            goto fail;
    }

    vgmstream->stream_size = stream_size;

    if (!vgmstream_open_stream(vgmstream, sb, start_offset))
        goto fail;
    close_streamfile(sb);
    return vgmstream;

fail:
    close_streamfile(sb);
    close_vgmstream(vgmstream);
    return NULL;
}
