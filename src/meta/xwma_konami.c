#include "meta.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "xwma_konami_streamfile.h"


/* XWMA - Konami (Armature?) variation [Metal Gear Solid 2 HD (X360), Metal Gear Solid 3 HD (X360), MGS2 Master Collection (PC)] */
VGMSTREAM* init_vgmstream_xwma_konami(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "XWMA") && !is_id32be(0x00,sf, "AMWX"))
        return NULL;
    if (!check_extensions(sf,"xwma"))
        return NULL;

    bool big_endian = is_id32be(0x00, sf, "XWMA");
    read_u32_t read_u32 = get_read_u32(big_endian);
    read_s32_t read_s32 = get_read_s32(big_endian);

    int codec           = read_s32(0x04,sf);
    int channels        = read_s32(0x08,sf);
    int sample_rate     = read_s32(0x0c,sf);
    uint32_t data_size  = read_u32(0x10,sf); // without padding
    int avg_bps         = read_s32(0x14,sf);
    int block_align     = read_s32(0x18,sf);
    // 0x1c: empty
    uint32_t start_offset = 0x20;

    int loop_flag  = 0;
    int32_t num_samples = 0;

    // PC has a mini-fmt (codec, channels, sample rate, avg-bps, block align) + seek table
    if (read_u32(0x24, sf) == sample_rate) {
        // 0x30: null?
        int seek_entries = read_s32(0x32, sf);
        // 0x36: entries

        uint32_t last_entry = read_u32(0x26 + 0x04 * (seek_entries - 1), sf); //last dpds entry = total bytes
        num_samples = pcm16_bytes_to_samples(last_entry, channels);

        // after entries sometimes there are garbage(?) bytes in the padding
        start_offset += 0x10 + 0x06 + 0x04 * seek_entries;
        start_offset = align_size_to_block(start_offset, 0x10);
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XWMA_KONAMI;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

#ifdef VGM_USE_FFMPEG
    {
        // XWMA blocks are padded to 0x10 (unrelated to KCEJ blocks)
        temp_sf = setup_xwma_konami_streamfile(sf, start_offset, block_align);
        if (!temp_sf) goto fail;

        vgmstream->codec_data = init_ffmpeg_xwma(temp_sf, 0x00, data_size, codec, channels, sample_rate, avg_bps, block_align);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        /* manually find total samples */
        if (vgmstream->num_samples == 0) {
            ms_sample_data msd = {0};

            msd.channels = vgmstream->channels;
            msd.data_offset = 0x00;
            msd.data_size = data_size;


            if (codec == 0x0161)
                wma_get_samples(&msd, temp_sf, block_align, vgmstream->sample_rate,0x001F);
            //else //TODO: not correct
            //    wmapro_get_samples(&msd, temp_streamFile, block_align, vgmstream->sample_rate,0x00E0);

            vgmstream->num_samples = msd.num_samples;
            if (vgmstream->num_samples == 0)
                vgmstream->num_samples = ffmpeg_get_samples(vgmstream->codec_data); // from avg-br
        }
    }
#else
    goto fail;
#endif

    close_streamfile(temp_sf);
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
