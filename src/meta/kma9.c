#include "meta.h"
#include "../coding/coding.h"
#include "kma9_streamfile.h"


/* KMA9 - Koei Tecmo games [Nobunaga no Yabou: Souzou (Vita)] */
VGMSTREAM* init_vgmstream_kma9(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t start_offset;
    size_t stream_size, interleave;
    int loop_flag, channels;
    int total_subsongs = 0, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf, "KMA9"))
        return NULL;
    if (!check_extensions(sf,"km9"))
        return NULL;

    start_offset = read_u32le(0x04,sf);
    channels = read_u16le(0x32,sf);
    loop_flag = (read_s32le(0x28,sf) != 0);

    total_subsongs = read_s32le(0x08,sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    /* 0x0c: unknown */
    interleave = read_u32le(0x10,sf); /* 1 superframe */
    stream_size = read_u32le(0x14,sf); /* per subsong */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32le(0x34,sf);
    vgmstream->num_samples = read_s32le(0x18,sf); /* without skip_samples? */
    vgmstream->loop_start_sample = read_s32le(0x24,sf); /* with skip_samples? */
    vgmstream->loop_end_sample = vgmstream->num_samples; /* 0x28 looks like end samples but isn't, no idea */
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_KMA9;

#ifdef VGM_USE_ATRAC9
    {
        atrac9_config cfg = {0};

        cfg.channels = channels;
        cfg.encoder_delay = read_s32le(0x20,sf);
        cfg.config_data = read_u32be(0x5c,sf);

        vgmstream->codec_data = init_atrac9(&cfg);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_ATRAC9;
        vgmstream->layout_type = layout_none;

        if (loop_flag) { /* seems correct but must double check */
            vgmstream->loop_start_sample -= cfg.encoder_delay;
            //vgmstream->loop_end_sample -= cfg.encoder_delay;
        }

        /* KMA9 interleaves one ATRAC9 frame per subsong */
        temp_sf = setup_kma9_streamfile(sf, start_offset, stream_size, interleave, (target_subsong-1), total_subsongs);
        if (!temp_sf) goto fail;
        start_offset = 0;
    }
#else
    goto fail;
#endif

    if (!vgmstream_open_stream(vgmstream, temp_sf, start_offset))
        goto fail;
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
