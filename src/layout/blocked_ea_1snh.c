#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"
#include "../util/endianness.h"

// flags for vgmstream
#define EA_FLAGS_BLOCK_ADPCM    0x01    // blocks have extra ADPCM config

/* set up for the block at the given offset */
void block_update_ea_1snh(off_t block_offset, VGMSTREAM* vgmstream) {
    STREAMFILE* sf = vgmstream->ch[0].streamfile;
    read_s32_t read_s32 = vgmstream->codec_endian ? read_s32be : read_s32le;


    /* EOF reads: signal we have nothing and let the layout fail */
    if (block_offset >= get_streamfile_size(sf)) {
        vgmstream->current_block_offset = block_offset;
        vgmstream->next_block_offset = block_offset;
        vgmstream->current_block_samples = -1;
        return;
    }

    uint32_t block_type = read_u32be(block_offset + 0x00, sf);
    uint32_t block_size = 0, block_header = 0;

    /* BE in SAT, but one file may have both BE and LE chunks [FIFA 98 (SAT): movie LE, audio BE] */
    if (guess_endian32(block_offset + 0x04, sf))
        block_size = read_u32be(block_offset + 0x04, sf);
    else
        block_size = read_u32le(block_offset + 0x04, sf);

    block_header = 0x00;

    if (block_type == 0x00000000 || block_type == 0xFFFFFFFF || block_type == get_id32be("1SNe")) { /* EOF */
        vgmstream->current_block_samples = -1;
        return;
    }

    // audio header 
    if (block_type == get_id32be("1SNh") || block_type == get_id32be("SEAD")) {
        bool is_sead = (block_type == get_id32be("SEAD"));
        bool is_eacs = is_id32be(block_offset + 0x08, sf, "EACS");
        bool is_zero = read_u32be(block_offset + 0x08, sf) == 0x00;

        block_header = (is_eacs || is_zero) ? 0x28 : (is_sead ? 0x14 : 0x2c);
        if (block_header >= block_size) /* sometimes has audio data after header */
            block_header = 0x00;
    }
    else if (block_type == get_id32be("1SNd") || block_type == get_id32be("SNDC")) {
        block_header = 0x08;
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset    = block_offset + block_size;

    // no audio data, skip this block
    if (block_header == 0x00) {
        vgmstream->current_block_samples = 0;
        return;
    }

    // ???
    if (block_size <= block_header) {
        vgmstream->current_block_samples = -1;
        return;
    }

    size_t audio_size = block_size - block_header;
    int channels = vgmstream->channels;

    /* set new channel offsets and block sizes */
    switch(vgmstream->coding_type) {
        case coding_PCM8_int:
        case coding_ULAW_int:
            vgmstream->current_block_samples = pcm8_bytes_to_samples(audio_size, channels);
            for (int i = 0; i < channels; i++) {
                vgmstream->ch[i].offset = block_offset + block_header + i;
            }
            break;

        case coding_PCM16_int:
            vgmstream->current_block_samples = pcm16_bytes_to_samples(audio_size, channels);
            for (int i = 0; i < channels; i++) {
                vgmstream->ch[i].offset = block_offset + block_header + (i*2);
            }
            break;

        case coding_PSX:
            if (vgmstream->layout_config & EA_FLAGS_BLOCK_ADPCM)  {/* extra field */
                block_header += 0x04;
                audio_size -= 0x04;
            }

            vgmstream->current_block_samples = ps_bytes_to_samples(audio_size, channels);
            for (int i = 0; i < channels; i++) {
                vgmstream->ch[i].offset = block_offset + block_header + i * (audio_size / channels);
            }
            break;

        case coding_DVI_IMA:
            if (vgmstream->layout_config & EA_FLAGS_BLOCK_ADPCM) { /* ADPCM hist */
                vgmstream->current_block_samples = read_s32(block_offset + block_header, sf);

                for(int i = 0; i < channels; i++) {
                    off_t adpcm_offset = block_offset + block_header + 0x04;
                    vgmstream->ch[i].adpcm_step_index  = read_s32(adpcm_offset + i * 0x04 + 0x00 * channels, sf);
                    vgmstream->ch[i].adpcm_history1_32 = read_s32(adpcm_offset + i * 0x04 + 0x04 * channels, sf);
                    vgmstream->ch[i].offset = adpcm_offset + 0x08*channels;
                }

                //VGM_ASSERT(vgmstream->current_block_samples != (block_size - block_header - 0x04 - 0x08*channels) * 2 / channels,
                //           "EA 1SHN blocked: different expected vs block num samples at %lx\n", block_offset);
            }
            else {
                vgmstream->current_block_samples = ima_bytes_to_samples(audio_size, channels);
                for(int i = 0; i < channels; i++) {
                    vgmstream->ch[i].offset = block_offset + block_header;
                }
            }
            break;

        default:
            break;
    }

}
