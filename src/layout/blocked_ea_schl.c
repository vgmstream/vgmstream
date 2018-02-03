#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* set up for the block at the given offset */
void block_update_ea_schl(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    int new_schl = 0;
    size_t block_size = 0, block_samples = 0;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_32bitBE : read_32bitLE;
    size_t file_size = get_streamfile_size(streamFile);


    while (block_offset < file_size) {
        uint32_t id = read_32bitBE(block_offset+0x00,streamFile);

        block_size = read_32bitLE(block_offset+0x04,streamFile);
        if (block_size > 0x00F00000) /* size is always LE, except in early SAT/MAC */
            block_size = read_32bitBE(block_offset+0x04,streamFile);

        block_samples = 0;

        if (id == 0x5343446C || id == 0x5344454E) { /* "SCDl" "SDEN" audio data */
            switch(vgmstream->coding_type) {
                case coding_PSX:
                    block_samples = ps_bytes_to_samples(block_size-0x10, vgmstream->channels);
                    break;
                default:
                    block_samples = read_32bit(block_offset+0x08,streamFile);
                    break;
            }
        }
        else { /* any other chunk, audio ("SCHl" "SCCl" "SCLl" "SCEl" etc), or video ("pQGT" "pIQT "MADk" etc) */
            /* padding between "SCEl" and next "SCHl" (when subfiles exist) */
            if (id == 0x00000000) {
                block_size = 0x04;
            }

            if (id == 0x5343486C || id == 0x5348454E) { /* "SCHl" "SHEN" end block */
                new_schl = 1;
            }
        }

        /* guard against errors (happens in bad rips/endianness, observed max is vid ~0x20000) */
        if (block_size == 0x00 || block_size > 0xFFFFF || block_samples > 0xFFFF) {
            block_size = 0x04;
            block_samples = 0;
        }


        if (block_samples) /* audio found */
            break;
        block_offset += block_size;

        /* "SCEl" are aligned to 0x80 usually, but causes problems if not 32b-aligned (ex. Need for Speed 2 PC) */
        if ((id == 0x5343456C || id == 0x5345454E) && block_offset % 0x04) {
            block_offset += 0x04 - (block_offset % 0x04);
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
        case coding_DVI_IMA:
            for(i = 0; i < vgmstream->channels; i++) {
                off_t header_offset = block_offset + 0xc + i*4;
                vgmstream->ch[i].adpcm_history1_32 = read_16bitLE(header_offset+0x00, vgmstream->ch[i].streamfile);
                vgmstream->ch[i].adpcm_step_index  = read_16bitLE(header_offset+0x02, vgmstream->ch[i].streamfile);
                vgmstream->ch[i].offset = block_offset + 0xc + (4*vgmstream->channels);
            }

            break;

        /* id, size, samples, hists-per-channel, stereo/interleaved data */
        case coding_EA_XA:
      //case coding_EA_XA_V2: /* handled in default */
        case coding_EA_XA_int:
            for (i = 0; i < vgmstream->channels; i++) {
                int is_interleaved = vgmstream->coding_type == coding_EA_XA_int;
                size_t interleave;

                /* read ADPCM history from all channels before data (not actually read in sx.exe) */
                //vgmstream->ch[i].adpcm_history1_32 = read_16bit(block_offset + 0x0C + (i*0x04) + 0x00,streamFile);
                //vgmstream->ch[i].adpcm_history2_32 = read_16bit(block_offset + 0x0C + (i*0x04) + 0x02,streamFile);

                /* the block can have padding so find the channel size from num_samples */
                interleave = is_interleaved ? (block_samples / 28 * 0x0f) : 0;

                /* NOT channels*0x04, as seen in Superbike 2000 (PC) EA-XA v1 mono vids */
                vgmstream->ch[i].offset = block_offset + 0x0c + 2*0x04 + i*interleave;
            }

            break;

        /* id, size, samples, offsets-per-channel, flag (0x01 = data start), data */
        case coding_EA_MT:
            for (i = 0; i < vgmstream->channels; i++) {
                off_t channel_start = read_32bit(block_offset + 0x0C + (0x04*i),streamFile);
                vgmstream->ch[i].offset = block_offset + 0x0C + (0x04*vgmstream->channels) + channel_start + 0x01;
            }

            /* flush decoder in every block change */
            flush_ea_mt(vgmstream);
            break;

#ifdef VGM_USE_MPEG
        /* id, size, samples, offset?, unknown (null for MP2, some constant for all blocks for EALayer3) */
        case coding_MPEG_custom:
        case coding_MPEG_layer1:
        case coding_MPEG_layer2:
        case coding_MPEG_layer3:
        case coding_MPEG_ealayer3:
            for (i = 0; i < vgmstream->channels; i++) {
                off_t channel_start = read_32bit(block_offset + 0x0C,streamFile);
                vgmstream->ch[i].offset = block_offset + 0x0C + (0x04*vgmstream->channels) + channel_start;
            }

            /* SCHl with multiple SCHl need to reset their MPEG decoder as there are trailing samples in the buffers */
            if (new_schl) {
                flush_mpeg(vgmstream->codec_data);
            }

            break;
#endif
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
}
