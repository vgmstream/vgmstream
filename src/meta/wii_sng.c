#include "meta.h"
#include "../util/meta_utils.h"
#include "../coding/coding.h"


static STREAMFILE* setup_song_streamfile(STREAMFILE* sf) {
    STREAMFILE* new_sf = NULL;
    STREAMFILE* multi_sf[2] = {0};

    multi_sf[0] = open_wrap_streamfile(sf);
    multi_sf[1] = open_streamfile_by_ext(sf, "sf0");
    new_sf = open_multifile_streamfile_f(multi_sf, 2);
    return new_sf;
}

/* Song - from Monster Games with split data [ExciteBots (Wii)] */
VGMSTREAM* init_vgmstream_song_monster(STREAMFILE* sf) {

    /* checks*/
    int sample_rate = read_s32le(0x00,sf);
    if (sample_rate != 32000)
        return NULL;
    if (read_u32be(0x04,sf))
        return NULL;
    // .sn0/sng: common
    if (!check_extensions(sf, "sn0,sng"))
        return NULL;


    meta_header_t h = {0};
    h.meta = meta_SONG_MONSTER;

    h.chan_size     = read_u32le(0x08,sf);
    h.interleave    = read_u32le(0x0c,sf);
    if (read_u32be(0x10,sf))
        return NULL;
    h.stream_offset = 0x14;

    h.sample_rate = sample_rate;
    h.channels = 2;
    if (h.interleave * h.channels + 0x14 != get_streamfile_size(sf))
        return NULL;

    h.coding = coding_PCM16BE;
    h.layout = layout_interleave;

    uint32_t total_size = (h.chan_size + h.interleave) * h.channels;
    h.num_samples = pcm16_bytes_to_samples(total_size, h.channels);

    // first block is in this header, rest of data in separate sf0; join both to play as one (could use segments too)
    STREAMFILE* temp_sf = setup_song_streamfile(sf);
    if (!temp_sf)
        return NULL;
    h.open_stream = true;
    h.sf = temp_sf;

    VGMSTREAM* v = alloc_metastream(&h);
    close_streamfile(temp_sf);
    return v;
}
