#include "meta.h"
#include "../coding/coding.h"


//todo test and rethink usefulness/viability of globally using this
/* generic helper with a usable fields to describe static header values */
typedef struct {
    int codec;
    int type;
    int channels;
    int sample_rate;
    int loop_flag;
    int loop_start;
    int loop_end;

    int total_subsongs;
    int target_subsong;

    size_t file_size;

    off_t info_start;

    off_t header_start;
    size_t header_entry;
    off_t header_offset;

    off_t data_start;
    size_t data_size;

    off_t stream_start;
    size_t stream_size;
} header_t;

/* XSSB - from Artoon games [Blinx (Xbox), Blinx 2 (Xbox)] */
VGMSTREAM* init_vgmstream_xssb(STREAMFILE *sf) {
    VGMSTREAM *vgmstream = NULL;
    //off_t start_offset, header_offset, data_start, info_start, header_start;
    //size_t header_size, stream_size;
    //int loop_flag, channel_count, sample_rate, codec, loop_start, loop_end;
    //int total_subsongs, target_subsong = streamFile->stream_index;
    header_t h;


    /* checks */
    /* .bin: from named files inside .ipk bigfiles */
    if (!check_extensions(sf, "bin,lbin"))
        goto fail;

    if (read_u32be(0x00, sf) != 0x58535342) /* "XSSB" */
        goto fail;
    /* 0x04: null */
    /* 0x08: date-version ('20011217' in hex) */
    /* 0x0c: null */

    h.info_start   = read_s32le(0x10, sf);
    h.header_start = read_s32le(0x14, sf);
    h.data_start   = read_s32le(0x18, sf);
    /* 0x1c: null */

    h.header_entry = read_s16le(h.info_start + 0x00, sf);
    /* 0x02: always 127 */

    /* get subsongs from header entries */
    {
        off_t offset = h.header_start;

        h.total_subsongs = 0;
        h.target_subsong = sf->stream_index <= 0 ? 1 : sf->stream_index;

        h.header_offset  = 0;
        while (offset < h.data_start) {
            /* headers are just pasted together and then padding */
            if (read_u32be(offset, sf) == 0)
                break;
            h.total_subsongs++;

            if (h.target_subsong == h.total_subsongs) {
                h.header_offset = offset;
            }

            offset += h.header_entry;
        }

        if (h.header_offset == 0)
            goto fail;
        if (h.target_subsong > h.total_subsongs || h.total_subsongs < 1)
            goto fail;
    }

    /* read header */
    h.codec         = read_s16le(h.header_offset + 0x00, sf);
    h.channels      = read_s16le(h.header_offset + 0x02, sf);
    h.sample_rate   = read_u16le(h.header_offset + 0x04, sf);
    /* 0x08: bitrate */
    /* 0x0c: block align/bps */
    /* 0x10: 0=PCM, 2=XBOX-IMA? */
    h.stream_start  = read_s32le(h.header_offset + 0x14, sf) + h.data_start;
    h.stream_size   = read_s32le(h.header_offset + 0x18, sf);
    h.loop_start    = read_s32le(h.header_offset + 0x1c, sf);
    h.loop_end      = read_s32le(h.header_offset + 0x20, sf);
    /* others: unknown and mostly fixed values */
    h.loop_flag = (h.loop_end > 0);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(h.channels, h.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XSSB;
    vgmstream->sample_rate = h.sample_rate;
    vgmstream->loop_start_sample = h.loop_start;
    vgmstream->loop_end_sample = h.loop_end;
    vgmstream->num_streams = h.total_subsongs;
    vgmstream->stream_size = h.stream_size;

    switch(h.codec) {
        case 0x01:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x01;

            vgmstream->num_samples = pcm_bytes_to_samples(h.stream_size, h.channels, 16);
            break;

        case 0x69:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = xbox_ima_bytes_to_samples(h.stream_size, h.channels);
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, h.stream_start))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
