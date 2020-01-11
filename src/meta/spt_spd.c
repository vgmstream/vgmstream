#include "meta.h"
#include "../coding/coding.h"


/* SPD+SPT - Nintendo bank (bgm or sfx) format [Bloodrayne (GC), 4x4 EVO 2 (GC), Table Tennis (Wii)] */
VGMSTREAM * init_vgmstream_spt_spd(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *sf_h = NULL;
    int channel_count, loop_flag, sample_rate;
    off_t header_offset, extra_offset, start_offset;
    int32_t loop_start, loop_end, stream_start, stream_end;
    size_t stream_size;
    uint32_t flags;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    if (!check_extensions(streamFile, "spd"))
        goto fail;
    sf_h = open_streamfile_by_ext(streamFile, "spt");
    if (!sf_h) goto fail;

    /* ignore alt .spt+spd [Spyro: Enter the Dragonfly (GC)] */
    if (read_u16be(0x00, sf_h) != 0) /* always 0xA20C? */
        goto fail;

    total_subsongs = read_s32be(0x00, sf_h);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    header_offset = 0x04 + 0x1c * (target_subsong-1);
    extra_offset  = 0x04 + 0x1c * total_subsongs + 0x2e * (target_subsong-1);

    flags           = read_u32be(header_offset + 0x00, sf_h);
    sample_rate     = read_s32be(header_offset + 0x04, sf_h);
    loop_start      = read_s32be(header_offset + 0x08, sf_h);
    loop_end        = read_s32be(header_offset + 0x0c, sf_h);
    stream_end      = read_s32be(header_offset + 0x10, sf_h);
    stream_start    = read_s32be(header_offset + 0x14, sf_h);
    /* 0x18: null */

    channel_count = 1;
    loop_flag = (flags & 1);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
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
            vgmstream->num_samples = dsp_bytes_to_samples(stream_size, channel_count);
            if (loop_flag) {
                vgmstream->loop_start_sample = dsp_bytes_to_samples(loop_start, channel_count);
                vgmstream->loop_end_sample = dsp_bytes_to_samples(loop_end, channel_count);
            }
            dsp_read_coefs_be(vgmstream, sf_h, extra_offset + 0x00, 0x00);
            dsp_read_hist_be (vgmstream, sf_h, extra_offset + 0x24, 0x00);
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
            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, channel_count, 16);
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

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    close_streamfile(sf_h);
    return vgmstream;

fail:
    close_streamfile(sf_h);
    close_vgmstream(vgmstream);
    return NULL;
}
