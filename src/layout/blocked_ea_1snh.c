#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void eacs_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
    off_t block_size=vgmstream->current_block_size;

    if(read_32bitBE(block_offset,vgmstream->ch[0].streamfile)==0x31534E6C) {
        block_offset+=0x0C;
    }

    vgmstream->current_block_offset = block_offset;

    if(read_32bitBE(block_offset,vgmstream->ch[0].streamfile)==0x31534E64) { /* 1Snd */
        block_offset+=4;
        if(vgmstream->ea_platform==0)
            block_size=read_32bitLE(vgmstream->current_block_offset+0x04,
                                    vgmstream->ch[0].streamfile);
        else
            block_size=read_32bitBE(vgmstream->current_block_offset+0x04,
                                    vgmstream->ch[0].streamfile);
        block_offset+=4;
    }

    vgmstream->current_block_size=block_size-8;

    if(vgmstream->coding_type==coding_DVI_IMA) {
        vgmstream->current_block_size=read_32bitLE(block_offset,vgmstream->ch[0].streamfile);

        for(i=0;i<vgmstream->channels;i++) {
            vgmstream->ch[i].adpcm_step_index = read_32bitLE(block_offset+0x04+i*4,vgmstream->ch[0].streamfile);
            vgmstream->ch[i].adpcm_history1_32 = read_32bitLE(block_offset+0x04+i*4+(4*vgmstream->channels),vgmstream->ch[0].streamfile);
            vgmstream->ch[i].offset = block_offset+0x14;
        }
    } else {
        if(vgmstream->coding_type==coding_PSX) {
            for (i=0;i<vgmstream->channels;i++)
                vgmstream->ch[i].offset = vgmstream->current_block_offset+8+(i*(vgmstream->current_block_size/2));
        } else {

            for (i=0;i<vgmstream->channels;i++) {
                if(vgmstream->coding_type==coding_PCM16_int)
                    vgmstream->ch[i].offset = block_offset+(i*2);
                else
                    vgmstream->ch[i].offset = block_offset+i;
            }
        }
        vgmstream->current_block_size/=vgmstream->channels;
    }
    vgmstream->next_block_offset = vgmstream->current_block_offset +
        (off_t)block_size;
}
