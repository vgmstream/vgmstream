#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* SWVR - from EA games, demuxed from .av/trk/mis/etc [Future Cop L.A.P.D. (PS/PC), Freekstyle (PS2/GC), EA Sports Supercross (PS)] */
VGMSTREAM * init_vgmstream_ea_swvr(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count, sample_rate, big_endian;
    coding_t coding;
    uint32_t block_id;
    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;
    int total_subsongs, target_subsong = streamFile->stream_index;


    /* checks */
    /* .stream: common (found inside files)
     * .str: shortened, probably unnecessary */
    if (!check_extensions(streamFile,"stream,str"))
        goto fail;

    /* blocks ids are in machine endianness */
    if (read_32bitBE(0x00,streamFile) == 0x52565753) { /* "RVWS" (PS1/PS2/PC) */
        big_endian = 0;
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        start_offset = read_32bit(0x04, streamFile);
    }
    else if (read_32bitBE(0x00,streamFile) == 0x53575652) { /* "SWVR" (GC) */
        big_endian = 1;
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        start_offset = read_32bit(0x04, streamFile);
    }
    else if (read_32bitBE(0x00,streamFile) == 0x4D474156) { /* "MGAV", Freekstyle (PS2) raw movies */
        big_endian = 0;
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        start_offset = 0x00;
    }
    else if (read_32bitBE(0x00,streamFile) == 0x4453504D) { /* "DSPM", Freekstyle (GC) raw movies */
        big_endian = 1;
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        start_offset = 0x00;
    }
    else {
        goto fail;
    }

    if (read_32bit(start_offset+0x00, streamFile) == 0x50414444) /* "PADD" (Freekstyle) */
        start_offset += read_32bit(start_offset+0x04, streamFile);
    else if (read_32bit(start_offset+0x10, streamFile) == 0x53484452) /* "SHDR" (Future Cop PC) */
        start_offset += read_32bit(start_offset+0x04, streamFile);

    if (read_32bit(start_offset+0x00, streamFile) == 0x46494C4C) /* "FILL" (Freekstyle) */
        start_offset += read_32bit(start_offset+0x04, streamFile);

    total_subsongs = 1;
    block_id = read_32bit(start_offset, streamFile);

    /* files are basically headerless so we inspect the first block
     * Freekstyle uses multiblocks/subsongs (though some subsongs may be clones?) */
    switch(block_id) {
        case 0x5641474D: /* "VAGM" */
            coding = coding_PSX;
            if (read_16bit(start_offset+0x1a, streamFile) == 0x0024) {
                total_subsongs = read_32bit(start_offset+0x0c, streamFile)+1;
                sample_rate = 22050;
            }
            else {
                sample_rate = 14008;
            }
            channel_count = 2;
            break;
        case 0x56414742: /* "VAGB" */
            coding = coding_PSX;
            if (read_16bit(start_offset+0x1a, streamFile) == 0x6400) {
                sample_rate = 22050;
            }
            else {
                sample_rate = 14008;
            }
            channel_count = 1;
            break;
        case 0x4453504D: /* "DSPM" */
            coding = coding_NGC_DSP;
            total_subsongs = read_32bit(start_offset+0x0c, streamFile)+1;
            sample_rate = 22050;
            channel_count = 2;
            break;
        case 0x44535042: /* "DSPB" */
            coding = coding_NGC_DSP;
            channel_count = 1;
            sample_rate = 22050;
            break;
        case 0x4D534943: /* "MSIC" */
            coding = coding_PCM8_U_int;
            channel_count = 2;
            sample_rate = 14008;
            break;
        case 0x53484F43: /* "SHOC" (a generic block but hopefully has PC sounds) */
            if (read_32bit(start_offset+0x10, streamFile) == 0x53484F43) { /* SHDR */
                coding = coding_PCM8_U_int; //todo there are other codecs
                channel_count = 1;
                sample_rate = 14008;
            }
            else {
                goto fail;
            }
            break;
        default:
            VGM_LOG("EA SWVR: unknown block id\n");
            goto fail;
    }

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    loop_flag = 0;//(channel_count > 1); /* some Future Cop LAPD tracks repeat but other games have fadeouts */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_EA_SWVR;
    vgmstream->sample_rate = sample_rate;
    vgmstream->codec_endian = big_endian;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = get_streamfile_size(streamFile) / total_subsongs; /* approx... */

    vgmstream->coding_type = coding;
    vgmstream->layout_type = layout_blocked_ea_swvr;
    /* DSP coefs are loaded per block */
    /* some files (voices etc) decode with pops but seems a mastering problem */

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
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
        while (vgmstream->next_block_offset < get_streamfile_size(streamFile));
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
