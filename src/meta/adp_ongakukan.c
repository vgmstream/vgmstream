#include "meta.h"
#include "../coding/coding.h"

/* Ongakukan RIFF with "ADP" extension [Train Simulator: Midousuji-sen (PS2), Mobile Train Simulator (PSP)] */
VGMSTREAM* init_vgmstream_adp_ongakukan(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0;
    bool sound_is_adpcm = false;
    int32_t fmt_size, fmt_offset;
    int32_t sample_rate, data_size;
    int16_t channels;
    int32_t expected_size, pcm_size, diff, fact_offset;

    /* checks */
    if (!is_id32be(0x00, sf, "RIFF"))
        return NULL;
    if (!check_extensions(sf, "adp"))
        return NULL;

    /* Format starts like RIFF but doesn't have valid chunks beyond fmt (ADPCM data overwrites anything after 0x2c). */

    start_offset = 0x2c;
    data_size = get_streamfile_size(sf) - start_offset;

    /* RIFF size seem to match original PCM .wav, while encoded .adp data equals or is slightly smaller that that */
    expected_size = (read_u32le(0x04, sf) - 0x24);
    pcm_size = data_size * 2 * sizeof(short); // * channels
    diff = expected_size - pcm_size;
    if (diff < 0 || diff > 14)
        return NULL;

    if (!is_id32be(0x08, sf, "WAVE"))
        return NULL;
    if (!is_id32be(0x0c, sf, "fmt "))
        return NULL;

    fmt_size = read_s32le(0x10, sf);
    /* depending on the adp, fmt_size alternates between 0x10 and 0x12 */
    if (fmt_size < 0x10 || fmt_size > 0x12)
        goto fail;
    fmt_offset = 0x14;

    if (read_s16le(fmt_offset + 0x00, sf) != 0x0001) /* PCM format */
        goto fail;
    channels = read_s16le(fmt_offset + 0x02, sf);
    if (channels != 1) /* not seen (decoder can't handle it) */
        goto fail;
    sample_rate = read_s32le(fmt_offset + 0x04, sf);
    if (read_s16le(fmt_offset + 0x0e, sf) != 16) /* PCM bit depth */
        goto fail;
    /* rest of fmt header is the usual header for 16-bit PCM wav files: bitrate, block size, and the like (see riff.c) */
    /* if fmt_size == 0x12 there is an additional s16 field that may or may not be zero. */

    /* depending on the adp, "fact" chunk is "movable" as we'll see later. */
    fact_offset = fmt_offset + fmt_size;
    if (fact_offset < 0x24 || fact_offset > 0x28)
        goto fail;

    /* for next chunk, if "data" then it's at fixed offset (0x24), regardless of fmt_size (fmt_size 0x12 with "data" at 0x24 is possible).
     * if "fact" however, offset goes AFTER fmt_size (mostly 0x26 and rarely 0x24).
     * "data" has chunk size (does not match ADP size but original WAV) and "fact" chunk size is always 0x04 (whether cut off or otherwise). */
    if (!is_id32be(0x24, sf, "data") && !is_id32be(fact_offset, sf, "fact"))
        goto fail;

    /* Ongagukan games using this format just read it by checking "ADP" extension in a provided file name,
     * and if extension is there they just read the reported "number of samples" and "sample_rate" vars
     * from RIFF WAVE "fmt " chunk based on an already-opened file with that same name.
     * and they don't even read RIFF chunks, they just pick these two vars and that's basically it. */

    sound_is_adpcm = true;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
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
