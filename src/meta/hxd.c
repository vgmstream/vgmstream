#include "meta.h"
#include "../coding/coding.h"


/* HXD - from Tecmo games [Tokobot Plus (PS2), Fatal Frame 2/3 (PS2), Gallop Racer 2004 (PS2)] */
VGMSTREAM* init_vgmstream_hxd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sf_body = NULL;
    uint32_t stream_offset, header_size, stream_size, interleave, loop_start, loop_end;
    int channels, loop_flag, bank, sample_rate;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf, "\0DXH"))
        goto fail;

    /* .hxd: actual extension (filenames in companion files/exe) */
    if (!check_extensions(sf, "hxd"))
        goto fail;

    /* 0x04: version? (0x1000) */
    total_subsongs = read_u32le(0x08,sf);
    bank = read_u32le(0x0c,sf);
    header_size = read_u32le(0x10,sf);
    interleave = read_u32le(0x14,sf); /* 0 in banks */
    /* 0x18-1c: null */

    /* Reject incorrectly ripped files, as .hxd is the header and data is always separate.
     * Rips with header+data pasted were allowed before, but since bigfiles may store
     * data first then header, audio could play wrong for no apparent reason. */
    if (header_size != get_streamfile_size(sf))
        goto fail;


    /* .hxd has 2 modes, banks with N subsongs or bgm with N channels. In both cases
     * there is header info per stream, though bgm just repeats values. */
    if (bank) {
        channels = 1;
    }
    else {
        channels = total_subsongs; /* seen 1/2 */
        total_subsongs = 1;
    }
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    {
        uint32_t info_offset = 0x20 + (target_subsong - 1) * 0x1c;
        uint32_t flags;

        sample_rate = read_s32le(info_offset + 0x00,sf);
        stream_offset = read_u32le(info_offset + 0x04,sf);
        /* 0x08: pitch? (44100=0x0EB3, 32000=0x0AAA, 22050=0759, 16000=0x5505, etc) */
        /* 0x0a: volume? (usually ~0x64, up to ~0x7F) */
        /* 0x0c: config? (pan, etc?) */
        flags = read_u16le(info_offset + 0x10,sf);
        /* 0x12: ? (seen in FF2 XB) */
        loop_start = read_u32le(info_offset + 0x14,sf) * 0x20;
        loop_end = read_u32le(info_offset + 0x18,sf) * 0x20;

        /* flags:
         * - 0x20: loop flag
         * - 0x10: ? (seen in banks)
         * - 0x02: ? (common in streams, not always)
         * - 0x01: ? (sometimes in streams) */
        loop_flag = flags & 0x20;

        /* different games use different combos */
        if (bank) {
            sf_body = open_streamfile_by_ext(sf, "bd"); /* Gallop Racer */
            if (!sf_body) {
                sf_body = open_streamfile_by_ext(sf, "str"); /* just in case */
            }
            if (!sf_body) goto fail;
        }
        else {
            sf_body = open_streamfile_by_ext(sf, "str"); /* Fatal Frame 2/3, Gallop Racer */
            if (!sf_body) {
                sf_body = open_streamfile_by_ext(sf, "at3"); /* Tokobot Plus (still ADPCM) */
            }
            if (!sf_body) goto fail;
        }

        /* size is not in the header (probably just leaves it to PS-ADPCM's EOF markets) */
        if (bank && target_subsong < total_subsongs) {
            /* find next usable offset (sometimes offsets repeat) */ //TODO: meh
            int i;
            uint32_t next_offset = 0;
            for (i = target_subsong; i < total_subsongs; i++) {
                next_offset = read_u32le(0x20 + i * 0x1c + 0x04,sf);

                if (next_offset > stream_offset)
                    break;
            }
            if (i == total_subsongs)
                next_offset = get_streamfile_size(sf_body);
            if (!next_offset)
                goto fail;
                
            stream_size = next_offset - stream_offset;
        }
        else {
            stream_size = get_streamfile_size(sf_body) - stream_offset;
        }
    }

    /* Xbox versions of Tecmo games use RIFF (music) or WBND/.xwb (sfx) in the body. Probably a quick
     * hack since they reuse extensions like .pss for movies too, for now reject. */
    if (read_u32be(0x00, sf_body) != 0)
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_HXD;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);
    vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channels);
    vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end, channels);
    if (!vgmstream->loop_end_sample)
        vgmstream->loop_end_sample = vgmstream->num_samples;

    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (!vgmstream_open_stream(vgmstream, sf_body, stream_offset))
        goto fail;

    close_streamfile(sf_body);
    return vgmstream;

fail:
    close_streamfile(sf_body);
    close_vgmstream(vgmstream);
    return NULL;
}
