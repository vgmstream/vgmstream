#include "meta.h"
#include "../coding/coding.h"


/* GWB+GWD - Ubisoft bank [Monster 4x4: World Circuit  (Wii)] */
VGMSTREAM* init_vgmstream_gwb_gwd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    uint32_t stream_offset = 0, stream_size = 0, coef_offset;
    int loop_flag, channels, sample_rate, interleave = 0;
    uint32_t loop_start, loop_end;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    int version = read_u8(0x00, sf);
    if (version != 6 && version != 7)
        return NULL;
    if (read_u32be(0x01, sf) > 0x0400) /* ID, max seen */
        return NULL;
    if (get_streamfile_size(sf) > 0x2000) /* arbitrary max */
        return NULL;
    if (!check_extensions(sf,"gwb"))
        return NULL;

    /* format (vaguely similar to ubi's hx and such banks)
     * common
     *   00: version (06/07, both found in the same game)
     *   01: file ID (low number: 0x0001, 0x0342...)
     * v6:
     *   05: subsongs
     * v7
     *   05: null
     *   09: subsongs
     * 
     * per subsong:
     * - 00: flags: (v6: 09=stereo, 02=mono; v7: 0a=stereo)
     * - 01: id (ex. 0x0002, 0x0343...)
     * v6
     * - 05: 0x4a header * channels
     * v7
     * - 05: always 0x02?
     * - 09: stream offset
     * - 0d: stream size
     * - 11: always 5
     * - 15: 0x4a header * channels
     * 
     * per header:
     * - 00: loop flag
     * - 04: sample rate
     * - 08: loop start nibbles
     * - 0c: loop end nibbles
     * - 10: end nibble
     * - 14: start nibble (after DSP frame header, so uses 0x02 at file start)
     * - 18: null
     * - 1c: coefs + gain + initial ps/hists + loop ps/hists
     * Data in .gwd is N headerless DSPs. All nibble values are absolute within the file. */

    uint32_t offset = version == 6 ? 0x05 : 0x09;

    total_subsongs = read_s32be(offset, sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) return NULL;
    offset += 0x04;

    /* find target header */
    for (int i = 0; i < total_subsongs; i++) {
        if (i + 1 == target_subsong)
            break;

        uint8_t type = read_u8(offset + 0x00, sf);
        if (type != 0x0a && type != 0x09 && type != 0x02)
            goto fail;

        offset += 0x05 + (version == 7 ? 0x10 : 0);
        offset += 0x4a * (type & 0x08 ? 2 : 1);
    }

    /* header */
    {
        uint32_t st_nibble, ed_nibble, ls_nibble, le_nibble;
        uint8_t type = read_u8(offset + 0x00, sf);
        channels = (type & 0x08 ? 2 : 1);

        offset += 0x05;
        if (version == 7) {
            stream_offset = read_u32be(offset + 0x04, sf);
            stream_size = read_u32be(offset + 0x08, sf);
            interleave = 0x4000;
            offset += 0x10;
        }
        loop_flag   = read_u32be(offset + 0x00, sf) == 1;
        sample_rate = read_u32be(offset + 0x04, sf);
        ls_nibble   = read_u32be(offset + 0x08, sf);
        le_nibble   = read_u32be(offset + 0x0c, sf);
        ed_nibble   = read_u32be(offset + 0x10, sf);
        st_nibble   = read_u32be(offset + 0x14, sf);
        coef_offset = offset + 0x1c;

        if (version == 6) {
            stream_offset = ((st_nibble - 2) / 2);
            stream_size = ((ed_nibble - st_nibble - 2) / 2) * channels;

            /* stereo repeats loop flag/sample rate/offsets/etc but simplify */
            if (channels == 2) {
                uint32_t s2_nibble = read_u32be(offset + 0x4a + 0x14, sf);
                interleave = (s2_nibble - st_nibble) / 2;
            }
        }
        loop_start = ((ls_nibble - 2) / 2 - stream_offset);
        loop_end = ((le_nibble) / 2 - stream_offset) * channels;
    }

    /* files also have an optional companion .gsb with volume/etc config that seems to be adapted from Xbox's .xsb */
    sf_body = open_streamfile_by_ext(sf, "gwd");
    if (!sf_body) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_GWB_GWD;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = dsp_bytes_to_samples(stream_size, channels);
    vgmstream->loop_start_sample = dsp_bytes_to_samples(loop_start, channels);
    vgmstream->loop_end_sample = dsp_bytes_to_samples(loop_end, channels);;

    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->coding_type = coding_NGC_DSP;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    dsp_read_coefs_be(vgmstream, sf, coef_offset + 0x00, 0x4a);
    dsp_read_hist_be (vgmstream, sf, coef_offset + 0x24, 0x4a);


    if (!vgmstream_open_stream(vgmstream, sf_body, stream_offset))
        goto fail;
    close_streamfile(sf_body);
    return vgmstream;

fail:
    close_streamfile(sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}
