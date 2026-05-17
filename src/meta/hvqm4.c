#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "hvqm4_streamfile.h"

/* HVQM4 - from Hudson HVQM4 videos [Resident Evil 0 (GC), Tales of Symphonia (GC)]
 * (info from hcs/Nisto's h4m_audio_decode) */
VGMSTREAM* init_vgmstream_hvqm4(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t start_offset;
    int loop_flag, channels;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id64be(0x00,sf, "HVQM4 1."))
        return NULL;
    if (!is_id32be(0x08,sf, "3\0\0\0") &&
        !is_id32be(0x08,sf, "5\0\0\0"))
        return NULL;

    /* checks */
    /* .h4m: common
     * .hvqm: Shrek: Extra Large (GC) */
    if (!check_extensions(sf, "h4m,hvqm"))
        return NULL;

    /* header */
    start_offset = read_u32be(0x10, sf); // header_size
    if (start_offset != 0x44) // known size
        return NULL;
    if (read_u32be(0x14, sf) > get_streamfile_size(sf) - start_offset) // body_size (may be padded in pikmin)
        return NULL;
    if (read_u32be(0x18, sf) == 0) // blocks
        return NULL;
    // 0x1c: video_frames
    if (read_u32be(0x20, sf) == 0) // audio_frames
        return NULL;
    // 0x24: frame interval
    // 0x28: max_video_frame_size
    // 0x2c: max_sp_packets (0)
    uint32_t max_audio_frame_size = read_u32be(0x30, sf);
    if (max_audio_frame_size == 0)
        return NULL;

    /* video info */
    // 0x34: width
    // 0x36: height
    // 0x38: h_sampling_rate
    // 0x39: v_sampling_rate
    // 0x3a: video_mode (0 or 0x12)
    // 0x3b: user_defined (0)

    /* audio info */
    uint8_t audio_format    =    read_u8(0x3c,sf);
    uint8_t sample_bits     =    read_u8(0x3d,sf);
    uint8_t audio_flags     =    read_u8(0x3e,sf);
    uint8_t multi_tracks    =    read_u8(0x3f,sf);
    int sample_rate         = read_s32be(0x40,sf);

    channels = 2;
    loop_flag  = 0;

    /* tracks for languages [Pokemon Channel], or sometimes used to fake multichannel [Tales of Symphonia] */
    total_subsongs = multi_tracks + 1;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = get_streamfile_size(sf) / total_subsongs; // approx...
    vgmstream->meta_type = meta_HVQM4;

    /* unsure about audio_format and audio_flags, pikmin needs audio_codec but
     * h4m_audio_decode handles audio_flags & 0x7F like this:
     * - 00 + sample_bits=16 = ADPCM
     * - 00 + sample_bits=00 = AFC
     * - 01: uncompressed PCM
     * - 04: 8-bit (A)DPCM 
     */
    switch(audio_format) {
        case 0x02:
            switch(sample_bits) {
                case 16: 
                    vgmstream->coding_type = coding_HVQM4_IMA; 
                    vgmstream->layout_type = layout_blocked_hvqm4;
                    vgmstream->codec_config = audio_flags; // for blocks
                    break; // common
                default:
                    goto fail;
            }
            break;

        case 0x04: // Pikmin (GC)
            vgmstream->coding_type = coding_AFC;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x09;

            // somehow max_audio_frame_size is the data size for AFC
            vgmstream->num_samples = afc_bytes_to_samples(max_audio_frame_size, channels);

            // needs streamfile since frames are cut in between blocks
            temp_sf = setup_hvqm4_streamfile(sf, start_offset, target_subsong, total_subsongs);
            if (!temp_sf) goto fail;
            break;

        case 0x05: // Pikmin (GC)
            vgmstream->coding_type = coding_AFC_4X;
            vgmstream->layout_type = layout_none;

            // somehow max_audio_frame_size is the data size for AFC
            vgmstream->num_samples = afc_4x_bytes_to_samples(max_audio_frame_size, channels);

            // needs streamfile since frames are cut in between blocks
            temp_sf = setup_hvqm4_streamfile(sf, start_offset, target_subsong, total_subsongs);
            if (!temp_sf) goto fail;
            break;

        default:
            VGM_LOG("HVQM4: unknown codec=%x, flags=%x\n", audio_format, audio_flags);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, temp_sf ? temp_sf : sf, temp_sf ? 0x00 : start_offset))
        goto fail;

    /* calc num_samples manually */
    if (vgmstream->layout_type == layout_blocked_hvqm4) {
        vgmstream->stream_index = target_subsong; /* extra setup */
        vgmstream->full_block_size = 0; /* extra setup */
        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset,vgmstream);
            vgmstream->num_samples += vgmstream->current_block_samples;
        }
        while (vgmstream->next_block_offset < get_streamfile_size(sf));
        vgmstream->full_block_size = 0; /* extra cleanup */
        block_update(start_offset, vgmstream);
    }

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    close_streamfile(temp_sf);
    return NULL;
}
