#include "meta.h"
#include "../layout/layout.h"
#include "../util.h"

/* Audio Interchange File Format AIFF-C */

/* for reading integers inexplicably packed into 80 bit floats */
uint32_t read80bitSANE(off_t offset, STREAMFILE *streamFile) {
    uint8_t buf[10];
    int32_t exponent;
    int32_t mantissa;
    int i;

    if (read_streamfile(buf,offset,10,streamFile) != 10) return 0;

    exponent = ((buf[0]<<8)|(buf[1]))&0x7fff;
    exponent -= 16383;

    mantissa = 0;
    for (i=0;i<8;i++) {
        int32_t shift = exponent-7-i*8;
        if (shift >= 0)
            mantissa |= buf[i+2] << shift;
        else if (shift > -8)
            mantissa |= buf[i+2] >> -shift;
    }

    return mantissa*((buf[0]&0x80)?-1:1);
}

VGMSTREAM * init_vgmstream_aifc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];

    off_t file_size = -1;
    int channel_count = 0;
    int sample_count = 0;
    int sample_size = 0;
    int sample_rate = 0;
    int coding_type = -1;
    off_t start_offset = -1;
    int interleave = -1;

    int FormatVersionChunkFound = 0;
    int CommonChunkFound = 0;
    int SoundDataChunkFound = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("aifc",filename_extension(filename)) &&
        strcasecmp("afc",filename_extension(filename))) goto fail;

    /* check header */
    if ((uint32_t)read_32bitBE(0,streamFile)!=0x464F524D || /* "FORM" */
        (uint32_t)read_32bitBE(8,streamFile)!=0x41494643 || /* "AIFC" */
        /* check that file = header (8) + data */
        read_32bitBE(4,streamFile)+8!=get_streamfile_size(streamFile)) goto fail;
    
    file_size = get_streamfile_size(streamFile);

    /* read through chunks to verify format and find metadata */
    {
        off_t current_chunk = 0xc; /* start with first chunk within FORM */

        while (current_chunk < file_size) {
            uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
            off_t chunk_size = read_32bitBE(current_chunk+4,streamFile);

            /* chunks must be padded to an even number of bytes but chunk
             * size does not include that padding */
            if (chunk_size % 2) chunk_size++;

            if (current_chunk+8+chunk_size > file_size) goto fail;

            switch(chunk_type) {
                case 0x46564552:    /* FVER */
                    /* only one per file */
                    if (FormatVersionChunkFound) goto fail;
                    FormatVersionChunkFound = 1;

                    /* specific size */
                    if (chunk_size != 4) goto fail;

                    /* Version 1 of AIFF-C spec timestamp */
                    if ((uint32_t)read_32bitBE(current_chunk+8,streamFile) !=
                            0xA2805140) goto fail;
                    break;
                case 0x434F4D4D:    /* COMM */
                    /* only one per file */
                    if (CommonChunkFound) goto fail;
                    CommonChunkFound = 1;

                    channel_count = read_16bitBE(current_chunk+8,streamFile);
                    if (channel_count <= 0) goto fail;

                    sample_count = (uint32_t)read_32bitBE(current_chunk+0xa,streamFile);

                    sample_size = read_16bitBE(current_chunk+0xe,streamFile);

                    sample_rate = read80bitSANE(current_chunk+0x10,streamFile);

                    switch (read_32bitBE(current_chunk+0x1a,streamFile)) {
                        case 0x53445832:    /* SDX2 */
                            coding_type = coding_SDX2;
                            interleave = 1;
                            break;
                        default:
                            /* we should probably support uncompressed here */
                            goto fail;
                    }
                    
                    /* we don't check the human-readable portion */

                    break;
                case 0x53534E44:    /* SSND */
                    /* at most one per file */
                    if (SoundDataChunkFound) goto fail;
                    SoundDataChunkFound = 1;

                    start_offset = current_chunk + 16 + read_32bitBE(current_chunk+8,streamFile);
                    break;
                default:
                    /* spec says we can skip unrecognized chunks */
                    break;
            }

            current_chunk += 8+chunk_size;
        }
    }

    /* we require at least these */
    if (!FormatVersionChunkFound || !CommonChunkFound || !SoundDataChunkFound)
        goto fail;

    /* build the VGMSTREAM */

    vgmstream = allocate_vgmstream(channel_count,0);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->num_samples = sample_count;
    vgmstream->sample_rate = sample_rate;

    vgmstream->coding_type = coding_type;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = interleave;
    vgmstream->meta_type = meta_AIFC;

    /* open the file, set up each channel */
    {
        int i;

        vgmstream->ch[0].streamfile = streamFile->open(streamFile,filename,
                STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!vgmstream->ch[0].streamfile) goto fail;

        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = vgmstream->ch[0].streamfile;
            vgmstream->ch[i].offset = vgmstream->ch[i].channel_start_offset =
                start_offset+i*interleave;
        }
    }


    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
