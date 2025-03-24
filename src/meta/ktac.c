#include "meta.h"
#include "../coding/coding.h"

typedef struct {
    int loop_flag;
    int32_t loop_start;
    int32_t loop_end;
    uint16_t loop_start_adjust;
    uint16_t loop_end_padding;
    uint32_t file_size;
#ifdef VGM_USE_FFMPEG
    mp4_custom_t mp4;
#endif
    int type;
} ktac_header_t;


/* KTAC - Koei Tecmo custom AAC [Kin'iro no Corda 3 (Vita), Shingeki no Kyojin: Shichi kara no Dasshutsu (3DS), Dynasty Warriors (PS4)] */
VGMSTREAM* init_vgmstream_ktac(STREAMFILE* sf) {
#ifdef VGM_USE_FFMPEG
    VGMSTREAM* vgmstream = NULL;
    ktac_header_t ktac = {0};

    /* checks */
    if (!is_id32be(0x00,sf, "KTAC"))
        return NULL;

    // .ktac: header id (probable extension from debug strings is "kac"
    if (!check_extensions(sf,"ktac"))
        return NULL;

    // 0x04: version? (0x01000000=common, 0x01010000=WP9 2022)
    ktac.file_size = read_u32le(0x08,sf);
    if (ktac.file_size != get_streamfile_size(sf))
        return NULL;
    ktac.mp4.stream_offset  = read_u32le(0x0c,sf);
    ktac.mp4.stream_size    = read_u32le(0x10,sf);
    ktac.type               = read_u32le(0x14,sf); // 0=AoT, KnC3 bgm, type 1=KnC3 1ch voices, type 2=DW4, Atelier Ryza, others
    ktac.mp4.sample_rate    = read_u32le(0x18,sf);
    ktac.mp4.num_samples    = read_u32le(0x1c,sf); // full samples (total_frames * frame_size)
    ktac.mp4.channels       = read_u16le(0x20,sf);
    ktac.mp4.frame_samples  = read_u16le(0x22,sf);
    ktac.mp4.encoder_delay  = read_u16le(0x24,sf);
    ktac.mp4.end_padding    = read_u16le(0x26,sf);
    ktac.loop_start         = read_u32le(0x28,sf);
    ktac.loop_end           = read_u32le(0x2c,sf);
    ktac.loop_start_adjust  = read_u16le(0x30,sf);
    ktac.loop_end_padding   = read_u16le(0x32,sf); // usually same as end_padding
    // 0x34: reserved? (always null)
    ktac.mp4.table_offset   = read_u32le(0x38,sf);
    ktac.mp4.table_entries  = read_u32le(0x3c,sf); // total_frames

    ktac.loop_flag = (ktac.loop_end > 0);

    // loop handling, correct vs full loops too [Winning Post 9 2022 (PC)]
    //  - loop_start == 1 = 2048 + adjust 64 == 2112 == encoder delay
    //  - (loop_end + 1) == total_frames, loop_end_adjust = 96 = end_padding
    ktac.loop_start = ktac.loop_start * ktac.mp4.frame_samples + ktac.loop_start_adjust;
    ktac.loop_end = (ktac.loop_end + 1) * ktac.mp4.frame_samples - ktac.loop_end_padding;

    int channels = ktac.mp4.channels;
    int sample_rate = ktac.mp4.sample_rate;
    int num_samples = ktac.mp4.num_samples;

    // type 1 has some odd behavior. FFmpeg returns 2 channels with dupe samples (must decode x2),
    // Possibly fake mp4's add_esds config is off, but internal sample_rate/etc seems correct (matters for decoding).
    // It's not impossible it's just some KT decoder trickery, so for now force double values.
    if (ktac.type == 1) {
        vgm_logi("KTAC: type %i found\n", ktac.type);
        if (channels != 1)
            goto fail;
        channels *= 2; //could use 1 channel + let copy-samples ignore extra channel?
        sample_rate *= 2;
        num_samples *= 2;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, ktac.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_KTAC;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples - ktac.mp4.end_padding - ktac.mp4.encoder_delay;
    vgmstream->loop_start_sample = ktac.loop_start - ktac.mp4.encoder_delay;
    vgmstream->loop_end_sample = ktac.loop_end - ktac.mp4.encoder_delay;

    // KTAC uses AAC, but not type found in .aac (that has headered frames, like mp3) but raw
    // packets + frame size table (similar to .mp4/m4a). We set config for FFmpeg's fake M4A header
    vgmstream->codec_data = init_ffmpeg_mp4_custom_ktac(sf, &ktac.mp4);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
#endif
    return NULL;
}
