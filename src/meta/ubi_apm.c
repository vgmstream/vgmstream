#include "meta.h"
#include "../coding/coding.h"

/* .APM - seen in old Ubisoft games [Rayman 2: The Great Escape (PC), Donald Duck: Goin' Quackers (PC)] */
VGMSTREAM* init_vgmstream_ubi_apm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t channels, sample_rate, file_size, nibble_size;
    off_t start_offset;
    int loop_flag;
    uint32_t i;

    if (read_u16le(0x00, sf) != 0x2000 || !is_id32be(0x14, sf, "vs12"))
        goto fail;

    if (!check_extensions(sf, "apm"))
        goto fail;

    /* (info from https://github.com/Synthesis/ray2get)
     * 0x00(2): format tag (0x2000 for Ubisoft ADPCM)
     * 0x02(2): channels
     * 0x04(4): sample rate
     * 0x08(4): byte rate? PCM samples?
     * 0x0C(2): block align
     * 0x0E(2): bits per sample
     * 0x10(4): header size
     * 0x14(4): "vs12"
     * 0x18(4): file size
     * 0x1C(4): nibble size
     * 0x20(4): -1?
     * 0x24(4): 0?
     * 0x28(4): high/low nibble flag (when loaded in memory)
     * 0x2C(N): ADPCM info per channel, last to first
     * - 0x00(4): ADPCM hist
     * - 0x04(4): ADPCM step index
     * - 0x08(4): copy of ADPCM data (after interleave, ex. R from data + 0x01)
     * 0x60(4): "DATA"
     * 0x64(N): ADPCM data
     */

    channels = read_u16le(0x02, sf);
    sample_rate = read_u32le(0x04, sf);
    file_size = read_u32le(0x18, sf);
    nibble_size = read_u32le(0x1c, sf);

    start_offset = 0x64;

    if (file_size != get_streamfile_size(sf))
        goto fail;

    if (nibble_size > (file_size - start_offset))
        goto fail;

    if (!is_id32be(0x60, sf, "DATA"))
        goto fail;

    loop_flag = 0;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_APM;
    vgmstream->coding_type = coding_DVI_IMA_mono;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x01;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = ima_bytes_to_samples(file_size - start_offset, channels);

    /* read initial hist (last to first) */
    for (i = 0; i < channels; i++) {
        vgmstream->ch[i].adpcm_history1_32 = read_s32le(0x2c + 0x0c * (channels - 1 - i) + 0x00, sf);
        vgmstream->ch[i].adpcm_step_index = read_s32le(0x2c + 0x0c * (channels - 1 - i) + 0x04, sf);
    }
    //todo supposedly APM IMA removes lower 3b after assigning step, but wave looks a bit off (Rayman 2 only?):
    // ...; step = adpcm_table[step_index]; delta = (step >> 3); step &= (~7); ...

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
