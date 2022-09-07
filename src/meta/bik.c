#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

static int bink_get_info(STREAMFILE* sf, int target_subsong, int* p_total_subsongs, size_t* p_stream_size, int* p_channels, int* p_sample_rate, int* p_num_samples);

/* BINK 1/2 - RAD Game Tools movies (audio/video format) */
VGMSTREAM* init_vgmstream_bik(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    int channels = 0, loop_flag = 0, sample_rate = 0, num_samples = 0;
    int total_subsongs = 0, target_subsong = sf->stream_index;
    size_t stream_size;

    /* checks */
    /* bink1/2 header, followed by version-char (audio is the same) */
    if ((read_u32be(0x00,sf) & 0xffffff00) != get_id32be("BIK\0") &&
        (read_u32be(0x00,sf) & 0xffffff00) != get_id32be("KB2\0"))
        goto fail;

    /* .bik/bik2/bk2: standard
     * .xmv: Reflections games [Driver: Parallel Lines (Wii), Emergency Heroes (Wii)]
     * .bik.ps3: Neversoft games [Guitar Hero: Warriors of Rock (PS3)]
     * .bik.xen: Neversoft games [various Guitar Hero (PC/PS3/X360)]
     * .vid: Etrange Libellules games [Alice in Wonderland (PC)] 
     * .bika: fake extension for demuxed audio */
    if (!check_extensions(sf,"bik,bik2,bk2,ps3,xmv,xen,vid,bika"))
        goto fail;

    /* find target stream info and samples */
    if (!bink_get_info(sf, target_subsong, &total_subsongs, &stream_size, &channels, &sample_rate, &num_samples))
        goto fail;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->layout_type = layout_none;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_BINK;

#ifdef VGM_USE_FFMPEG
    {
        /* target_subsong should be passed manually */
        vgmstream->codec_data = init_ffmpeg_header_offset_subsong(sf, NULL,0, 0x0,0, target_subsong);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
    }
#else
    goto fail;
#endif

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/**
 * Gets stream info, and number of samples in a BINK file by reading all frames' headers (as it's VBR),
 * as they are not in the main header. The header for BINK1 and 2 is the same.
 * (a ~3 min movie needs ~6000-7000 frames = fseeks, should be fast enough)
 */
static int bink_get_info(STREAMFILE* sf, int target_subsong, int* p_total_subsongs, size_t* p_stream_size, int* p_channels, int* p_sample_rate, int* p_num_samples) {
    uint32_t* offsets = NULL;
    uint32_t num_frames, num_samples_b = 0;
    off_t cur_offset;
    int i, j, sample_rate, channels;
    int total_subsongs;
    size_t stream_size = 0;

    size_t filesize = get_streamfile_size(sf);
    uint32_t signature = (read_32bitBE(0x00,sf) & 0xffffff00);
    uint8_t revision = (read_32bitBE(0x00,sf) & 0xFF);


    if (read_32bitLE(0x04,sf) + 0x08 != filesize)
        goto fail;

    num_frames = (uint32_t)read_32bitLE(0x08,sf);
    if (num_frames == 0 || num_frames > 0x100000) goto fail; /* something must be off (avoids big allocs below) */

    /* multichannel/multilanguage audio is usually N streams of stereo/mono, no way to know channel layout */
    total_subsongs = read_32bitLE(0x28,sf);
    if (total_subsongs < 1) {
        vgm_logi("BIK: no audio in movie (ignore)\n");
        goto fail;
    }

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs > 255) goto fail;


    /* find stream info and position in offset table */
    cur_offset = 0x2c;
    if ((signature == 0x42494B00 && (revision == 0x6b)) || /* k */
        (signature == 0x4B423200 && (revision == 0x69 || revision == 0x6a || revision == 0x6b))) /* i,j,k */
        cur_offset += 0x04; /* unknown v2 header field */
    cur_offset += 0x04*total_subsongs; /* skip streams max packet bytes */
    sample_rate   = (uint16_t)read_16bitLE(cur_offset+0x04*(target_subsong-1)+0x00,sf);
    channels = (uint16_t)read_16bitLE(cur_offset+0x04*(target_subsong-1)+0x02,sf) & 0x2000 ? 2 : 1; /* stereo flag */
    cur_offset += 0x04*total_subsongs; /* skip streams info */
    cur_offset += 0x04*total_subsongs; /* skip streams ids */


    /* read frame offsets in a buffer, to avoid fseeking to the table back and forth */
    offsets = malloc(sizeof(uint32_t) * num_frames);
    if (!offsets) goto fail;

    for (i=0; i < num_frames; i++) {
        offsets[i] = read_32bitLE(cur_offset,sf) & 0xFFFFFFFE; /* mask first bit (= keyframe) */
        cur_offset += 0x4;

        if (offsets[i] > filesize) goto fail;
    }
    /* after the last index is the file size, validate just in case */
    if (read_32bitLE(cur_offset,sf) != filesize) goto fail;

    /* read each frame header and sum all samples
     * a frame has N audio packets with a header (one per stream) + video packet */
    for (i=0; i < num_frames; i++) {
        cur_offset = offsets[i];

        /* read audio packet headers per stream */
        for (j=0; j < total_subsongs; j++) {
            uint32_t ap_size = read_32bitLE(cur_offset+0x00,sf); /* not counting this int */

            if (j == target_subsong-1) {
                stream_size += 0x04 + ap_size;
                if (ap_size > 0)
                    num_samples_b += read_32bitLE(cur_offset+0x04,sf); /* decoded samples in bytes */
                break; /* next frame */
            }
            else { /* next stream packet or frame */
                cur_offset += 4 + ap_size; //todo sometimes ap_size doesn't include itself (+4), others it does?
            }
        }
    }

    free(offsets);


    if (p_total_subsongs) *p_total_subsongs = total_subsongs;
    if (p_stream_size)    *p_stream_size = stream_size;
    if (p_sample_rate)    *p_sample_rate = sample_rate;
    if (p_channels)  *p_channels = channels;
    //todo returns a few more samples (~48) than binkconv.exe?
    if (p_num_samples)    *p_num_samples = num_samples_b / (2 * channels);

    return 1;

fail:
    free(offsets);
    return 0;
}
