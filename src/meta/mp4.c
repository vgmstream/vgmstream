#include "meta.h"
#include "../coding/coding.h"

#ifdef VGM_USE_FFMPEG
typedef struct {
    int channels;
    int sample_rate;
    int32_t num_samples;
    int loop_flag;
    int32_t loop_start;
    int32_t loop_end;
    int32_t encoder_delay;
    int subsongs;
} mp4_header;

static void parse_mp4(STREAMFILE* sf, mp4_header* mp4);


VGMSTREAM* init_vgmstream_mp4_aac_ffmpeg(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset = 0;
    mp4_header mp4 = {0};
    size_t file_size;
    ffmpeg_codec_data* ffmpeg_data = NULL;


    /* checks */
    if ((read_u32be(0x00,sf) & 0xFFFFFF00) != 0) /* first atom BE size (usually ~0x18) */
        goto fail;
    if (!is_id32be(0x04,sf, "ftyp"))
        goto fail;

    /* .bin: Final Fantasy Dimensions (iOS), Final Fantasy V (iOS)
     * .msd: UNO (iOS) */
    if (!check_extensions(sf,"mp4,m4a,m4v,lmp4,bin,lbin,msd"))
        goto fail;

    file_size = get_streamfile_size(sf);

    ffmpeg_data = init_ffmpeg_offset(sf, start_offset, file_size);
    if (!ffmpeg_data) goto fail;

    parse_mp4(sf, &mp4);

    /* most values aren't read directly and use FFmpeg b/c MP4 makes things hard */
    if (!mp4.num_samples)
        mp4.num_samples = ffmpeg_get_samples(ffmpeg_data);  /* does this take into account encoder delay? see FFV */
    if (!mp4.channels)
        mp4.channels = ffmpeg_get_channels(ffmpeg_data);
    if (!mp4.sample_rate)
        mp4.sample_rate = ffmpeg_get_sample_rate(ffmpeg_data);
    if (!mp4.subsongs)
        mp4.subsongs = ffmpeg_get_subsong_count(ffmpeg_data); /* may contain N tracks */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(mp4.channels, mp4.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_MP4;
    vgmstream->sample_rate = mp4.sample_rate;
    vgmstream->num_samples = mp4.num_samples;
    vgmstream->loop_start_sample = mp4.loop_start;
    vgmstream->loop_end_sample = mp4.loop_end;

    vgmstream->codec_data = ffmpeg_data;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;
    vgmstream->num_streams = mp4.subsongs;

    vgmstream->channel_layout = ffmpeg_get_channel_layout(ffmpeg_data);

    /* needed for CRI MP4, otherwise FFmpeg usually reads standard delay */
    ffmpeg_set_skip_samples(vgmstream->codec_data, mp4.encoder_delay);

    return vgmstream;

fail:
    free_ffmpeg(ffmpeg_data);
    if (vgmstream) {
        vgmstream->codec_data = NULL;
        close_vgmstream(vgmstream);
    }
    return NULL;
}

/* read useful MP4 chunks */
static void parse_mp4(STREAMFILE* sf, mp4_header* mp4) {
    uint32_t offset, suboffset, max_offset, max_suboffset;


    /* MOV format chunks, called "atoms", size goes first because Apple */
    offset = 0x00;
    max_offset = get_streamfile_size(sf);
    while (offset < max_offset) {
        uint32_t size = read_u32be(offset + 0x00,sf);
        uint32_t type = read_u32be(offset + 0x04,sf);
        //offset += 0x08;

        /* just in case */
        if (size == 0)
            break;

        switch(type) {
            case 0x66726565: /* "free" */
                /* Tales of Hearts R (iOS) has loop info in the first "free" atom */
                if (read_u32be(offset + 0x08,sf) == 0x4F700002 && (size == 0x38 || size == 0x40)) {
                    /* 0x00: id / "Op" */
                    /* 0x02: channels */
                    /* 0x04/8: sample rate */
                    /* 0x0c: null? */
                    /* 0x10: num_samples (without padding, same as FFmpeg's) */
                    /* 0x14/18/1c/20: offsets to stream info (stts/stsc/stsz/stco) */
                    mp4->encoder_delay = read_u32be(offset + 0x08 + 0x24,sf); /* Apple's 2112 */
                    mp4->loop_flag = read_u32be(offset + 0x08 + 0x28,sf);
                    if (mp4->loop_flag) { /* atom ends if no loop flag */
                        mp4->loop_start = read_u32be(offset + 0x08 + 0x2c,sf);
                        mp4->loop_end = read_u32be(offset + 0x08 + 0x30,sf);
                    }
                    max_offset = 0;
                }
                /* M2 emu's .m4a (codename zoom) */
                else if (is_id32be(offset + 0x08 + 0x00,sf, "ZOOM")) {
                    /* 0x00: id */
                    mp4->encoder_delay  = read_s32be(offset + 0x08 + 0x04,sf); /* Apple's 2112, also in iTunes tag */
                    /* 0x08: end padding */
                    mp4->num_samples    = read_s32be(offset + 0x08 + 0x0c,sf);
                    mp4->loop_start     = read_s32be(offset + 0x08 + 0x10,sf);
                    mp4->loop_end       = read_s32be(offset + 0x08 + 0x14,sf);
                    mp4->loop_flag = (mp4->loop_end != 0);
                    if (mp4->loop_flag)
                        mp4->loop_end++; /* assumed, matches num_samples this way */
                    max_offset = 0;
                }
                break;

            case 0x6D6F6F76: { /* "moov" (header) */
                suboffset = offset + 0x08;
                max_suboffset = offset + size;
                while (suboffset < max_suboffset) {
                    uint32_t subsize = read_u32be(suboffset + 0x00,sf);
                    uint32_t subtype = read_u32be(suboffset + 0x04,sf);

                    /* padded in ToRR */
                    if (subsize == 0)
                        break;

                    switch(subtype) {
                        case 0x75647461: /* "udta" */
                            /* CRI subchunk [Imperial SaGa Eclipse (Browser)]
                             * incidentally "moov" header comes after data ("mdat") in CRI's files */
                            if (subsize >= 0x28 && is_id32be(suboffset + 0x08 + 0x04,sf, "criw")) {
                                off_t criw_offset = suboffset + 0x08 + 0x08;

                                mp4->loop_start     = read_s32be(criw_offset + 0x00,sf);
                                mp4->loop_end       = read_s32be(criw_offset + 0x04,sf);
                                mp4->encoder_delay  = read_s32be(criw_offset + 0x08,sf); /* Apple's 2112 */
                                mp4->num_samples    = read_s32be(criw_offset + 0x0c,sf);
                                mp4->loop_flag = (mp4->loop_end > 0);
                                /* next 2 fields are null */
                                max_offset = 0;
                            }
                            break;

                        default:
                            break;
                    }

                    suboffset += subsize;
                }

                break;
            }

            default:
                break;
        }

        offset += size; /* atoms don't seem to need to padding byte, unlike RIFF */
    }
}

/* CRI's encryption info (for lack of a better place) [Final Fantasy Digital Card Game (Browser)]
 * 
 * Like other CRI stuff their MP4 can be encrypted, from file's beginning (including headers).
 * This is more or less how data is decrypted (supposedly, from decompilations), for reference:
 */
#if 0
void criAacCodec_SetDecryptionKey(uint64_t keycode, uint16_t* key) {
    if (!keycode)
        return;
    uint16_t k0 = 4 * ((keycode >> 0)  & 0x0FFF) | 1;
    uint16_t k1 = 2 * ((keycode >> 12) & 0x1FFF) | 1;
    uint16_t k2 = 4 * ((keycode >> 25) & 0x1FFF) | 1;
    uint16_t k3 = 2 * ((keycode >> 38) & 0x3FFF) | 1;

    key[0] = k0 ^ k1;
    key[1] = k1 ^ k2;
    key[2] = k2 ^ k3;
    key[3] = ~k3;

    /* criatomexacb_generate_aac_decryption_key is slightly different, unsure which one is used: */
  //key[0] = k0 ^ k3;
  //key[1] = k2 ^ k3;
  //key[2] = k2 ^ k3;
  //key[3] = ~k3;
}

void criAacCodec_DecryptData(const uint16_t* key, uint8_t* data, uint32_t size) {
    if (data_size)
        return;
    uint16_t seed0 = ~key[3];
    uint16_t seed1 = seed0 ^ key[2];
    uint16_t seed2 = seed1 ^ key[1];
    uint16_t seed3 = seed2 ^ key[0];

    uint16_t xor = 2 * seed0 | 1;
    uint16_t add = 2 * seed0 | 1; /* not seed1 */
    uint16_t mul = 4 * seed2 | 1;

    for (int i = 0; i < data_size; i++) {

        if (!(uint16_t)i) { /* every 0x10000, without modulo */
            mul = (4 * seed2 + seed3 * (mul & 0xFFFC)) & 0xFFFD | 1;
            add = (2 * seed0 + seed1 * (add & 0xFFFE)) | 1;
        }
        xor = xor * mul + add;

        *data ^= (xor >> 8) & 0xFF;
        ++data;
    }
}
#endif

#endif
