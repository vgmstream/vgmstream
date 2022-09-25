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
    uint32_t xbb_whole_size; // size of XBB file in itself, minus 4.
    uint32_t entries; // how many sounds an XBB file can have.
    uint32_t entry_offset; // XBB sound entry info offset.

    /* RIFF chunks */
    uint32_t chunk_type; // RIFF chunk type.
    uint32_t chunk_size; // RIFF chunk size (if chunk type wills it into existence).
    uint32_t riff_offset; // "RIFF" offset pointer.
    uint32_t riff_size; // "RIFF" chunk size (in itself).
    uint32_t fmt_offset; // "fmt " offset pointer.
    uint32_t fmt_size; // "fmt " chunk size.
    uint32_t data_offset; // "data" offset pointer.
    uint32_t data_size; // "data" chunk size, doesn't always cover whole sound. in which case it's set to 0xfffffff8 alongside two fields (see below.)
    uint32_t smpl_offset; // "smpl" offset pointer.
    uint32_t smpl_size; // "smpl" chunk size.

    /* RIFF WAVE format header */
    uint16_t codec; // RIFF WAVE format tag, usually an audio codec of some sort.
    uint16_t channels; // total number of channels that this sound has.
    uint32_t sample_rate; // sample rate, in samples per second. most common ones are 8000, 11025, 22050 and 44100.
    uint32_t avg_bps; // Xbox IMA ADPCM average data transfer rate, in bytes per second.
    uint16_t blkalgn; // Xbox IMA ADPCM block alignment, set to either 0x24 or 0x48 depending on how mono (1 channel) or stereo (2 channels) an sound file can be.
    uint16_t bit_depth; // Xbox IMA ADPCM bit depth, set to 4.
    uint16_t extra_size; // extra size field for RIFF WAVE sound files using the Xbox IMA ADPCM codec.
    uint16_t blksmpl; // Xbox IMA ADPCM block samples.

    /* RIFF WAVE data header, in case some sound entry needs an XSB file to be played back in-game */
    uint32_t external_data_offset; // offset pointing to external sound data.
    uint32_t external_data_size; // size of external sound data.

    /* other stuff */
    uint32_t xbb_real_size; // size of XBB file in itself.
    uint8_t xbb_flags; // config var meant to tell which sound is which.
    uint32_t stream_offset; // offset of streamed sound entry, meant to be used into vgmstream.
    uint32_t stream_size; // size of streamed sound entry, meant to be used into vgmstream.
    uint32_t riff_size_from_zero; // counter meant to cover the entire sound entry based on info from XBB file.
    uint32_t is_riff; // if chunk is a "RIFF" one.
    uint32_t is_wave; // if chunk is a "WAVE" one.
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
                    xbb.extra_size = read_u16le(xbb.fmt_offset + 16, sf_h);
                    if (xbb.extra_size > 0) {
                        xbb.blksmpl = read_u16le(xbb.fmt_offset + 18, sf_h);
                    }
                    /* do some checks against existing fmt values */
                    if (xbb.blkalgn != (0x24 * xbb.channels)) goto fail; // Xbox IMA ADPCM frame size, in theory 
                    if (xbb.bit_depth != 4) goto fail; // Xbox IMA ADPCM bit depth
                    if (xbb.extra_size != 2) goto fail; // cbSize for Xbox IMA ADPCM
                    if (xbb.blksmpl != 0x40) goto fail; // Xbox IMA ADPCM output block samples
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
