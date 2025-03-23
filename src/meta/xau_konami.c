#include "meta.h"
#include "../coding/coding.h"

/* SFXB - from Konami games [Yu-Gi-Oh: The Dawn of Destiny (Xbox)] */
VGMSTREAM* init_vgmstream_sfxb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag, channels, sample_rate;


    /* checks */
    if (!is_id32be(0x00,sf, "SFXB"))
        return NULL;
    if (!check_extensions(sf,"xau"))
        return NULL;
    // 04: version? (2)
    // 08: file id
    // 0c: file size
    // 10: possibly chunk definitions (all files have 4)
    //   00: type (00030200=subsong info, 00030201=headers, 00030202=data)
    //   04: size
    //   08: relative offset (after chunks = 0x50)
    //   0c: always 0x00000202

    uint32_t subs_offset = 0x50 + read_u32le(0x28,sf); //always 0x00
    uint32_t head_offset = 0x50 + read_u32le(0x38,sf); //always 0x10
    uint32_t data_offset = 0x50 + read_u32le(0x48,sf); //varies

    // subsong chunk
    // 00: file id again
    // 04: subsongs
    // 08: always 0x7F
    // 0c: null
    int target_subsong = sf->stream_index;
    int total_subsongs = read_u32le(subs_offset + 0x04, sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > total_subsongs)
        return NULL;

    uint32_t entry_offset = head_offset + 0x60 * (target_subsong - 1);
    // 00: type
    // 04: flags?
    // 08: flags? (always 0x00020000)
    // 0c: offset
    // 10: size
    // 14: null
    // 18: loop start
    // 1c: loop end
    // 20: RIFF + "XWV" + "fmt" + "loop" (same as loop start/end) + "data" (same as size)

    uint32_t stream_type    = read_u32le(entry_offset + 0x00, sf);
    uint32_t stream_offset  = read_u32le(entry_offset + 0x0c, sf) + data_offset;
    uint32_t stream_size    = read_u32le(entry_offset + 0x10, sf);
    uint32_t loop_start     = read_u32le(entry_offset + 0x18, sf);
    uint32_t loop_end       = read_u32le(entry_offset + 0x1c, sf) + loop_start;
    if (!is_id32be(entry_offset + 0x20,sf, "RIFF"))
        return NULL;
    if (read_u16le(entry_offset + 0x34,sf) != 0x01) /* codec */
        return NULL;
    channels    = read_u16le(entry_offset + 0x36, sf);
    sample_rate = read_u32le(entry_offset + 0x38, sf);

    loop_flag = (loop_end > 0);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_SFXB;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    switch(stream_type) {
        case 0x0001:
            vgmstream->coding_type = coding_SILENCE;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = sample_rate;

            // stream info is repeat of first subsong
            break;

        case 0x0F01:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm16_bytes_to_samples(stream_size, channels);
            vgmstream->loop_start_sample = pcm16_bytes_to_samples(loop_start, channels);
            vgmstream->loop_end_sample = pcm16_bytes_to_samples(loop_end, channels);
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
