#include "meta.h"
#include "../coding/coding.h"
#include "../coding/coding.h"


typedef enum { MSADPCM, DSP, MP3, XMA2 } ckd_codec;

/* CKD RIFF - UbiArt Framework (v1) audio [Rayman Origins (Wii/X360/PS3/PC)] */
VGMSTREAM* init_vgmstream_ubi_ckd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset, first_offset = 0x0c, chunk_offset;
    size_t chunk_size, data_size;
    int loop_flag, channels, interleave = 0, format;
    ckd_codec codec;
    int big_endian;
    uint32_t (*read_u32)(off_t,STREAMFILE*);
    uint16_t (*read_u16)(off_t,STREAMFILE*);

    /* checks */
    if (!is_id32be(0x00,sf, "RIFF"))
        goto fail;
    if (!is_id32be(0x08,sf, "WAVE"))
        goto fail;

    /* .wav.ckd: main (other files are called .xxx.ckd too) */
    if (!check_extensions(sf,"ckd"))
        goto fail;

    /* another slighly funny RIFF, mostly standard except machine endian and minor oddities */
    if (!is_id32be(0x0c,sf, "fmt "))
        goto fail;

    big_endian = guess_endianness32bit(0x04, sf);
    read_u32 = big_endian ? read_u32be : read_u32le;
    read_u16 = big_endian ? read_u16be : read_u16le;

    if (read_u32(0x04, sf) + 0x04 + 0x04 != get_streamfile_size(sf))
        goto fail;

    loop_flag = 0;
    format = read_u16(0x14,sf);
    channels = read_u16(0x16,sf);

    switch(format) {
        case 0x0002:
            if (big_endian) {
                if (read_u32(0x26,sf) != 0x6473704C) /* "dspL" */
                    goto fail;

                /* find data chunk, in 2 variants */
                if (find_chunk_be(sf, 0x64617453,first_offset,0, &chunk_offset,&chunk_size)) { /* "datS" */
                    /* normal interleave */
                    start_offset = chunk_offset;
                    data_size = chunk_size;
                    interleave = 0x08;
                } else if (find_chunk_be(sf, 0x6461744C,first_offset,0, &chunk_offset,&chunk_size)) { /* "datL" */
                    /* mono "datL" or full interleave with a "datR" after the "datL" (no check, pretend it exists) */
                    start_offset = chunk_offset;
                    data_size = chunk_size * channels;
                    interleave = (0x4+0x4) + chunk_size; /* don't forget to skip the "datR"+size chunk */
                } else {
                    goto fail;
                }

                codec = DSP;
            }
            else {
                /* PC has MS-ADPCM, same as wav's except without "fact" (recommended by MS), kinda useless
                 * but might as well have it here */
                if (find_chunk_le(sf, 0x64617461,first_offset,0, &chunk_offset,&chunk_size)) { /* "data" */
                    start_offset = chunk_offset;
                    data_size = chunk_size;
                } else {
                    goto fail;
                }

                interleave = read_u16(0x20, sf);
                if (!msadpcm_check_coefs(sf, 0x28))
                    goto fail;

                /* there is also a "smpl" chunk with full loops too, but other codecs don't have it for the same tracks... */

                codec = MSADPCM;
            }
            break;

        case 0x0055:
            if (read_u32(0x26,sf) != 0x6D736620)    /* "msf " */
                goto fail;
            start_offset = 0x26;
            data_size = read_u32(0x2A,sf);
            codec = MP3;
            break;

        case 0x0166:
            if (read_u32(0x48,sf) != 0x7365656B &&  /* "seek */
                read_u32(0x48,sf) != 0x7365656B)    /* "data" */
                goto fail;

            if (find_chunk_be(sf, 0x64617461,first_offset,0, &chunk_offset,&chunk_size)) { /* "data" */
                start_offset = chunk_offset;
                data_size = chunk_size;
            } else {
                goto fail;
            }

            codec = XMA2;
            break;

        default:
            goto fail;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_u32(0x18,sf);
    vgmstream->num_samples = dsp_bytes_to_samples(data_size, channels);

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->meta_type = meta_UBI_CKD;

    switch(codec) {
        case MSADPCM:
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->frame_size = interleave;
            break;

        case DSP:
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;
            dsp_read_coefs_be(vgmstream,sf, 0x4A, (4+4)+0x60);
            break;

#ifdef VGM_USE_MPEG
        case MP3: {
            vgmstream->codec_data = init_mpeg(sf, start_offset, &vgmstream->coding_type, vgmstream->channels);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = mpeg_bytes_to_samples(data_size, vgmstream->codec_data);
            break;
        }
#endif

#ifdef VGM_USE_FFMPEG
        case XMA2: {
            uint8_t buf[0x100];
            int bytes;

            bytes = ffmpeg_make_riff_xma_from_fmt_chunk(buf, sizeof(buf), 0x14, 0x34, data_size, sf, 1);
            vgmstream->codec_data = init_ffmpeg_header_offset(sf, buf,bytes, start_offset,data_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = read_u32(0x14+0x18,sf);

            xma_fix_raw_samples(vgmstream, sf, start_offset,data_size, 0, 0,0); /* should apply to num_samples? */
            break;
        }
#endif

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
