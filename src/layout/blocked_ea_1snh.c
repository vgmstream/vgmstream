#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_ea_1snh(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    uint32_t id;
    size_t file_size, block_size = 0, block_header = 0;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_32bitBE : read_32bitLE;


    /* find target block ID and skip the rest */
    file_size = get_streamfile_size(streamFile);
    while (block_offset < file_size) {
        id = read_32bitBE(block_offset+0x00,streamFile);
        block_size = read_32bit(block_offset+0x04,streamFile); /* includes id/size */
        block_header = 0x0;

        if (id == 0x31534E68) {  /* "1SNh" header block found */
            block_header = read_32bitBE(block_offset+0x08, streamFile) == 0x45414353 ? 0x28 : 0x2c; /* "EACS" */
            if (block_header < block_size) /* sometimes has data */
                break;
        }

        if (id == 0x31534E64) {  /* "1SNd" data block found */
            block_header = 0x08;
            break;
        }

        if (id == 0x00000000 || id == 0xFFFFFFFF) { /* EOF: possible? */
            break;
        }

        /* any other blocks "1SNl" "1SNe" etc */ //todo parse movie blocks
        block_offset += block_size;
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset    = block_offset + block_size;
    vgmstream->current_block_size   = block_size - block_header;


    /* set new channel offsets and block sizes */
    switch(vgmstream->coding_type) {
        case coding_PCM8_int:
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
            vgmstream->current_block_size -= 0x14;
            for(i = 0; i < vgmstream->channels; i++) {
                off_t adpcm_offset = block_offset + block_header + 0x04;
                vgmstream->ch[i].adpcm_step_index  = read_32bit(adpcm_offset + i*0x04, streamFile);
                vgmstream->ch[i].adpcm_history1_32 = read_32bit(adpcm_offset + 0x04*vgmstream->channels + i*0x04, streamFile);
                // todo some demuxed vids don't have ADPCM hist? not sure how to correctly detect
                vgmstream->ch[i].offset = block_offset + block_header + 0x14;
            }
            break;

        default:
            break;
    }

}
