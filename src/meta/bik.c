#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

#ifdef VGM_USE_FFMPEG

static uint32_t bik_get_num_samples(STREAMFILE *streamFile, int bits_per_sample);

/* BIK 1/2 - RAD Game Tools movies (audio/video format) */
VGMSTREAM * init_vgmstream_bik(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    ffmpeg_codec_data *data = NULL;

    /* check extension, case insensitive (bika = manually demuxed audio) */
    if (!check_extensions(streamFile,"bik,bika,bik2,bik2a,bk2,bk2a")) goto fail;

    /* check header "BIK" (bik1) or "KB2" (bik2) (followed by version-char) */
    if ((read_32bitBE(0x00,streamFile) & 0xffffff00) != 0x42494B00 &&
        (read_32bitBE(0x00,streamFile) & 0xffffff00) != 0x4B423200 ) goto fail;

    /* FFmpeg can parse BIK audio, but can't get the number of samples, which vgmstream needs.
     * The only way to get them is to read all frame headers */
    data = init_ffmpeg_offset(streamFile, 0x0, get_streamfile_size(streamFile));
    if (!data) goto fail;

    vgmstream = allocate_vgmstream(data->channels, 0); /* alloc FFmpeg first to get channel count */
    if (!vgmstream) goto fail;
    vgmstream->codec_data = data;
    vgmstream->coding_type = coding_FFmpeg;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_FFmpeg;
    vgmstream->sample_rate = data->sampleRate;

    /* manually get num_samples since data->totalSamples is always 0 */
    vgmstream->num_samples = bik_get_num_samples(streamFile, data->bitsPerSample);
    if (vgmstream->num_samples == 0)
        goto fail;

    return vgmstream;

fail:
    free_ffmpeg(data);
    if (vgmstream) {
        vgmstream->codec_data = NULL;
        close_vgmstream(vgmstream);
    }
    return NULL;
}

/**
 * Gets the number of samples in a BIK file by reading all frames' headers,
 * as they are not in the main header. The header for BIK1 and 2 is the same.
 * (a ~3 min movie needs ~6000-7000 frames = fseeks, should be fast enough)
 *
 * Needs bits per sample to calculate PCM samples, since most bink audio seems to use 32, actually.
 */
static uint32_t bik_get_num_samples(STREAMFILE *streamFile, int bits_per_sample) {
    uint32_t *offsets = NULL;
    uint32_t num_samples_b = 0;
    off_t cur_offset;
    size_t filesize;
    int i, j, num_frames, num_tracks;
    int target_stream = 0;

    filesize = get_streamfile_size(streamFile);

    num_frames = read_32bitLE(0x08,streamFile);
    if (num_frames <= 0) goto fail;
    if (num_frames > 0x100000) goto fail; /* something must be off (avoids big allocs below) */

    /* multichannel audio is usually N tracks of stereo/mono, no way to know channel layout */
    num_tracks = read_32bitLE(0x28,streamFile);
    if (num_tracks<=0 || num_tracks > 255) goto fail;

    /* find the frame index table, which is after 3 audio headers of size 4 for each track */
    cur_offset = 0x2c + num_tracks*4 * 3;

    /* read offsets in a buffer, to avoid fseeking to the table back and forth
     * the number of frames can be highly variable so we'll alloc */
    offsets = malloc(sizeof(uint32_t) * num_frames);
    if (!offsets) goto fail;

    for (i=0; i < num_frames; i++) {
        offsets[i] = read_32bitLE(cur_offset,streamFile) & 0xFFFFFFFE; /* mask first bit (= keyframe) */
        cur_offset += 0x4;

        if (offsets[i] > filesize) goto fail;
    }
    /* after the last index is the file size, validate just in case */
    if (read_32bitLE(cur_offset,streamFile)!=filesize) goto fail;

    /* multistream support just for fun (FFmpeg should select the same target stream)
     * (num_samples for other streams seem erratic though) */
    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || target_stream > num_tracks || num_tracks < 1) goto fail;

    /* read each frame header and sum all samples
     * a frame has N audio packets with header (one per track) + video packet */
    for (i=0; i < num_frames; i++) {
        cur_offset = offsets[i];

        /* read audio packet headers */
        for (j=0; j < num_tracks; j++) {
            uint32_t ap_size, samples_b;
            ap_size = read_32bitLE(cur_offset+0x00,streamFile); /* not counting this int */
            samples_b = read_32bitLE(cur_offset+0x04,streamFile); /* decoded samples in bytes */
            if (ap_size==0) break; /* no audio in this frame */

            if (j == target_stream-1) { /* target samples found, read next frame */
                num_samples_b += samples_b;
                break;
            } else { /* check next audio packet */
                cur_offset += 4 + ap_size; /* todo sometimes ap_size doesn't include itself (+4), others it does? */
            }
        }
    }


    free(offsets);
    return num_samples_b / (bits_per_sample / 8);

fail:
    free(offsets);
    return 0;
}

#endif
