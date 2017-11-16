#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"

/* CD-XA - from Sony PS1 CDs */
VGMSTREAM * init_vgmstream_cdxa(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count, sample_rate;
    int xa_channel=0;
    int is_blocked;
    size_t file_size = get_streamfile_size(streamFile);

    /* check extension (.xa: common, .str: sometimes used) */
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
        else { /* headerless */
            is_blocked = 0;
            start_offset = 0x00;
        }
    }

    /* test first block (except when RIFF) */
    if (start_offset == 0) {
        int i, j;

        /* 0x80 frames for 1 sector (max ~0x800 for ISO mode)  */
        for (i = 0; i < (0x800/0x80); i++) {
            off_t test_offset = start_offset + (is_blocked ? 0x18 : 0x00) + 0x80*i;

            /* ADPCM predictors should be 0..3 index */
            for (j = 0; j < 16; j++) {
                uint8_t header = read_8bit(test_offset + i, streamFile);
                if (((header >> 4) & 0xF) > 3)
                    goto fail;
            }
        }
    }


    /* data is ok: parse header */
    if (is_blocked) {
        uint8_t xa_header;

        /* parse 0x18 sector header (also see xa_blocked.c)  */
        xa_channel = read_8bit(start_offset + 0x11,streamFile);
        xa_header  = read_8bit(start_offset + 0x13,streamFile);

        switch((xa_header >> 0) & 3) { /* 0..1: stereo */
            case 0: channel_count = 1; break;
            case 1: channel_count = 2; break;
            default: goto fail;
        }
        switch((xa_header >> 2) & 3) { /* 2..3: sample rate */
            case 0: sample_rate = 37800; break;
            case 1: sample_rate = 18900; break;
            default: goto fail;
        }
        VGM_ASSERT(((xa_header >> 4) & 3) == 1, /* 4..5: bits per sample (0=4, 1=8) */
                "XA: 8 bits per sample mode found\n"); /* spec only? */
        /* 6: emphasis (applies a filter but apparently not used by games)
         *   XA is also filtered when resampled to 44100 during output, differently from PS-ADPCM */
        /* 7: reserved */
    }
    else {
        /* headerless, probably will go wrong */
        channel_count = 2;
        sample_rate = 44100; /* not 37800? */
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = xa_bytes_to_samples(file_size - start_offset, channel_count, is_blocked);
    vgmstream->xa_headerless = !is_blocked;
    vgmstream->xa_channel = xa_channel;

    vgmstream->coding_type = coding_XA;
    vgmstream->layout_type = layout_xa_blocked;
    vgmstream->meta_type = meta_PSX_XA;

    if (is_blocked)
        start_offset += 0x18; /* move to first frame (hack for xa_blocked.c) */

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;

    xa_block_update(start_offset,vgmstream);

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
