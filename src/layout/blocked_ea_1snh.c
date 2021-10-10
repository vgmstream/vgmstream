#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"
#include "../util/endianness.h"

/* set up for the block at the given offset */
void block_update_ea_1snh(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    int i;
    uint32_t block_id;
    size_t block_size = 0, block_header = 0, audio_size = 0;
    read_s32_t read_s32 = vgmstream->codec_endian ? read_s32be : read_s32le;


    /* EOF reads: signal we have nothing and let the layout fail */
    if (block_offset >= get_streamfile_size(sf)) {
        vgmstream->current_block_offset = block_offset;
        vgmstream->next_block_offset = block_offset;
        vgmstream->current_block_samples = -1;
        return;
    }

    block_id = read_u32be(block_offset + 0x00, sf);

    /* BE in SAT, but one file may have both BE and LE chunks [FIFA 98 (SAT): movie LE, audio BE] */
    if (guess_endian32(block_offset + 0x04, sf))
        block_size = read_u32be(block_offset + 0x04, sf);
    else
        block_size = read_u32le(block_offset + 0x04, sf);

    block_header = 0;

    if (block_id == get_id32be("1SNh") || block_id == get_id32be("SEAD")) {  /* audio header */
        int is_sead = (block_id == get_id32be("SEAD"));
        int is_eacs = is_id32be(block_offset + 0x08, sf, "EACS");
        int is_zero = read_u32be(block_offset + 0x08, sf) == 0x00;

        block_header = (is_eacs || is_zero) ? 0x28 : (is_sead ? 0x14 : 0x2c);
        if (block_header >= block_size) /* sometimes has audio data after header */
            block_header = 0;
    }
    else if (block_id == get_id32be("1SNd") || block_id == get_id32be("SNDC")) {
        block_header = 0x08;
    }
    else if (block_id == 0x00000000 || block_id == 0xFFFFFFFF || block_id == get_id32be("1SNe")) { /* EOF */
        vgmstream->current_block_samples = -1;
        return;
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset    = block_offset + block_size;
    if (block_header == 0) {
        /* no audio data, skip this block */
        vgmstream->current_block_samples = 0;
        return;
    }

    audio_size = block_size - block_header;

    /* set new channel offsets and block sizes */
    switch(vgmstream->coding_type) {
        case coding_PCM8_int:
        case coding_ULAW_int:
            vgmstream->current_block_samples = pcm_bytes_to_samples(audio_size, vgmstream->channels, 8);
            for (i=0;i<vgmstream->channels;i++) {
                vgmstream->ch[i].offset = block_offset + block_header + i;
            }
            break;

        case coding_PCM16_int:
            vgmstream->current_block_samples = pcm_bytes_to_samples(audio_size, vgmstream->channels, 16);
            for (i=0;i<vgmstream->channels;i++) {
                vgmstream->ch[i].offset = block_offset + block_header + (i*2);
            }
            break;

        case coding_PSX:
            if (vgmstream->codec_config == 1)  {/* extra field */
                block_header += 0x04;
                audio_size -= 0x04;
            }

            vgmstream->current_block_samples = ps_bytes_to_samples(audio_size, vgmstream->channels);
            for (i = 0; i < vgmstream->channels; i++) {
                vgmstream->ch[i].offset = block_offset + block_header + i*(audio_size/vgmstream->channels);
            }
            break;

        case coding_DVI_IMA:
            if (vgmstream->codec_config == 1) { /* ADPCM hist */
                vgmstream->current_block_samples = read_s32(block_offset + block_header, sf);

                for(i = 0; i < vgmstream->channels; i++) {
                    off_t adpcm_offset = block_offset + block_header + 0x04;
                    vgmstream->ch[i].adpcm_step_index  = read_s32(adpcm_offset + i*0x04 + 0x00*vgmstream->channels, sf);
                    vgmstream->ch[i].adpcm_history1_32 = read_s32(adpcm_offset + i*0x04 + 0x04*vgmstream->channels, sf);
                    vgmstream->ch[i].offset = adpcm_offset + 0x08*vgmstream->channels;
                }

                //VGM_ASSERT(vgmstream->current_block_samples != (block_size - block_header - 0x04 - 0x08*vgmstream->channels) * 2 / vgmstream->channels,
                //           "EA 1SHN blocked: different expected vs block num samples at %lx\n", block_offset);
            }
            else {
                vgmstream->current_block_samples = ima_bytes_to_samples(audio_size, vgmstream->channels);
                for(i = 0; i < vgmstream->channels; i++) {
                    vgmstream->ch[i].offset = block_offset + block_header;
                }
            }
            break;

        default:
            break;
    }

}
