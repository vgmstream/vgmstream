#include "meta.h"
#include "../coding/coding.h"


/* FFDL - Matrix Software wrapper [Final Fantasy Dimensions (Android/iOS)] */
VGMSTREAM* init_vgmstream_ffdl(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int loop_flag = 0, is_ffdl = 0;
    int32_t num_samples = 0, loop_start_sample = 0, loop_end_sample = 0;
    off_t start_offset;
    size_t file_size;


    /* checks */
    if (!is_id32be(0x00,sf, "FFDL") &&
        !is_id32be(0x00,sf, "mtxs"))
        goto fail;

    /* .ogg/logg: probable extension for Android
     * .mp4/lmp4: probable extension for iOS
     * .bin: iOS FFDL extension
     * (extensionless): for FFDL files without names in Android .obb bigfile */
    if (!check_extensions(sf, "ogg,logg,mp4,lmp4,bin,"))
        goto fail;

    /* "FFDL" is a wrapper used in all of the game's files, that may contain standard
     * Ogg/MP4 or "mtxs" w/ loops + Ogg/MP4, and may concatenate multiple of them
     * (without size in sight), so they should be split externally first. */

    /* may start with wrapper (not split) */
    start_offset = 0x00;
    if (is_id32be(0x00,sf, "FFDL")) {
        is_ffdl = 1;
        start_offset += 0x04;
    }

    /* may start with sample info (split) or after "FFDL" */
    if (is_id32be(start_offset+0x00,sf, "mtxs")) {
        is_ffdl = 1;

        num_samples       = read_s32le(start_offset + 0x04,sf);
        loop_start_sample = read_s32le(start_offset + 0x08,sf);
        loop_end_sample   = read_s32le(start_offset + 0x0c,sf);
        loop_flag = !(loop_start_sample==0 && loop_end_sample==num_samples);

        start_offset += 0x10;
    }

    /* don't parse regular files */
    if (!is_ffdl)
        goto fail;

    file_size = get_streamfile_size(sf) - start_offset;

    if (read_u32be(start_offset + 0x00,sf) == 0x4F676753) { /* "OggS" */
        temp_sf = setup_subfile_streamfile(sf, start_offset, file_size, "ogg");
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream_ogg_vorbis(temp_sf);
        if (!vgmstream) goto fail;
    }
    else if (read_u32be(start_offset + 0x04,sf) == 0x66747970) { /* "ftyp" after atom size */
#ifdef VGM_USE_FFMPEG
        temp_sf = setup_subfile_streamfile(sf, start_offset, file_size, "mp4");
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream_mp4_aac_ffmpeg(temp_sf);
        if (!vgmstream) goto fail;
#else
    goto fail;
#endif
    }
    else {
        goto fail;
    }

    /* install loops */
    if (loop_flag) {
        /* num_samples is erratic (can be bigger = padded, or smaller = cut; doesn't matter for looping though) */
        //;VGM_ASSERT(vgmstream->num_samples != num_samples,
        //        "FFDL: mtxs samples = %i vs num_samples = %i\n", num_samples, vgmstream->num_samples);
        //vgmstream->num_samples = num_samples;

        /* loop samples are within num_samples, and don't have encoder delay (loop_start=0 starts from encoder_delay) */
        vgmstream_force_loop(vgmstream, 1, loop_start_sample, loop_end_sample);
    }

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
