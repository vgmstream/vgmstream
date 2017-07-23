#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void ea_schl_block_update(off_t block_offset, VGMSTREAM * vgmstream) {
    int i;
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    uint32_t id;
    size_t file_size, block_size = 0, block_samples;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_32bitBE : read_32bitLE;
    //int16_t (*read_16bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_16bitBE : read_16bitLE;


    /* find target block ID and skip the rest */
    file_size = get_streamfile_size(streamFile);
    while (block_offset < file_size) {
        id = read_32bitBE(block_offset+0x00,streamFile);

        block_size = read_32bitLE(block_offset+0x04,streamFile);
        if (block_size > 0x00F00000) /* size is always LE, except in early SS/MAC */
            block_size = read_32bitBE(block_offset+0x04,streamFile);

        /* SCxx blocks have size in the header, but others may not. To simplify we just try to find
         * a SCDl (main data) every 0x04. EA sometimes concats many small files, so after a SCEl (end block)
         * there may be a new SCHl + SCDl too, so this pretends they are a single stream. */
        if (id == 0x5343446C) {  /* "SCDl" data block found */

            /* use num_samples from header if possible; don't calc as data may have padding (ex. PCM8) or not possible (ex. MP3) */
            switch(vgmstream->coding_type) {
                case coding_PSX:
                    block_samples = ps_bytes_to_samples(block_size-0x10, vgmstream->channels);
                    break;

                default:
                    block_samples = read_32bit(block_offset+0x08,streamFile);
                    break;
            }

            /* guard against false positives (happens in "pIQT" blocks) */
            if (block_size > 0xFFFF || block_samples > 0xFFFF) { /* observed max is ~0xf00 but who knows */
                block_offset += 0x04;
                continue;
            }

            break;
        }
        else {
            /* movie "pIQT" may be bigger than what block_size says, but seems to help */
            if (id == 0x5343486C || id == 0x5343436C || id == 0x53434C6C || id == 0x70495154) { /* "SCHl" "SCCl" "SCLl" "SCEl" "pIQT" */
                block_offset += block_size;
            } else {
                block_offset += 0x04;
            }

            if (id == 0x5343456C) { /* "SCEl" end block found */
                block_offset += (block_offset % 0x04) == 0 ? 0 : 0x04 - (block_offset % 0x04); /* 32b-aligned, important */
                /* Usually there is padding between SCEl and SCHl too (aligned to 0x80) */
            }

            continue;
        }
    }

    /* EOF reads: pretend we have samples to please the layout (unsure if this helps) */
    if (block_offset >= file_size) {
        vgmstream->current_block_offset = block_offset;
        vgmstream->next_block_offset = block_offset + 0x04;
        vgmstream->current_block_samples = vgmstream->num_samples;
        return;
    }


    /* set new channel offsets and ADPCM history */
    /* ADPCM hist could be considered part of the stream/decoder (some EAXA decoders call it "EAXA R1" when it has hist), and BNKs
     * (with no blocks) may also have them in the first offset, but also may not. To simplify we just read them here. */
    switch(vgmstream->coding_type) {
        /* id, size, unk1, unk2, interleaved data */
        case coding_PSX:
            for (i = 0; i < vgmstream->channels; i++) {
                size_t interleave = (block_size-0x10) / vgmstream->channels;
                vgmstream->ch[i].offset = block_offset + 0x10 + i*interleave;
            }
            /* 0x08/0x0c: unknown (doesn't look like hist or offsets, as 1ch files has them too) */

            break;

        /* id, size, IMA hist, stereo/mono data */
        case coding_EACS_IMA:
            for(i = 0; i < vgmstream->channels; i++) {
                off_t header_offset = block_offset + 0xc + i*4;
                vgmstream->ch[i].adpcm_history1_32 = read_16bitLE(header_offset+0x00, vgmstream->ch[i].streamfile);
                vgmstream->ch[i].adpcm_step_index  = read_16bitLE(header_offset+0x02, vgmstream->ch[i].streamfile);
                vgmstream->ch[i].offset = block_offset + 0xc + (4*vgmstream->channels);
            }

            break;

        /* id, size, samples, hists-per-channel, stereo/interleaved data */
        case coding_EA_XA:
        case coding_EA_XA_int:
            for (i = 0; i < vgmstream->channels; i++) {
                int is_interleaved = vgmstream->coding_type == coding_EA_XA_int;
                size_t interleave;

                /* read ADPCM history from all channels before data (not actually read in sx.exe) */
                //vgmstream->ch[i].adpcm_history1_32 = read_16bit(block_offset + 0x0C + (i*0x04) + 0x00,streamFile);
                //vgmstream->ch[i].adpcm_history2_32 = read_16bit(block_offset + 0x0C + (i*0x04) + 0x02,streamFile);

                /* the block can have padding so find the channel size from num_samples */
                interleave = is_interleaved ? (block_samples / 28 * 0x0f) : 0;
                vgmstream->ch[i].offset = block_offset + 0x0c + vgmstream->channels*0x04 + i*interleave;
            }

            break;

        /* id, size, samples, offsets-per-channel, interleaved data (w/ optional hist per channel) */
        default:
            for (i = 0; i < vgmstream->channels; i++) {
                off_t channel_start = read_32bit(block_offset + 0x0C + (0x04*i),streamFile);
                vgmstream->ch[i].offset = block_offset + 0x0C + (0x04*vgmstream->channels) + channel_start;
            }

            /* read ADPCM history before each channel if needed (not actually read in sx.exe) */
            if (vgmstream->codec_version == 1) {
                for (i = 0; i < vgmstream->channels; i++) {
                    //vgmstream->ch[i].adpcm_history1_32 = read_16bit(vgmstream->ch[i].offset+0x00,streamFile);
                    //vgmstream->ch[i].adpcm_history3_32 = read_16bit(vgmstream->ch[i].offset+0x02,streamFile);
                    vgmstream->ch[i].offset += 4;
                }
            }

            break;
    }

    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
    vgmstream->current_block_samples = block_samples;
    vgmstream->current_block_size = 0; /* uses current_block_samples instead */


    /* reset channel sub offset for codecs using it */
    if (vgmstream->coding_type == coding_EA_XA
            || vgmstream->coding_type == coding_EA_XA_int
            || vgmstream->coding_type == coding_EA_XA_V2) {
        for(i=0;i<vgmstream->channels;i++) {
            vgmstream->ch[i].channel_start_offset=0;
        }
    }
}

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

    if(vgmstream->coding_type==coding_EACS_IMA) {
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
                if(vgmstream->coding_type==coding_PCM16LE_int)
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
