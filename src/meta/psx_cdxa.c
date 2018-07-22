#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* CD-XA - from Sony PS1 CDs */
VGMSTREAM * init_vgmstream_cdxa(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count, sample_rate;
    int is_blocked;
    size_t file_size = get_streamfile_size(streamFile);

    /* checks
     * .xa: common, .str: sometimes (mainly videos) */
    if ( !check_extensions(streamFile,"xa,str") )
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
        else { /* headerless and incorrectly ripped */
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
                /* first 0x10 ADPCM filter index should be 0..3 */
                for (j = 0; j < 16; j++) {
                    uint8_t header = (uint8_t)read_8bit(test_offset + i, streamFile);
                    if (((header >> 4) & 0xF) > 3)
                        goto fail;
                }
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
        /* headerless, probably will sound wrong */
        channel_count = 2;
        sample_rate = 37800;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    //todo do block_updates to find num_samples? (to skip non-audio blocks)
    vgmstream->num_samples = xa_bytes_to_samples(file_size - start_offset, channel_count, is_blocked);

    vgmstream->meta_type = meta_PSX_XA;
    vgmstream->coding_type = coding_XA;
    vgmstream->layout_type = is_blocked ? layout_blocked_xa : layout_none;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    if (vgmstream->layout_type == layout_blocked_xa)
        block_update_xa(start_offset,vgmstream);
    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
