#include "meta.h"
#include "../coding/coding.h"
#include "../util/spu_utils.h"


/* 2PFS - from Konami Games [Mahoromatic: Moetto-KiraKira Maid-San (PS2), GANTZ The Game (PS2)] */
VGMSTREAM* init_vgmstream_2pfs(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag, channels, version, interleave, sample_rate;
    uint32_t stream_size, stream_offset, loop_start_block, loop_end_block;
    int loop_start_adjust, loop_end_adjust; // loops start/end a few samples into the start/end block
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00,sf, "2PFS"))
        return NULL;
    /* .sap: music
     * .iap: bank (sfx/voices) */
    if (!check_extensions(sf, "sap,iap"))
        return NULL;

    version = read_u16le(0x04,sf);
    if (version != 0x01 && version != 0x02) // v1: Mahoromatic, v2: Gantz, Meine Liebe
        return NULL;
    // 06: unknown, v1=0x0004 v2=0x0001
    // 08: file id
    // 0c: base header size (v1=0x50, v2=0x60) + datasize (without the 0x800 full header size)
    // 10-30: mostly unknown (v1 differs from v2)
    // music:
    // 30: garbage
    // 34: data size
    // 38: header size (up to 0x800 with padding)
    // 3c: stream count?
    // 40: music header
    // bank:
    // 30: garbage
    // 34: bank table size (values at 0x60)
    // 38: bank header size (values at 0x50)
    // 3c: garbage
    // 40: garbage
    // 44: data size
    // 48: header size (bank table + header size)
    // 4c: garbage
    // 50: file id again?
    // 54: subsongs
    // 58: 127 (volume?)
    // 5c: null
    // 60: bank music headers

    int type = read_u8(0x10,sf); // there may be other ways but seems safe enough
    switch(type) {
        case 0x01:      // music
            stream_size     = read_u32le(0x34,sf);
            stream_offset   = read_u32le(0x38,sf) + 0x40;
            total_subsongs  = read_s32le(0x3c,sf);
            if (total_subsongs != 1)
                return NULL;
            if (!check_subsongs(&target_subsong, total_subsongs))
                return NULL;

            // music header
            channels        = read_u8(0x40,sf);
            loop_flag       = read_u8(0x41,sf);

            if (version == 0x01) {
                loop_start_adjust   = read_u16le(0x42,sf);
                sample_rate         = read_u32le(0x44,sf);
                loop_start_block    = read_u32le(0x48,sf);
                loop_end_block      = read_u32le(0x4c,sf);
            }
            else {
                // 42: null
                loop_start_adjust   = read_u32le(0x44,sf);
                sample_rate         = read_u32le(0x48,sf);
                loop_start_block    = read_u32le(0x50,sf);
                loop_end_block      = read_u32le(0x54,sf);
                // 58: null
                // 5c: null
            }

            interleave = 0x1000;
            loop_end_adjust = interleave; // loops end after all samples in the end_block AFAIK
            break;

        case 0x02:      // se
        case 0x03: {    // vo
            stream_offset   = read_u32le(0x48,sf) + 0x50; //data start

            total_subsongs  = read_s32le(0x54,sf);
            if (!check_subsongs(&target_subsong, total_subsongs))
                return NULL;

            uint32_t offset = 0x60 + (target_subsong - 1) * 0x20;
            channels        = read_u8   (offset + 0x00,sf);
            // 01: config?
            // 02: config?
            // 04: pitch related?
            sample_rate     = read_u32le(offset + 0x08,sf);
            stream_offset   = read_u32le(offset + 0x0c,sf) + stream_offset;
            stream_size     = read_u32le(offset + 0x10,sf);
            // 14: num samples?
            // 18: loop start? (not seen)
            // 1c: loop end? (not seen)

            if (channels != 1)
                return NULL;

            sample_rate = spu2_pitch_to_sample_rate(sample_rate);

            loop_flag = 0;
            loop_start_block = 0;
            loop_end_block = 0;
            loop_start_adjust = 0;
            loop_end_adjust = 0;

            interleave = 0x1000; // not seen

            // stream offsets may repeat data and aren't sorted
            break;
        }

        default:
            return NULL;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_2PFS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ps_bytes_to_samples(stream_size, channels);
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->coding_type = coding_PSX;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;

    if (loop_flag) {
        // block to offset > offset to sample + adjust (number of frames into the block)
        vgmstream->loop_start_sample = ps_bytes_to_samples(loop_start_block * channels * interleave, channels)
                + ps_bytes_to_samples(loop_start_adjust * channels, channels);
        vgmstream->loop_end_sample = ps_bytes_to_samples(loop_end_block * channels * interleave, channels)
                + ps_bytes_to_samples(loop_end_adjust * channels, channels);
    }

    if (!vgmstream_open_stream(vgmstream, sf, stream_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
