#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"
#include "mul_streamfile.h"

typedef enum { PSX, DSP, IMA, XMA1, FSB4 } mul_codec;

static off_t get_start_offset(STREAMFILE* sf);
static int guess_codec(STREAMFILE* sf, off_t start_offset, int big_endian, int channels, mul_codec* p_codec, off_t* p_extra_offset);
static layered_layout_data* build_layered_mul(STREAMFILE* sf, off_t offset, int big_endian, VGMSTREAM* vgmstream, mul_codec codec);

/* .MUL - from Crystal Dynamics games [Legacy of Kain: Defiance (PS2), Tomb Raider Underworld (multi)] */
VGMSTREAM* init_vgmstream_mul(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, coefs_offset = 0;
    int loop_flag, channel_count, sample_rate, num_samples, loop_start;
    int big_endian;
    mul_codec codec;
    read_u32_t read_u32;
    read_f32_t read_f32;


    /* checks */
    /* .mul: found in the exe/reversed names, used by the bigfile extractor (Gibbed.TombRaider)
     *       (some files have companion .mus/sam files but seem to be sequences/control stuff)
     * .emff: fake extension ('Eidos Music File Format') */
    if (!check_extensions(sf, "mul,emff"))
        goto fail;
    if (read_u32be(0x10,sf) != 0 ||
        read_u32be(0x14,sf) != 0 ||
        read_u32be(0x18,sf) != 0 ||
        read_u32be(0x1c,sf) != 0)
        goto fail;

    big_endian = guess_endianness32bit(0x00, sf);
    read_u32 = big_endian ? read_u32be : read_u32le;
    read_f32 = big_endian ? read_f32be : read_f32le;

    sample_rate   = read_u32(0x00,sf);
    loop_start    = read_u32(0x04,sf);
    num_samples   = read_u32(0x08,sf);
    channel_count = read_u32(0x0C,sf);
    if (sample_rate < 8000 || sample_rate > 48000 || channel_count > 8)
        goto fail;
    /* 0x20: flag when file has non-audio blocks (ignored by the layout) */
    /* 0x24: same? */
    /* 0x28: loop offset within audio data (not file offset) */
    /* 0x2c: some value related to loop? */
    /* 0x34: id? */
    /* 0x38+: channel config until ~0x100? (multiple 1.0f depending on the number of channels) */

    /* extra tests just in case (1.0=common, varies but goes around ~2800.0) */
    {
        float check1 = read_f32(0x38,sf);
        float check2 = read_f32(0x3c,sf);
        if (!(check1 >= 1.0 && check1 <= 3000.0) && check2 != 1.0)
            goto fail;
    }

    loop_flag = (loop_start >= 0); /* 0xFFFFFFFF when not looping */

    start_offset = get_start_offset(sf);
    if (!start_offset) goto fail;

    /* format is pretty limited so we need to guess codec */
    if (!guess_codec(sf, start_offset, big_endian, channel_count, &codec, &coefs_offset))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MUL;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = num_samples;
    vgmstream->codec_endian = big_endian;

    switch(codec) {
        case PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_blocked_mul;
            break;

        case DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_blocked_mul;

            dsp_read_coefs_be(vgmstream,sf,coefs_offset+0x00,0x2e);
            dsp_read_hist_be (vgmstream,sf,coefs_offset+0x24,0x2e);
            break;

        case IMA:
            vgmstream->coding_type = coding_CD_IMA;
            vgmstream->layout_type = layout_blocked_mul;
            break;

        case XMA1:
        case FSB4:
            vgmstream->layout_data = build_layered_mul(sf, start_offset, big_endian, vgmstream, codec);
            if (!vgmstream->layout_data) goto fail;
            //vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_layered;
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

/* find first block with header info (probably hardcoded) */
static off_t get_start_offset(STREAMFILE* sf) {
    uint32_t test1, test2;

    /* earlier games */
    test1 = read_u32be(0x0800,sf);
    test2 = read_u32be(0x0804,sf);
    if ((test1 != 0 && test1 != 0xFFFFFFFF) || (test2 != 0 && test2 != 0xFFFFFFFF))
        return 0x800;

    /* later games */
    test1 = read_u32be(0x2000,sf);
    test2 = read_u32be(0x2004,sf);
    if ((test1 != 0 && test1 != 0xFFFFFFFF) || (test2 != 0 && test2 != 0xFFFFFFFF))
        return 0x2000;

    return 0;
}

static int guess_codec(STREAMFILE* sf, off_t start_offset, int big_endian, int channels, mul_codec* p_codec, off_t* p_extra_offset) {
    uint32_t (*read_u32)(off_t,STREAMFILE*);

    read_u32 = big_endian ? read_u32be : read_u32le;
    *p_extra_offset = 0;

    if (big_endian) {
        /* test DSP (GC/Wii): check known coef locations */
        if (read_u32be(0xC8,sf) != 0) { /*  Tomb Raider Legend (GC) */
            *p_codec = DSP;
            *p_extra_offset = 0xC8;
            return 1;
        }
        else if (read_u32be(0xCC,sf) != 0) { /* Tomb Raider Anniversary (Wii) */
            *p_codec = DSP;
            *p_extra_offset = 0xCC;
            return 1;
        }
        else if (read_u32be(0x2D0,sf) != 0) { /* Tomb Raider Underworld (Wii) */
            *p_codec = DSP;
            *p_extra_offset = 0x2D0;
            return 1;
        }
        else { //if (ps_check_format(sf, 0x820, 0x100)) {
            /* may be PS3/X360, tested below [Tomb Raider 7 (PS3)] */
        }
    }

    {
        int i;
        off_t offset = start_offset;
        size_t file_size = get_streamfile_size(sf);
        size_t frame_size;

        /* check first audio frame */
        while (offset < file_size) {
            uint32_t block_type = read_u32(offset+0x00, sf);
            uint32_t block_size = read_u32(offset+0x04, sf);
            uint32_t data_size  = read_u32(offset+0x10, sf);

            if (block_type == 0xFFFFFFFF || block_size == 0xFFFFFFFF || data_size == 0xFFFFFFFF)
                return 0;

            if (block_type != 0x00) {
                offset += 0x10 + block_size;
                continue; /* not audio */
            }

            /* test FSB4 header */
            if (is_id32be(offset + 0x10, sf, "FSB4") || is_id32be(offset + 0x20, sf, "FSB4")) {
                *p_codec = FSB4;
                return 1;
            }

            /* test XMA1 (X360): has sub-blocks of 0x800 per channel */
            if (block_size == 0x810 * channels) {
                for (i = 0; i < channels; i++) {
                    off_t test_offset = offset + 0x10 + 0x810*i;
                    if (read_u32(test_offset + 0x00, sf) != 0x800 || /* XMA packet size */
                        read_u32(test_offset + 0x10, sf) != 0x08000000) /* XMA packet first header */
                        break;
                }
                if (i == channels) {
                    *p_codec = XMA1;
                    return 1;
                }
            }

            /* test PS-ADPCM (PS2/PSP): flag is always 2 in .mul */
            frame_size = 0x10;
            for (i = 0; i < data_size / frame_size; i++) {
                if (read_8bit(offset + 0x20 + frame_size*i + 0x01, sf) != 0x02)
                    break;
            }
            if (i == data_size / frame_size) {
                *p_codec = PSX;
                return 1;
            }

            /* test XBOX-IMA (PC/Xbox): reserved frame header value is always 0 */
            frame_size = 0x24;
            for (i = 0; i < data_size / frame_size; i++) {
                if (read_8bit(offset + 0x20 + frame_size*i + 0x03, sf) != 0x00)
                    break;
            }
            if (i == data_size / frame_size) {
                *p_codec = IMA;
                return 1;
            }

            break;
        }
    }

    return 0;
}


/* MUL contain one XMA1/FSB streams per channel so we need the usual voodoo */
static layered_layout_data* build_layered_mul(STREAMFILE* sf, off_t offset, int big_endian, VGMSTREAM* vgmstream, mul_codec codec) {
    layered_layout_data* data = NULL;
    STREAMFILE* temp_sf = NULL;
    int i, layers = vgmstream->channels;


    /* init layout */
    data = init_layout_layered(layers);
    if (!data) goto fail;

    for (i = 0; i < layers; i++) {

        switch(codec) {
#ifdef VGM_USE_FFMPEG
            case XMA1: {
                size_t stream_size;
                int layer_channels = 1;

                temp_sf = setup_mul_streamfile(sf, offset, big_endian, i, layers, NULL);
                if (!temp_sf) goto fail;

                stream_size = get_streamfile_size(temp_sf);

                /* build the layer VGMSTREAM */
                data->layers[i] = allocate_vgmstream(layer_channels, 0);
                if (!data->layers[i]) goto fail;

                data->layers[i]->sample_rate = vgmstream->sample_rate;
                data->layers[i]->num_samples = vgmstream->num_samples;

                data->layers[i]->codec_data = init_ffmpeg_xma1_raw(temp_sf, 0x00, stream_size, data->layers[i]->channels, data->layers[i]->sample_rate, 0);
                if (!data->layers[i]->codec_data) goto fail;
                data->layers[i]->coding_type = coding_FFmpeg;
                data->layers[i]->layout_type = layout_none;

                data->layers[i]->stream_size = stream_size;

                xma_fix_raw_samples(data->layers[i], temp_sf, 0x00,stream_size, 0, 0,0); /* ? */
                break;
            }
#endif
            case FSB4: { /* FSB4 w/ mono MP3 [Tomb Raider 8 (PS3), Avengers (PC)] */
                temp_sf = setup_mul_streamfile(sf, offset, big_endian, i, layers, "fsb");
                if (!temp_sf) goto fail;

                data->layers[i] = init_vgmstream_fsb(temp_sf);
                if (!data->layers[i]) goto fail;
                break;
            }

            default:
                goto fail;
        }

        close_streamfile(temp_sf);
        temp_sf = NULL;
    }

    if (!setup_layout_layered(data))
        goto fail;

    vgmstream->coding_type = data->layers[0]->coding_type;
    return data;

fail:
    close_streamfile(temp_sf);
    free_layout_layered(data);
    return NULL;
}
