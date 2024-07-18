#include "meta.h"
#include "../coding/coding.h"

/* Ongakukan RIFF with "ADP" extension [Train Simulator - Midousuji-sen (PS2)] */
VGMSTREAM* init_vgmstream_ongakukan_adp(STREAMFILE* sf)
{
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t file_size;
    bool has_data_chunk = false, has_fact_chunk = false;
    int loop_flag = 0;
    int riff_wave_header_size = 0x2c;
    /* ^ where sound data begins, as a consequence their tools couldn't even write full RIFF WAVE header to file beyond that point.. */
    bool sound_is_adpcm = false;
    int32_t supposed_size, fmt_size, fmt_offset, offset_of_supposed_last_chunk;
    int32_t sample_rate, data_size;
    int16_t num_channels, block_size;

    /* RIFF+WAVE checks */
    if (!is_id32be(0x00, sf, "RIFF")) goto fail;
    if (!is_id32be(0x08, sf, "WAVE")) goto fail;
    /* WAVE "fmt " check */
    if (!is_id32be(0x0c, sf, "fmt ")) goto fail;
    /* "adp" extension check (literally only one) */
    if (!check_extensions(sf, "adp")) goto fail;

    /* catch adp file size from here and use it whenever needed. */
    file_size = get_streamfile_size(sf);

    /* RIFF size from adp file can go beyond actual size (e.g: reported 10MB vs 2MB). do quick calcs around this. */
    supposed_size = ((read_s32le(0x04, sf) - 0x24) >> 2) + 0x2c;
    if (file_size != supposed_size) goto fail;

    /* read entire WAVE "fmt " chunk. we start by reading fmt_size from yours truly and setting fmt_offset. */
    fmt_size = read_s32le(0x10, sf);
    fmt_offset = 0x14;
    if ((fmt_size >= 0x10) && (fmt_size <= 0x12)) /* depending on the adp, fmt_size alternates between 0x10 and 0x12 */
    {
        if (read_s16le(fmt_offset + 0, sf) != 1) goto fail; /* chunk reports codec number as signed little-endian PCM, couldn't be more wrong. */
        num_channels = read_s16le(fmt_offset + 2, sf);
        sample_rate = read_s32le(fmt_offset + 4, sf);
        if (read_s16le(fmt_offset + 14, sf) != 0x10) goto fail; /* bit depth as chunk reports it. */
        /* rest of fmt header is the usual header for 16-bit PCM wav files: bitrate, block size, and the like (see riff.c) */
        /* if fmt_size == 0x12 there is an additional s16 field that's always zero. */
    }
    else {
        goto fail;
    }

    /* now calc the var so we can read either "data" or "fact" chunk; */
    offset_of_supposed_last_chunk = fmt_offset + fmt_size;

    /* we need to get to the last WAVE chunk manually, and that means the calc below. */
     offset_of_supposed_last_chunk = fmt_offset + fmt_size;
    if (is_id32be(offset_of_supposed_last_chunk + 0, sf, "data")) has_data_chunk = true;
    if (is_id32be(offset_of_supposed_last_chunk + 0, sf, "fact")) has_fact_chunk = true;

    /* and because sound data *must* start at 0x2c, they have to bork both chunks too, so they're now essentially useless.
     * they're basically leftovers from original (lossless) WAV files at this point. */
    if (has_data_chunk)
    {
        /* RIFF adp files have leftover "data" chunk size... that does NOT match the ADP file size at hand. */
        supposed_size = (read_s32le(offset_of_supposed_last_chunk + 4, sf) >> 2) + 0x2c;
        if (file_size != supposed_size) goto fail;
    }

    if (has_fact_chunk)
    {
        /* RIFF adp files have also cut off "fact" chunk so we're just left with a useless number now. */
        if (read_s16le(offset_of_supposed_last_chunk + 4, sf) != 4) goto fail;
    }

    /* set start_offset value to riff_wave_header_size and calculate data_size by ourselves, basically how Ongakukan does it also. */
    start_offset = riff_wave_header_size;
    data_size = (int32_t)(file_size) - riff_wave_header_size;

    /* Ongagukan games using this format just read it by checking "ADP" extension in an provided file name of a programmer's own choosing,
     * and if extension is there they just read the reported "number of samples" and "sample_rate" vars
     * from RIFF WAVE "fmt " chunk based on an already-opened file with that same name.
     * and they don't even read RIFF chunks, they just pick these two vars and that's basically it. */

    /* our custom decoder needs at least one flag set. */
    sound_is_adpcm = true;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(num_channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_ONGAKUKAN_RIFF_ADP;
    vgmstream->sample_rate = sample_rate;
    vgmstream->codec_data = init_ongakukan_adp(sf, start_offset, data_size, sound_is_adpcm);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_ONGAKUKAN_ADPCM;
    vgmstream->layout_type = layout_none;
    vgmstream->num_samples = ongakukan_adp_get_samples(vgmstream->codec_data);

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
