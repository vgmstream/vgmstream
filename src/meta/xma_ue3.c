#include "meta.h"
#include "../coding/coding.h"

/* XMA from Unreal Engine games */
VGMSTREAM* init_vgmstream_xma_ue3(STREAMFILE *sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, chunk_offset;
    int loop_flag, channel_count, sample_rate, is_xma2_old = 0;
    int num_samples, loop_start_sample, loop_end_sample;
    size_t file_size, fmt_size, seek_size, data_size;


    /* checks */
    /* .xma: assumed */
    /* .x360audio: fake produced by UE Viewer */
    if (!check_extensions(sf, "xma,x360audio,"))
        goto fail;

    /* UE3 uses class-like chunks called "SoundNodeWave" to store info and (rarely multi) raw audio data. Other
     * platforms use standard formats (PC=Ogg, PS3=MSF), while X360 has mutant XMA. Extractors transmogrify
     * UE3 XMA into RIFF XMA (discarding seek table and changing endianness) but we'll support actual raw
     * data for completeness. UE4 has .uexp which are very similar so XBone may use the same XMA. */

    file_size = get_streamfile_size(sf);
    fmt_size  = read_u32be(0x00, sf);
    seek_size = read_u32be(0x04, sf);
    data_size = read_u32be(0x08, sf);
    if (0x0c + fmt_size + seek_size + data_size != file_size)
        goto fail;
    chunk_offset = 0x0c;

    /* parse sample data (always BE unlike real XMA) */
    if (fmt_size != 0x34) { /* old XMA2 [The Last Remnant (X360)] */
        is_xma2_old = 1;
        xma2_parse_xma2_chunk(sf, chunk_offset, &channel_count,&sample_rate, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample);
    }
    else { /* new XMA2 [Shadows of the Damned (X360)] */
        channel_count = read_s16be(chunk_offset + 0x02, sf);
        sample_rate   = read_s32be(chunk_offset + 0x04, sf);
        xma2_parse_fmt_chunk_extra(sf, chunk_offset, &loop_flag, &num_samples, &loop_start_sample, &loop_end_sample, 1);
    }

    start_offset = 0x0c + fmt_size + seek_size;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XMA_UE3;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample   = loop_end_sample;

#ifdef VGM_USE_FFMPEG
    {
        uint8_t buf[0x100];
        size_t bytes;

        if (is_xma2_old) {
            bytes = ffmpeg_make_riff_xma2_from_xma2_chunk(buf,0x100, chunk_offset,fmt_size, data_size, sf);
        } else {
            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf,0x100, chunk_offset,fmt_size, data_size, sf, 1);
        }
        vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, start_offset,data_size);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        xma_fix_raw_samples(vgmstream, sf, start_offset, data_size, chunk_offset, 1,1);
    }
#else
    goto fail;
#endif

    /* UE3 seems to set full loops for non-looping tracks (real loops do exist though), try to detect */
    {
        int full_loop, is_small;

        /* *must* be after xma_fix_raw_samples */
        full_loop = vgmstream->loop_start_sample == 0 && vgmstream->loop_end_sample == vgmstream->num_samples;
        is_small = 1; //vgmstream->num_samples < 20 * vgmstream->sample_rate; /* all files */

        if (full_loop && is_small) {
            VGM_LOG("XMA UE3a: disabled unwanted loop\n");
            vgmstream->loop_flag = 0;
        }
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
