#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* SWVR - from EA games, demuxed from .av/trk/mis/etc [Future Cop L.A.P.D. (PS/PC), Freekstyle (PS2/GC), EA Sports Supercross (PS)] */
VGMSTREAM* init_vgmstream_ea_swvr(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channels, sample_rate, big_endian;
    coding_t coding;
    uint32_t block_id;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    /* .stream: common (found inside files)
     * .str: shortened, probably unnecessary */
    if (!check_extensions(sf,"stream,str"))
        goto fail;

    /* Files have no actual audio headers, so we inspect the first block for known values.
     * Freekstyle uses multiblocks/subsongs (though some subsongs may be clones?) */

    /* blocks ids are in machine endianness */
    if (read_u32be(0x00,sf) == get_id32be("RVWS")) { /* PS1/PS2/PC */
        big_endian = 0;
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        start_offset = read_32bit(0x04, sf);
    }
    else if (read_u32be(0x00,sf) == get_id32be("SWVR")) { /* GC */
        big_endian = 1;
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        start_offset = read_32bit(0x04, sf);
    }
    else if (read_u32be(0x00,sf) == get_id32be("MGAV")) { /* Freekstyle (PS2) raw movies */
        big_endian = 0;
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        start_offset = 0x00;
    }
    else if (read_u32be(0x00,sf) == get_id32be("DSPM")) { /* Freekstyle (GC) raw movies */
        big_endian = 1;
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        start_offset = 0x00;
    }
    else {
        goto fail;
    }

    if (read_32bit(start_offset+0x00, sf) == get_id32be("PADD")) /* Freekstyle */
        start_offset += read_32bit(start_offset+0x04, sf);

    if (read_32bit(start_offset+0x00, sf) == get_id32be("FILL")) /* Freekstyle */
        start_offset += read_32bit(start_offset+0x04, sf);

    total_subsongs = 1;
    block_id = read_32bit(start_offset, sf);
    /* value after block id (usually at 0x38) is number of blocks of 0x6000 (results in file size, including FILLs) */

    /* intended sample rate for PSX music (verified in emus) should be 14260, but is found in ELF as pitch value
     * (ex. Nascar Rumble 0x052C in SLUS_010.68 at 0x000143BC): 0x052C * 44100 / 4096 ~= 14254.98046875hz
     * Future Cop PSX pitch looks similar (comparing vs recordings). */
    switch(block_id) {
        case 0x5641474D: /* "VAGM" (stereo music) */
            coding = coding_PSX;
            if (read_16bit(start_offset+0x1a, sf) == 0x0024) {
                total_subsongs = read_32bit(start_offset+0x0c, sf)+1;
                sample_rate = 22050; /* Freekstyle (PS2) */
            }
            else {
                sample_rate = 1324 * 44100 / 4096; /* ~14254 [Future Cop (PS1), Nascar Rumble (PS1), EA Sports Motocross (PS1)] */
            }
            channels = 2;
            break;
        case 0x56414742: /* "VAGB" (mono sfx/voices)*/
            coding = coding_PSX;
            if (read_16bit(start_offset+0x1a, sf) == 0x6400) {
                sample_rate = 22050; /* Freekstyle (PS2) */
            }
            else {
                /* approximate as pitches vary per file (ex. Nascar Rumble: engine=3779, heli=22050, commentary=11050) */
                sample_rate = 1080 * 44100 / 4096; /* ~11627 [EA Sports Motocross (PS1)] */
            }
            channels = 1;
            break;
        case 0x4453504D: /* "DSPM" (stereo music) */
            coding = coding_NGC_DSP;
            total_subsongs = read_32bit(start_offset+0x0c, sf)+1;
            sample_rate = 22050; /* Freekstyle (GC) */
            channels = 2;
            break;
        case 0x44535042: /* "DSPB" (mono voices/sfx) */
            coding = coding_NGC_DSP;
            channels = 1;
            sample_rate = 22050; /* Freekstyle (GC) */
            break;
        case 0x4D534943: /* "MSIC" (stereo music) */
            coding = coding_PCM8_U_int;
            channels = 2;
            sample_rate = 14291; /* assumed, by comparing vs PSX output [Future Cop (PC)] */
            break;
        case 0x53484F43: /* "SHOC" (a generic block but hopefully has PC sounds) */
            if (read_32bit(start_offset+0x10, sf) == get_id32be("SHDR")) { /* Future Cop (PC) */
                /* there is a mini header? after SHDR
                 * 0x00: 5
                 * 0x04: "snds"
                 * 0x08: channels?
                 * 0x0c: data size without blocks/padding
                 * 0x10: null
                 * 0x14: null
                 * 0x18: null
                 * 0x1c: 1
                 * 0x20: 1
                 * 0x24: 0x4F430000 */
                if (read_32bit(start_offset+0x18, sf) != get_id32be("snds"))
                    goto fail;
                coding = coding_PCM8_U_int;
                channels = 1;
                sample_rate = 22050; /* assumed */
            }
            else {
                //todo
                /* The Lord of the Rings - The Return of the King (PC) uses IMA for sfx (in non-RVWS chunks)
                 * and some unknown codec for streams (mixed with strings and cutscene(?) commands) */
                goto fail;
            }
            break;
        default:
            VGM_LOG("EA SWVR: unknown block id\n");
            goto fail;
    }

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    loop_flag = 0;//(channels > 1); /* some Future Cop LAPD tracks repeat but other games have fadeouts */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_EA_SWVR;
    vgmstream->sample_rate = sample_rate;
    vgmstream->codec_endian = big_endian;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = get_streamfile_size(sf) / total_subsongs; /* approx... */

    vgmstream->coding_type = coding;
    vgmstream->layout_type = layout_blocked_ea_swvr;
    /* DSP coefs are loaded per block */
    /* some files (voices etc) decode with pops but seems a mastering problem */

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;

    /* calc num_samples manually */
    {
        int num_samples;
        vgmstream->stream_index = target_subsong; /* needed to skip other subsong-blocks */
        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset,vgmstream);
            switch(vgmstream->coding_type) {
                case coding_PSX:     	num_samples = ps_bytes_to_samples(vgmstream->current_block_size,1); break;
                case coding_NGC_DSP: 	num_samples = dsp_bytes_to_samples(vgmstream->current_block_size,1); break;
                case coding_PCM8_U_int: num_samples = pcm_bytes_to_samples(vgmstream->current_block_size,1,8); break;
                default:             	num_samples = 0; break;
            }
            vgmstream->num_samples += num_samples;
        }
        while (vgmstream->next_block_offset < get_streamfile_size(sf));
        block_update(start_offset, vgmstream);
    }

    if (loop_flag) {
        vgmstream->loop_start_sample = 0;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
