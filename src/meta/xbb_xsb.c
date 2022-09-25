#include "meta.h"
#include "../coding/coding.h"
#include "../util/chunks.h"
#include "../util/endianness.h"

/* xbb defines, made up for easier identification. */
#define xbb_has_riff_chunk    0x01
#define xbb_has_wave_chunk    0x02
#define xbb_has_fmt_chunk     0x04
#define xbb_has_data_chunk    0x08
#define xbb_has_external_data 0x10
#define xbb_has_smpl_chunk    0x20

typedef struct {
    /* xbb format stuff */
    uint32_t xbb_whole_size;
    uint32_t xbb_real_size;
    uint32_t entries;
    uint32_t entry_offset;

    /* RIFF chunks */
    uint32_t chunk_type;
    uint32_t chunk_size;
    uint32_t riff_offset;
    uint32_t riff_size;
    uint32_t fmt_offset;
    uint32_t fmt_size;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t smpl_offset;
    uint32_t smpl_size;

    /* RIFF WAVE format header */
    uint16_t codec;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t avg_bps; // "average_bytes_per_second"
    uint16_t blkalgn; // "block_align"
    uint16_t bit_depth; // "bit_depth"
    uint16_t out_bps_pch; // "out_bytes_per_second_per_channel"
    uint16_t out_blkalgn_pch; // "out_block_align_per_channel"

    /* RIFF WAVE data header */
    uint32_t external_data_offset;
    uint32_t external_data_size;

    /* other stuff */
    uint8_t xbb_flags;
    uint32_t stream_offset;
    uint32_t stream_size;
    uint32_t riff_size_from_zero;
    uint32_t is_riff;
    uint32_t is_wave;
} xbb_header;

/* .xbb+.xsb - from Gladius [Xbox] */
VGMSTREAM* init_vgmstream_xbb_xsb(STREAMFILE* sf)
{
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_h = NULL, *sf_b = NULL, *sf_data = NULL;
    xbb_header xbb = { 0 };
    int target_subsong = sf->stream_index;
    int i;

    /* checks */
    if (!check_extensions(sf, "xbb")) goto fail;

    sf_h = sf;

    xbb.xbb_whole_size = read_u32le(0, sf_h);
    if (xbb.xbb_whole_size < 4) goto fail;
    xbb.xbb_real_size = get_streamfile_size(sf_h);
    if (xbb.xbb_real_size != (xbb.xbb_whole_size + 4)) goto fail;

    xbb.entries = read_u32le(4, sf_h);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > xbb.entries || xbb.entries <= 0) goto fail;

    xbb.entry_offset = 8;
    for (i = 0; i < xbb.entries; i++)
    {
        xbb.xbb_flags = 0;
        /* read RIFF chunk */
        xbb.is_riff = read_u32le(xbb.entry_offset + 0, sf_h);
        if (xbb.is_riff == 0x46464952) { // "RIFF"
            xbb.xbb_flags |= xbb_has_riff_chunk;
            xbb.riff_size = read_u32le(xbb.entry_offset + 4, sf_h);
            xbb.entry_offset += 8;
            xbb.riff_offset = xbb.entry_offset;
            xbb.entry_offset += xbb.riff_size;
        } else {
            goto fail;
        }

        /* check if "RIFF" chunk exists, unlikely. */
        if (!(xbb.xbb_flags & xbb_has_riff_chunk)) goto fail;

        xbb.riff_size_from_zero = 0;
        /* read "WAVE" chunk */
        xbb.is_wave = read_u32le(xbb.riff_offset + 0, sf_h);
        if (xbb.is_wave == 0x45564157) { // "WAVE"
            xbb.xbb_flags |= xbb_has_wave_chunk;
            xbb.riff_size_from_zero += 4;
            xbb.riff_offset += 4;
        } else {
            goto fail;
        }

        /* check if "WAVE" chunk exists, unlikely. */
        if (!(xbb.xbb_flags & xbb_has_wave_chunk)) goto fail;

        while (xbb.riff_size_from_zero < xbb.riff_size) {
            /* read the rest of the chunks */
            xbb.chunk_type = read_u32le(xbb.riff_offset + 0, sf_h);
            xbb.chunk_size = read_u32le(xbb.riff_offset + 4, sf_h);
            xbb.riff_offset += 8;
            xbb.riff_size_from_zero += 8;
            switch (xbb.chunk_type) {
                case 0x20746d66: // "fmt "
                    xbb.xbb_flags |= xbb_has_fmt_chunk;
                    xbb.fmt_offset = xbb.riff_offset;
                    xbb.fmt_size = xbb.chunk_size;
                    xbb.riff_offset += xbb.fmt_size;
                    xbb.riff_size_from_zero += xbb.fmt_size;
                    break;
                case 0x61746164: // "data"
                    xbb.data_offset = xbb.riff_offset;
                    xbb.xbb_flags |= xbb_has_data_chunk;
                    if (xbb.chunk_size == 0xfffffff8) {
                        xbb.xbb_flags |= xbb_has_external_data;
                        xbb.riff_offset += 8;
                        xbb.riff_size_from_zero += 8;
                    }
                    else {
                        xbb.data_size = xbb.chunk_size;
                        xbb.riff_offset += xbb.data_size;
                        xbb.riff_size_from_zero += xbb.data_size;
                    }
                    break;
                case 0x6c706d73: // "smpl"
                    xbb.xbb_flags |= xbb_has_smpl_chunk;
                    xbb.smpl_offset = xbb.riff_offset;
                    xbb.smpl_size = xbb.chunk_size;
                    xbb.riff_offset += xbb.smpl_size;
                    xbb.riff_size_from_zero += xbb.smpl_size;
                    break;
                default:
                    break;
            }
        }

        if (i + 1 == target_subsong) {
            /* check if "fmt " chunk exists, unlikely. */
            //if (!(xbb.xbb_flags & xbb_has_fmt_chunk)) goto fail; // alternative in case the if-else condition isn't enough or looks ugly.
            if ((xbb.xbb_flags & xbb_has_fmt_chunk)) {
                /* read all "fmt " info there is. */
                if (xbb.fmt_size == 0x14) {
                    /* usual Xbox IMA ADPCM codec header data */
                    xbb.codec = read_u16le(xbb.fmt_offset + 0, sf_h);
                    xbb.channels = read_u16le(xbb.fmt_offset + 2, sf_h);
                    xbb.sample_rate = read_u32le(xbb.fmt_offset + 4, sf_h);
                    xbb.avg_bps = read_u32le(xbb.fmt_offset + 8, sf_h);
                    xbb.blkalgn = read_u16le(xbb.fmt_offset + 12, sf_h);
                    xbb.bit_depth = read_u16le(xbb.fmt_offset + 14, sf_h);
                    xbb.out_bps_pch = read_u16le(xbb.fmt_offset + 16, sf_h);
                    xbb.out_blkalgn_pch = read_u16le(xbb.fmt_offset + 18, sf_h);
                    /* do some checks against existing fmt values */
                    if (xbb.blkalgn != (0x24 * xbb.channels)) goto fail;
                    if (xbb.bit_depth != 4) goto fail;
                    if (xbb.out_bps_pch != 2) goto fail;
                    if (xbb.out_blkalgn_pch != 0x40) goto fail;
                } else {
                    goto fail;
                }
            }
            /* check if "smpl" chunk exists, unlike also not used by the game files themselves. */
            if ((xbb.xbb_flags & xbb_has_smpl_chunk)) {
                // (todo) implement existing details of "smpl" chunk from the game exe itself.
            }
            break;
        }
    }

    /* check if "data" chunk exists, unlikely. */
    if (!(xbb.xbb_flags & xbb_has_data_chunk)) goto fail;
    if (i + 1 == target_subsong) {
        /* read all "data" info there is. */
        if (xbb.xbb_flags & xbb_has_external_data) {
            /* if some sound has external data, read offset and size values from internal XBB entry info */
            xbb.external_data_offset = read_u32le(xbb.data_offset + 0, sf_h);
            xbb.external_data_size = read_u32le(xbb.data_offset + 4, sf_h);
            xbb.stream_offset = xbb.external_data_offset;
            xbb.stream_size = xbb.external_data_size;
            /* open external .xsb file */
            sf_b = open_streamfile_by_ext(sf, "xsb");
            if (!sf_b) goto fail;
            sf_data = sf_b;
        }
        else {
            /* if some sound has internal data, get offset and size values from other vars */
            xbb.stream_offset = xbb.data_offset;
            xbb.stream_size = xbb.data_size;
            sf_data = sf_h;
        }
    }

    /* build and allocate the vgmstream */
    vgmstream = allocate_vgmstream(xbb.channels, 0);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XBB_XSB;
    vgmstream->sample_rate = xbb.sample_rate;
    vgmstream->num_streams = xbb.entries;
    vgmstream->stream_size = xbb.stream_size;

    switch (xbb.codec) {
        case 0x0069:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = xbox_ima_bytes_to_samples(xbb.stream_size,xbb.channels);
            break;
    }

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,sf_data,xbb.stream_offset))
        goto fail;
    if (sf_h != sf) close_streamfile(sf_h);
    if (sf_b != sf) close_streamfile(sf_b);
    return vgmstream;
fail:
    if (sf_h != sf) close_streamfile(sf_h);
    if (sf_b != sf) close_streamfile(sf_b);
    close_vgmstream(vgmstream);
    return NULL;
}
