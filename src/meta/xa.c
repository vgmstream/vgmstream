#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* CD-XA - from Sony PS1 and Philips CD-i CD audio */
VGMSTREAM * init_vgmstream_xa(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count, sample_rate;
    int is_blocked;
    size_t file_size = get_streamfile_size(streamFile);


    /* checks
     * .xa: common, .str: sometimes (mainly videos)
     * .adp: Phantasy Star Collection (SAT) raw XA */
    if ( !check_extensions(streamFile,"xa,str,adp") )
        goto fail;

    /* Proper XA comes in raw (BIN 2352 mode2/form2) CD sectors, that contain XA subheaders.
     * This also has minimal support for headerless (ISO 2048 mode1/data) mode. */

    /* check RIFF header = raw (optional, added when ripping and not part of the CD data) */
    if (read_32bitBE(0x00,streamFile) == 0x52494646 &&  /* "RIFF" */
        read_32bitBE(0x08,streamFile) == 0x43445841 &&  /* "CDXA" */
        read_32bitBE(0x0C,streamFile) == 0x666D7420) {  /* "fmt " */
        is_blocked = 1;
        start_offset = 0x2c; /* after "data" (chunk size tends to be a bit off) */
    }
    else {
        /* sector sync word = raw */
        if (read_32bitBE(0x00,streamFile) == 0x00FFFFFF &&
            read_32bitBE(0x04,streamFile) == 0xFFFFFFFF &&
            read_32bitBE(0x08,streamFile) == 0xFFFFFF00) {
            is_blocked = 1;
            start_offset = 0x00;
        }
        else { /* headerless and possibly incorrectly ripped */
            is_blocked = 0;
            start_offset = 0x00;
        }
    }

    /* test some blocks (except when RIFF) since other .XA/STR may start blank */
    if (start_offset == 0) {
        int i, j, block;
        off_t test_offset = start_offset;
        size_t sector_size = (is_blocked ? 0x900 : 0x800);
        size_t block_size = 0x80;

        for (block = 0; block < 3; block++) {
            test_offset += (is_blocked ? 0x18 : 0x00); /* header */

            for (i = 0; i < (sector_size/block_size); i++) {
                /* XA headers checks: filter indexes should be 0..3, and shifts 0..C */
                for (j = 0; j < 16; j++) {
                    uint8_t header = (uint8_t)read_8bit(test_offset + j, streamFile);
                    if (((header >> 4) & 0xF) > 0x03)
                        goto fail;
                    if (((header >> 0) & 0xF) > 0x0c)
                        goto fail;
                }
                /* XA headers pairs are repeated */
                if (read_32bitBE(test_offset+0x00,streamFile) != read_32bitBE(test_offset+0x04,streamFile) ||
                    read_32bitBE(test_offset+0x08,streamFile) != read_32bitBE(test_offset+0x0c,streamFile))
                    goto fail;
                /* blank frames should always use 0x0c0c0c0c (due to how shift works) */
                if (read_32bitBE(test_offset+0x00,streamFile) == 0 &&
                    read_32bitBE(test_offset+0x04,streamFile) == 0 &&
                    read_32bitBE(test_offset+0x08,streamFile) == 0 &&
                    read_32bitBE(test_offset+0x0c,streamFile) == 0)
                    goto fail;

                test_offset += 0x80;
            }

            test_offset += (is_blocked ? 0x18 : 0x00); /* footer */
        }
    }


    /* data is ok: parse header */
    if (is_blocked) {
        /* parse 0x18 sector header (also see xa_blocked.c)  */
        uint8_t xa_header = (uint8_t)read_8bit(start_offset + 0x13,streamFile);

        switch((xa_header >> 0) & 3) { /* 0..1: mono/stereo */
            case 0: channel_count = 1; break;
            case 1: channel_count = 2; break;
            default: goto fail;
        }
        switch((xa_header >> 2) & 3) { /* 2..3: sample rate */
            case 0: sample_rate = 37800; break;
            case 1: sample_rate = 18900; break;
            default: goto fail;
        }
        switch((xa_header >> 4) & 3) { /* 4..5: bits per sample (0=4, 1=8) */
            case 0: break;
            default: /* PS1 games only do 4b */
                VGM_LOG("XA: unknown bits per sample found\n");
                goto fail;
        }
        switch((xa_header >> 6) & 1) { /* 6: emphasis (applies a filter) */
            case 0: break;
            default: /*  shouldn't be used by games */
                VGM_LOG("XA: unknown emphasis found\n");
                 break;
        }
        switch((xa_header >> 7) & 1) { /* 7: reserved */
            case 0: break;
            default:
                VGM_LOG("XA: unknown reserved bit found\n");
                 break;
        }
    }
    else {
        /* headerless */
        if (check_extensions(streamFile,"adp")) {
            /* Phantasy Star Collection (SAT) raw files */
            /* most are stereo, though a few (mainly sfx banks, sometimes using .bin) are mono */

            char filename[PATH_LIMIT] = {0};
            get_streamfile_filename(streamFile, filename,PATH_LIMIT);

            /* detect PS1 mono files, very lame but whatevs, no way to detect XA mono/stereo */
            if (filename[0]=='P' && filename[1]=='S' && filename[2]=='1' && filename[3]=='S') {
                channel_count = 1;
                sample_rate = 22050;
            }
            else {
                channel_count = 2;
                sample_rate = 44100;
            }
        }
        else {
            /* incorrectly ripped standard XA */
            channel_count = 2;
            sample_rate = 37800;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;

    vgmstream->meta_type = meta_XA;
    vgmstream->coding_type = coding_XA;
    vgmstream->layout_type = is_blocked ? layout_blocked_xa : layout_none;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;


    if (is_blocked) {
        /* calc num_samples as blocks may be empty or smaller than usual depending on flags */
        vgmstream->next_block_offset = start_offset;
        do {
            block_update(vgmstream->next_block_offset,vgmstream);
            vgmstream->num_samples += vgmstream->current_block_samples;
        }
        while (vgmstream->next_block_offset < get_streamfile_size(streamFile));
        block_update(start_offset,vgmstream);
    }
    else {
        vgmstream->num_samples = xa_bytes_to_samples(file_size - start_offset, channel_count, is_blocked);
    }

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
