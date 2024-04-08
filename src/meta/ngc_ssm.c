#include "meta.h"
#include "../util/meta_utils.h"
#include "../coding/coding.h"


/* .SSM - from Hal Laboratory games [Smash Bros Melee! (GC), Konjiki no Gashbell: YnTB Full Power (GC), Kururin Squash! (GC)] */
VGMSTREAM* init_vgmstream_ssm(STREAMFILE* sf) {
    meta_header_t h = {0};


    /* checks */
    h.head_size = read_u32be(0x00,sf);
    h.data_size = read_u32be(0x04,sf);
    h.total_subsongs = read_s32be(0x08,sf);
    int file_id = read_u32be(0x0c,sf);

    /* extra tests + arbitrary maxes since no good header ID and values aren't exact */
    if (0x10 + h.head_size + h.data_size > get_streamfile_size(sf))
        return NULL;
    if (h.head_size < h.total_subsongs * 0x48 || h.total_subsongs <= 0 || h.total_subsongs > 0x1000 || file_id > 0x1000)
        return NULL;
    if (!check_extensions(sf, "ssm"))
        return NULL;

    h.target_subsong = sf->stream_index;
    if (!check_subsongs(&h.target_subsong, h.total_subsongs))
        return NULL;

    /* sometimes there is padding after head_size, DSP's start ps matches this */
    h.data_offset = get_streamfile_size(sf) - h.data_size; //0x10 + h.head_size;


    uint32_t offset = 0x10;
    for (int i = 0; i < h.total_subsongs; i++) {
        int channels = read_u32be(offset + 0x00,sf);
        if (channels < 1 || channels > 2) return NULL;

        if (i + 1 == h.target_subsong) {
            h.channels      = read_u32be(offset + 0x00,sf);
            h.sample_rate   = read_s32be(offset + 0x04,sf);

            /* use first channel as base */
            h.loop_flag     = read_s16be(offset + 0x08,sf); 
            h.loop_start    = read_u32be(offset + 0x0c,sf);
            h.chan_size     = read_u32be(offset + 0x10,sf);
            h.stream_offset = read_s32be(offset + 0x14,sf);
            h.coefs_offset = offset + 0x18;
            h.coefs_spacing = 0x40;
            h.hists_offset = h.coefs_offset + 0x24;
            h.hists_spacing = h.coefs_spacing;
            if (h.channels >= 2) {
                h.interleave = read_s32be(offset + 0x54,sf); /* use 2nd channel offset as interleave */
            }

            break;
        }

        offset += 0x08 + channels * 0x40;
    }

    /* oddly enough values are in absolute nibbles within the stream, adjust them here
     * rarely may even point to a nibble after the header one (ex. 0x1005), but it's adjusted to 0x00 here */
    h.loop_start -= h.stream_offset;
    h.chan_size  -= h.stream_offset;
    if (h.interleave)
        h.interleave -= h.stream_offset;

    h.loop_start    = dsp_nibbles_to_samples(h.loop_start);
    h.loop_end      = dsp_nibbles_to_samples(h.chan_size);
    h.num_samples   = h.loop_end;

    h.stream_offset = (h.stream_offset / 0x10 * 0x08) + h.data_offset;
    h.stream_size   = (h.chan_size / 0x10 * 0x08 + (h.chan_size % 0x08 ? 0x08 : 0x00)) * h.channels;
    h.interleave    = h.interleave / 0x10 * 0x08;

    h.coding = coding_NGC_DSP;
    h.layout = layout_interleave; //TODO layout flat + channel offset may be more appropriate
    h.meta = meta_SSM;

    h.sf = sf;
    h.big_endian = true;
    h.open_stream = true;

    return alloc_metastream(&h);
}
