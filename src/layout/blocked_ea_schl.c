#include "layout.h"
#include "../coding/coding.h"
#include "../vgmstream.h"

/* parse EA style blocks, id+size+samples+data */
void block_update_ea_schl(off_t block_offset, VGMSTREAM * vgmstream) {
    STREAMFILE* streamFile = vgmstream->ch[0].streamfile;
    int i;
    size_t block_size, block_samples;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = vgmstream->codec_endian ? read_32bitBE : read_32bitLE;

    uint32_t flag_lang = (vgmstream->codec_config >> 16) & 0xFFFF;
    int flag_be = (vgmstream->codec_config & 0x02);
    int flag_adpcm = (vgmstream->codec_config & 0x01);
    int flag_offsets = (vgmstream->codec_config & 0x04);


    /* EOF reads: signal we have nothing and let the layout fail */
    if (block_offset >= get_streamfile_size(streamFile)) {
        vgmstream->current_block_offset = block_offset;
        vgmstream->next_block_offset = block_offset;
        vgmstream->current_block_samples = -1;
        return;
    }

    /* read a single block */
    {
        uint32_t block_id = read_32bitBE(block_offset+0x00,streamFile);

        if (flag_be) /* size is always LE, except in early SS/MAC */
            block_size = read_32bitBE(block_offset + 0x04,streamFile);
        else
            block_size = read_32bitLE(block_offset + 0x04,streamFile);

        if (block_id == 0x5343446C || block_id == (0x53440000 | flag_lang)) {
            /* "SCDl" or "SDxx" audio chunk */
            if (vgmstream->coding_type == coding_PSX)
                block_samples = ps_bytes_to_samples(block_size-0x10, vgmstream->channels);
            else
                block_samples = read_32bit(block_offset+0x08,streamFile);
        }
        else {
            /* ignore other chunks (audio "SCHl/SCCl/...", non-target lang, video "pIQT/MADk/...", etc) */
            block_samples = 0; /* layout ignores this */
        }

        if (block_id == 0x00000000 || block_id == 0xFFFFFFFF || block_id == 0x5343456C) { /* EOF */
            vgmstream->current_block_samples = -1;
            return;
        }
    }


    vgmstream->current_block_offset = block_offset;
    vgmstream->next_block_offset = block_offset + block_size;
    vgmstream->current_block_samples = block_samples;
    vgmstream->current_block_size = 0; /* uses current_block_samples instead */

    /* no need to setup offsets (plus could read over filesize near EOF) */
    if (block_samples == 0)
        return;


    /* set new channel offsets and ADPCM history */
    /* ADPCM hist could be considered part of the stream/decoder (some EAXA decoders call it "EAXA R1" when it has hist), and BNKs
     * (with no blocks) may also have them in the first offset, but also may not. To simplify we just read them here. */
    if (!flag_offsets) { /* v0 doesn't provide channel offsets, they need to be calculated */
        switch (vgmstream->coding_type) {
            /* id, size, samples, data */
            case coding_PCM8_int:
                for (i = 0; i < vgmstream->channels; i++) {
                    vgmstream->ch[i].offset = block_offset + 0x0c + (i*0x01);
                }

                break;

            /* id, size, samples, data */
            case coding_PCM16_int:
                for (i = 0; i < vgmstream->channels; i++) {
                    vgmstream->ch[i].offset = block_offset + 0x0c + (i*0x02);
                }

                break;

            /* id, size, samples, data */
            case coding_PCM8:
                for (i = 0; i < vgmstream->channels; i++) {
                    vgmstream->ch[i].offset = block_offset + 0x0c + (block_samples*i*0x01);
                }

                break;

            /* id, size, samples, data */
            case coding_PCM16LE:
            case coding_PCM16BE:
                for (i = 0; i < vgmstream->channels; i++) {
                    vgmstream->ch[i].offset = block_offset + 0x0c + (block_samples*i*0x02);
                }

                break;

            /* id, size, unk1, unk2, interleaved data */
            case coding_PSX:
                for (i = 0; i < vgmstream->channels; i++) {
                    size_t interleave = (block_size-0x10) / vgmstream->channels;
                    vgmstream->ch[i].offset = block_offset + 0x10 + i*interleave;
                }
                /* 0x08/0x0c: unknown (doesn't look like hist or offsets, as 1ch files has them too) */

                break;

            /* id, size, samples, IMA hist, stereo/mono data */
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

            case coding_EA_MT: /* not seen in v0 streams so far, may not exist */
            default:
                VGM_LOG("EA SCHl: Unkonwn channel offsets in blocked layout\n");
                vgmstream->current_block_samples = -1;
                break;
        }
    } else {
        switch(vgmstream->coding_type) {
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
            /* id, size, samples, offsets, unknown (null for MP2, some size/config for EALayer3; only if not >2ch) */
            case coding_MPEG_custom:
            case coding_MPEG_layer1:
            case coding_MPEG_layer2:
            case coding_MPEG_layer3:
            case coding_MPEG_ealayer3:
                for (i = 0; i < vgmstream->channels; i++) {
                    off_t channel_start;

                    /* EALayer3 6ch uses 1ch*6 with offsets, no flag in header [Medal of Honor 2010 (PC) movies] */
                    if (vgmstream->channels > 2) {
                        channel_start = read_32bit(block_offset + 0x0C + 0x04*i,streamFile);
                    } else {
                        channel_start = read_32bit(block_offset + 0x0C,streamFile);
                    }

                    vgmstream->ch[i].offset = block_offset + 0x0C + (0x04*vgmstream->channels) + channel_start;
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
                if (flag_adpcm) {
                    for (i = 0; i < vgmstream->channels; i++) {
                        //vgmstream->ch[i].adpcm_history1_32 = read_16bit(vgmstream->ch[i].offset+0x00,streamFile);
                        //vgmstream->ch[i].adpcm_history3_32 = read_16bit(vgmstream->ch[i].offset+0x02,streamFile);
                        vgmstream->ch[i].offset += 0x04;
                    }
                }

                break;
        }
    }
}
