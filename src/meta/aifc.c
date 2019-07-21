#include "meta.h"
#include "../layout/layout.h"


/* for reading integers inexplicably packed into 80-bit ('double extended') floats */
static uint32_t read80bitSANE(off_t offset, STREAMFILE *streamFile) {
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

static uint32_t find_marker(STREAMFILE *streamFile, off_t MarkerChunkOffset, int marker_id) {
    uint16_t marker_count;
    int i;
    off_t marker_offset;

    marker_count = read_16bitBE(MarkerChunkOffset+8,streamFile);
    marker_offset = MarkerChunkOffset+10;
    for (i=0;i<marker_count;i++) {
        int name_length;
        
        if (read_16bitBE(marker_offset,streamFile) == marker_id)
            return read_32bitBE(marker_offset+2,streamFile);

        name_length = (uint8_t)read_8bit(marker_offset+6,streamFile) + 1;
        if (name_length % 2) name_length++;
        marker_offset += 6 + name_length;
    }

    return -1;
}


/* Audio Interchange File Format AIFF/AIFF-C - from Mac/3DO games */
VGMSTREAM * init_vgmstream_aifc(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0;
    size_t file_size;
    coding_t coding_type = 0;
    int channel_count = 0, sample_count = 0, sample_size = 0, sample_rate = 0;
    int interleave = 0;

    int loop_flag = 0;
    int32_t loop_start = 0, loop_end = 0;

    int is_aiff_ext = 0, is_aifc_ext = 0, is_aiff = 0, is_aifc = 0;
    int FormatVersionChunkFound = 0, CommonChunkFound = 0, SoundDataChunkFound = 0, MarkerChunkFound = 0, InstrumentChunkFound = 0;
    off_t MarkerChunkOffset = -1, InstrumentChunkOffset = -1;


    /* checks */
    /* .aif: common (AIFF or AIFC), .aiff: common AIFF, .aifc: common AIFC
     * .laif/laifc/laiff: for plugins
     * .aifcl/aiffl: for plugins?
     * .cbd2: M2 games
     * .bgm: Super Street Fighter II Turbo (3DO)
     * .acm: Crusader - No Remorse (SAT)
     * .adp: Sonic Jam (SAT)
     * .ai: Dragon Force (SAT)
     * (extensionless: Doom (3DO) */
    if (check_extensions(streamFile, "aif,laif,")) {
        is_aifc_ext = 1;
        is_aiff_ext = 1;
    }
    else if (check_extensions(streamFile, "aifc,laifc,aifcl,afc,cbd2,bgm")) {
        is_aifc_ext = 1;
    }
    else if (check_extensions(streamFile, "aiff,laiff,acm,adp,ai,aiffl")) {
        is_aiff_ext = 1;
    }
    else {
        goto fail;
    }

    file_size = get_streamfile_size(streamFile);
    if ((uint32_t)read_32bitBE(0x00,streamFile) != 0x464F524D &&  /* "FORM" */
        (uint32_t)read_32bitBE(0x04,streamFile)+0x08 != file_size)
        goto fail;

    if ((uint32_t)read_32bitBE(0x08,streamFile) == 0x41494643) { /* "AIFC" */
        if (!is_aifc_ext) goto fail;
        is_aifc = 1;
    }
    else if ((uint32_t)read_32bitBE(0x08,streamFile) == 0x41494646) { /* "AIFF" */
        if (!is_aiff_ext) goto fail;
        is_aiff = 1;
    }
    else {
        goto fail;
    }


    /* read through chunks to verify format and find metadata */
    {
        off_t current_chunk = 0x0c; /* start with first chunk within FORM */

        while (current_chunk < file_size) {
            uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
            off_t chunk_size = read_32bitBE(current_chunk+0x04,streamFile);

            /* chunks must be padded to an even number of bytes but chunk
             * size does not include that padding */
            if (chunk_size % 2) chunk_size++;

            if (current_chunk+8+chunk_size > file_size) goto fail;

            switch(chunk_type) {
                case 0x46564552:    /* "FVER" (version info) */
                    if (FormatVersionChunkFound) goto fail;
                    if (is_aiff) goto fail; /* plain AIFF shouldn't have */
                    FormatVersionChunkFound = 1;

                    /* specific size */
                    if (chunk_size != 4) goto fail;

                    /* Version 1 of AIFF-C spec timestamp */
                    if ((uint32_t)read_32bitBE(current_chunk+0x08,streamFile) != 0xA2805140) goto fail;
                    break;

                case 0x434F4D4D:    /* "COMM" (main header) */
                    if (CommonChunkFound) goto fail;
                    CommonChunkFound = 1;

                    channel_count = read_16bitBE(current_chunk+8,streamFile);
                    if (channel_count <= 0) goto fail;

                    sample_count = (uint32_t)read_32bitBE(current_chunk+0x0a,streamFile); /* sometimes number of blocks */
                    sample_size = read_16bitBE(current_chunk+0x0e,streamFile);
                    sample_rate = read80bitSANE(current_chunk+0x10,streamFile);

                    if (is_aifc) {
                        switch (read_32bitBE(current_chunk+0x1a,streamFile)) {
                            case 0x53445832:    /* "SDX2" [3DO games: Super Street Fighter II Turbo (3DO), etc] */
                                coding_type = coding_SDX2;
                                interleave = 0x01;
                                break;
                            case 0x43424432:    /* "CBD2" [M2 (arcade 3DO) games: IMSA Racing (M2), etc] */
                                coding_type = coding_CBD2;
                                interleave = 0x01;
                                break;
                            case 0x41445034:    /* "ADP4" */
                                coding_type = coding_DVI_IMA_int;
                                if (channel_count != 1) break; /* don't know how stereo DVI is laid out */
                                break;
                            case 0x696D6134:    /* "ima4"  [Alida (PC), Lunar SSS (iOS)] */
                                coding_type = coding_APPLE_IMA4;
                                interleave = 0x22;
                                sample_count = sample_count * ((interleave-0x2)*2);
                                break;
                            default:
                                VGM_LOG("AIFC: unknown codec\n");
                                goto fail;
                        }
                        /* string size and human-readable AIFF-C codec follows */
                    }
                    else if (is_aiff) {
                        switch (sample_size) {
                            case 8:
                                coding_type = coding_PCM8;
                                interleave = 1;
                                break;
                            case 16:
                                coding_type = coding_PCM16BE;
                                interleave = 2;
                                break;
                            case 4: /* Crusader: No Remorse (SAT), Road Rash (3DO) */
                                coding_type = coding_XA;
                                break;
                            default:
                                VGM_LOG("AIFF: unknown codec\n");
                                goto fail;
                        }
                    }
                    break;

                case 0x53534E44:    /* "SSND" (main data) */
                case 0x4150434D:    /* "APCM" (main data for XA) */
                    if (SoundDataChunkFound) goto fail;
                    SoundDataChunkFound = 1;

                    start_offset = current_chunk + 0x10 + read_32bitBE(current_chunk+0x08,streamFile);
                    /* when "APCM" XA frame size is at 0x0c, fixed to 0x914 */
                    break;

                case 0x4D41524B:    /* "MARK" (loops) */
                    if (MarkerChunkFound) goto fail;
                    MarkerChunkFound = 1;

                    MarkerChunkOffset = current_chunk;
                    break;

                case 0x494E5354:    /* "INST" (loops) */
                    if (InstrumentChunkFound) goto fail;
                    InstrumentChunkFound = 1;

                    InstrumentChunkOffset = current_chunk;
                    break;

                default:
                    /* spec says we can skip unrecognized chunks */
                    break;
            }

            current_chunk += 0x08+chunk_size;
        }
    }

    if (is_aifc) {
        if (!FormatVersionChunkFound || !CommonChunkFound || !SoundDataChunkFound)
            goto fail;
    } else if (is_aiff) {
        if (!CommonChunkFound || !SoundDataChunkFound)
            goto fail;
    }


    /* read loop points */
    if (InstrumentChunkFound && MarkerChunkFound) {
        int start_marker;
        int end_marker;
        /* use the sustain loop */
        /* if playMode=ForwardLooping */
        if (read_16bitBE(InstrumentChunkOffset+16,streamFile) == 1) {
            start_marker = read_16bitBE(InstrumentChunkOffset+18,streamFile);
            end_marker = read_16bitBE(InstrumentChunkOffset+20,streamFile);
            /* check for sustain markers != 0 (invalid marker no) */
            if (start_marker && end_marker) {
                /* find start marker */
                loop_start = find_marker(streamFile,MarkerChunkOffset,start_marker);
                loop_end = find_marker(streamFile,MarkerChunkOffset,end_marker);

                /* find_marker is type uint32_t as the spec says that's the type
                 * of the position value, but it returns a -1 on error, and the
                 * loop_start and loop_end variables are int32_t, so the error
                 * will become apparent.
                 * We shouldn't have a loop point that overflows an int32_t
                 * anyway. */
                loop_flag = 1;
                if (loop_start==loop_end)
                    loop_flag = 0;
            }
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = sample_count;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_type;
    if (coding_type == coding_XA) {
        vgmstream->layout_type = layout_blocked_xa_aiff;
        /* AIFF XA can use sample rates other than 37800/18900 */
        /* some Crusader: No Remorse tracks have XA headers with incorrect 0xFF, rip bug/encoder feature? */
    }
    else {
        vgmstream->layout_type = (channel_count > 1) ? layout_interleave : layout_none;
        vgmstream->interleave_block_size = interleave;
    }

    if (is_aifc)
        vgmstream->meta_type = meta_AIFC;
    else if (is_aiff)
        vgmstream->meta_type = meta_AIFF;


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
