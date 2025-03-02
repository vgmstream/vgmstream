#include "meta.h"
#include "../coding/coding.h"
#include "lp_ap_lep_streamfile.h"

/* LP/AP/LEP - from Konami (KCES)'s Enthusia: Professional Racing (PS2) */
VGMSTREAM* init_vgmstream_lp_ap_lep(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate, interleave;
    uint32_t data_size, loop_start, loop_end;
    uint32_t id;


    /* checks */
    if (!is_id32be(0x00,sf,"LP  ") && !is_id32be(0x00,sf,"AP  ") && !is_id32be(0x00,sf,"LEP "))
        return NULL;

    /* .bin/lbin: internal (no names in bigfiles but exes mention "bgm%05d.bin" and "LEP data")
     * .lp/lep/ap: header ID */
    if (!check_extensions(sf, "bin,lbin,lp,lep,ap"))
        return NULL;

    id = read_u32be(0x00,sf);
    switch (id) {
        case 0x41502020: /* "AP  " */
        case 0x4C502020: /* "LP  " */
            data_size   = read_u32le(0x04,sf); // end offset after header
            sample_rate = read_u32le(0x08,sf);
            interleave  = read_u32le(0x0c,sf);
            // 10: pan/volume?
            loop_start  = read_u32le(0x14,sf); // absolute
            loop_end    = read_u32le(0x18,sf); // end offset after header
            start_offset= read_u32le(0x1C,sf); // after header

            // tweak values considering (applies to both PS-ADPCM and PCM):
            // - start_offset with PCM starts 0x20 before data, so must be after header (not a blank frame)
            // - end offset after 0x20 usually ends when padding (0xFF) starts
            // - loop end is usually the same except in PCM
            // - loop start is always aligned to 0x800 (meaning no loop/0 uses 0x800 w/ start offset 0x7E0 + 0x20)
            // * in PS-ADPCM start after header points to a regular frame, a bit odd since PS-ADPCM should start with blank frames
            //   but correct as LEP starts at 0x800 without them
            start_offset += 0x20;
            data_size += 0x20;
            loop_end += 0x20;
            data_size -= start_offset;
            loop_end -= start_offset;
            loop_start -= start_offset;
            break;

        case 0x4C455020: /* "LEP " (memory data?) */
            // 04: config?
            data_size   = read_u32le(0x08,sf); // within stream
            // 10: pan/volume?
            sample_rate = read_u16le(0x12,sf);
            loop_start  = read_u32le(0x58,sf); // within stream?
            // 5c: loop start?
            // 60+: related to loop?

            // not sure if loops are absolute, but since values are not 0x800-aligned like AP/LP assuming it's not

            loop_end = data_size;
            interleave  = 0x10;
            start_offset = 0x800;
            break;

        default:
            return NULL;
    }

    loop_flag = loop_start != 0;
    channels = 2;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_LP_AP_LEP;
    vgmstream->sample_rate = sample_rate;

    switch (id) {
        case 0x4C502020: /* "LP  " */
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = pcm16_bytes_to_samples(data_size, channels);
            vgmstream->loop_start_sample = pcm16_bytes_to_samples(loop_start, channels);
            vgmstream->loop_end_sample = pcm16_bytes_to_samples(loop_end, channels);

            temp_sf = setup_lp_streamfile(sf, start_offset); /* encrypted/obfuscated PCM */
            if (!temp_sf) goto fail;
            break;

        case 0x41502020: /* "AP  " */
        case 0x4C455020: /* "LEP " */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = interleave;

            vgmstream->num_samples = ps_bytes_to_samples(data_size, channels);
            vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start, channels);
            vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end, channels);
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, temp_sf ? temp_sf : sf, start_offset))
        goto fail;
    close_streamfile(temp_sf);
    return vgmstream;
fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
