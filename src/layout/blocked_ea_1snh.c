#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_ea_1snh(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    size_t block_size = 0, block_header = 0;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_32bitBE : read_32bitLE;
    size_t file_size = get_streamfile_size(streamFile);


    /* EOF reads: signal we have nothing and let the layout fail */
    if (block_offset >= file_size) {
        vgmstream->current_block_offset = block_offset;
        vgmstream->next_block_offset = block_offset;
        vgmstream->current_block_samples = -1;
        return;
    }


    while (block_offset < file_size) {
        uint32_t id = read_32bitBE(block_offset+0x00,streamFile);

        /* BE in SAT, but one file may have both BE and LE chunks [FIFA 98 (SAT): movie LE, audio BE] */
        if (guess_endianness32bit(block_offset+0x04,streamFile))
            block_size = read_32bitBE(block_offset+0x04,streamFile);
        else
            block_size = read_32bitLE(block_offset+0x04,streamFile);

        block_header = 0;

        if (id == 0x31534E68 || id == 0x53454144) {  /* "1SNh" "SEAD" audio header */
            int is_sead = (id == 0x53454144);
            int is_eacs = read_32bitBE(block_offset+0x08, streamFile) == 0x45414353;

            block_header = is_eacs ? 0x28 : (is_sead ? 0x14 : 0x2c);
            if (block_header >= block_size) /* sometimes has audio data after header */
                block_header = 0;
        }
        else if (id == 0x31534E64 || id == 0x534E4443) {  /* "1SNd" "SNDC" audio data */
            block_header = 0x08;
        }
        else if (id == 0x00000000 || id == 0xFFFFFFFF || id == 0x31534E65) { /* EOF or "1SNe" */
            vgmstream->current_block_samples = -1;
            break;
        }

        if (block_header) {
            break;
        }

        block_offset += block_size;
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset    = block_offset + block_size;
    vgmstream->current_block_size   = block_size - block_header;
    if (vgmstream->current_block_samples == -1)
        return;


    /* set new channel offsets and block sizes */
    switch(vgmstream->coding_type) {
        case coding_PCM8_int:
        case coding_ULAW_int:
            vgmstream->current_block_size /= vgmstream->channels;
            for (i=0;i<vgmstream->channels;i++) {
                vgmstream->ch[i].offset = block_offset + block_header + i;
            }
            break;

        case coding_PCM16_int:
            vgmstream->current_block_size /= vgmstream->channels;
            for (i=0;i<vgmstream->channels;i++) {
                vgmstream->ch[i].offset = block_offset + block_header + (i*2);
            }
            break;

        case coding_PSX:
            vgmstream->current_block_size /= vgmstream->channels;
            for (i=0;i<vgmstream->channels;i++) {
                vgmstream->ch[i].offset = block_offset + block_header + i*vgmstream->current_block_size;
            }
            break;

        case coding_DVI_IMA:
            if (vgmstream->codec_config == 1) { /* ADPCM hist */
                vgmstream->current_block_samples = read_32bit(block_offset + block_header, streamFile);
                vgmstream->current_block_size = 0; // - (0x04 + 0x08*vgmstream->channels); /* should be equivalent */

                for(i = 0; i < vgmstream->channels; i++) {
                    off_t adpcm_offset = block_offset + block_header + 0x04;
                    vgmstream->ch[i].adpcm_step_index  = read_32bit(adpcm_offset + i*0x04 + 0x00*vgmstream->channels, streamFile);
                    vgmstream->ch[i].adpcm_history1_32 = read_32bit(adpcm_offset + i*0x04 + 0x04*vgmstream->channels, streamFile);
                    vgmstream->ch[i].offset = adpcm_offset + 0x08*vgmstream->channels;
                }

                //VGM_ASSERT(vgmstream->current_block_samples != (block_size - block_header - 0x04 - 0x08*vgmstream->channels) * 2 / vgmstream->channels,
                //           "EA 1SHN blocked: different expected vs block num samples at %lx\n", block_offset);
            }
            else {
                for(i = 0; i < vgmstream->channels; i++) {
                    vgmstream->ch[i].offset = block_offset + block_header;
                }
            }
            break;

        default:
            break;
    }

}
