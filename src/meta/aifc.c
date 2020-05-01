#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* for reading integers inexplicably packed into 80-bit ('double extended') floats */
static uint32_t read_f80be(off_t offset, STREAMFILE* sf) {
    uint8_t buf[0x0a];
    int32_t exponent;
    int32_t mantissa;
    int i;

    if (read_streamfile(buf, offset, sizeof(buf), sf) != sizeof(buf))
        return 0;

    exponent = ((buf[0]<<8) | (buf[1])) & 0x7fff;
    exponent -= 16383;

    mantissa = 0;
    for (i = 0; i < 8; i++) {
        int32_t shift = exponent-7-i*8;
        if (shift >= 0)
            mantissa |= buf[i+2] << shift;
        else if (shift > -8)
            mantissa |= buf[i+2] >> -shift;
    }

    return mantissa * ((buf[0]&0x80) ? -1 : 1);
}

static uint32_t find_marker(STREAMFILE* sf, off_t mark_offset, int marker_id) {
    uint16_t marker_count;
    off_t marker_offset;
    int i;

    marker_count = read_u16be(mark_offset + 0x00,sf);
    marker_offset = mark_offset + 0x02;
    for (i = 0; i < marker_count; i++) {
        int name_length;
        
        if (read_u16be(marker_offset + 0x00, sf) == marker_id)
            return read_u32be(marker_offset + 0x02,sf);

        name_length = read_u8(marker_offset + 0x06,sf) + 1;
        if (name_length % 2) name_length++;
        marker_offset += 0x06 + name_length;
    }

    return -1;
}


/* AIFF/AIFF-C (Audio Interchange File Format) - Apple format, from Mac/3DO/other games */
VGMSTREAM* init_vgmstream_aifc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset = 0;
    size_t file_size;
    coding_t coding_type = 0;
    int channel_count = 0, sample_count = 0, sample_size = 0, sample_rate = 0;
    int interleave = 0;
    int loop_flag = 0;
    int32_t loop_start = 0, loop_end = 0;

    int is_aiff_ext = 0, is_aifc_ext = 0, is_aiff = 0, is_aifc = 0;
    int fver_found = 0, comm_found = 0, data_found = 0;
    off_t mark_offset = 0, inst_offset = 0;


    /* checks */
    /* .aif: common (AIFF or AIFC), .aiff: common AIFF, .aifc: common AIFC
     * .laif/laifc/laiff: for plugins
     * .aifcl/aiffl: for plugins?
     * .cbd2: M2 games
     * .bgm: Super Street Fighter II Turbo (3DO)
     * .acm: Crusader - No Remorse (SAT)
     * .adp: Sonic Jam (SAT)
     * .ai: Dragon Force (SAT)
     * (extensionless: Doom (3DO)
     * .fda: Homeworld 2 (PC) */
    if (check_extensions(sf, "aif,laif,")) {
        is_aifc_ext = 1;
        is_aiff_ext = 1;
    }
    else if (check_extensions(sf, "aifc,laifc,aifcl,afc,cbd2,bgm,fda")) {
        is_aifc_ext = 1;
    }
    else if (check_extensions(sf, "aiff,laiff,acm,adp,ai,aiffl")) {
        is_aiff_ext = 1;
    }
    else {
        goto fail;
    }

    file_size = get_streamfile_size(sf);
    if (read_u32be(0x00,sf) != 0x464F524D &&  /* "FORM" */
        read_u32be(0x04,sf)+0x08 != file_size)
        goto fail;

    if (read_u32be(0x08,sf) == 0x41494643) { /* "AIFC" */
        if (!is_aifc_ext) goto fail;
        is_aifc = 1;
    }
    else if (read_u32be(0x08,sf) == 0x41494646) { /* "AIFF" */
        if (!is_aiff_ext) goto fail;
        is_aiff = 1;
    }
    else {
        goto fail;
    }


    /* read through chunks to verify format and find metadata */
    {
        off_t offset = 0x0c; /* start with first chunk within FORM */

        while (offset < file_size) {
            uint32_t chunk_type = read_u32be(offset + 0x00,sf);
            uint32_t chunk_size = read_u32be(offset + 0x04,sf);

            /* chunks must be padded to an even number of bytes but chunk
             * size does not include that padding */
            if (chunk_size % 2)
                chunk_size++;

            offset += 0x08;
            if (offset + chunk_size > file_size)
                goto fail;

            switch(chunk_type) {
                case 0x46564552:    /* "FVER" (version info) */
                    if (fver_found) goto fail;
                    if (is_aiff) goto fail; /* plain AIFF shouldn't have */
                    fver_found = 1;

                    /* specific size */
                    if (chunk_size != 4) goto fail;

                    /* Version 1 of AIFF-C spec timestamp */
                    if (read_u32be(offset + 0x00,sf) != 0xA2805140)
                        goto fail;
                    break;

                case 0x434F4D4D:    /* "COMM" (main header) */
                    if (comm_found) goto fail;
                    comm_found = 1;

                    channel_count = read_u16be(offset + 0x00,sf);
                    if (channel_count <= 0) goto fail;

                    sample_count = read_u32be(offset + 0x02,sf); /* sometimes number of blocks */
                    sample_size  = read_u16be(offset + 0x06,sf);
                    sample_rate  = read_f80be(offset + 0x08,sf);

                    if (is_aifc) {
                        uint32_t codec = read_u32be(offset + 0x12,sf);
                        switch (codec) {
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

                            case 0x434F4D50: {  /* "COMP" (generic compression) */
                                uint8_t comp_name[255] = {0};
                                uint8_t comp_size = read_u8(offset + 0x16, sf);
                                if (comp_size >= sizeof(comp_name) - 1) goto fail;
                                
                                read_streamfile(comp_name, offset + 0x17, comp_size, sf);
                                if (memcmp(comp_name, "Relic Codec v1.6", comp_size) == 0) { /* Homeworld 2 (PC) */
                                    coding_type = coding_RELIC;
                                    sample_count = sample_count * 512;
                                }
                                else {
                                    goto fail;
                                }
                                break;
                            }

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
                    if (data_found) goto fail;
                    data_found = 1;

                    start_offset = offset + 0x08 + read_u32be(offset + 0x00,sf);
                    /* when "APCM" XA frame size is at 0x0c, fixed to 0x914 */
                    break;

                case 0x4D41524B:    /* "MARK" (loops) */
                    mark_offset = offset;
                    break;

                case 0x494E5354:    /* "INST" (loops) */
                    inst_offset = offset;
                    break;

                default:
                    break;
            }

            offset += chunk_size;
        }
    }

    if (is_aifc) {
        if (!fver_found || !comm_found || !data_found)
            goto fail;
    } else if (is_aiff) {
        if (!comm_found || !data_found)
            goto fail;
    }


    /* read loop points */
    if (inst_offset && mark_offset) {
        int start_marker;
        int end_marker;

        /* use the 'sustain loop', if playMode=ForwardLooping */
        if (read_u16be(inst_offset + 0x08,sf) == 1) {
            start_marker = read_u16be(inst_offset + 0x0a,sf);
            end_marker = read_u16be(inst_offset + 0x0c,sf);
            /* check for sustain markers != 0 (invalid marker no) */
            if (start_marker && end_marker) {
                /* find start marker */
                loop_start = find_marker(sf, mark_offset, start_marker);
                loop_end = find_marker(sf, mark_offset, end_marker);

                /* find_marker is type uint32_t as the spec says that's the type
                 * of the position value, but it returns a -1 on error, and the
                 * loop_start and loop_end variables are int32_t, so the error
                 * will become apparent.
                 * We shouldn't have a loop point that overflows an int32_t anyway. */
                loop_flag = 1;
                if (loop_start==loop_end)
                    loop_flag = 0;
            }
        }

        /* Relic has "beg loop" "end loop" comments but no actual looping? */
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = sample_count;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_type;
    switch(coding_type) {
        case coding_XA:
            vgmstream->layout_type = layout_blocked_xa_aiff;
            /* AIFF XA can use sample rates other than 37800/18900 */
            /* some Crusader: No Remorse tracks have XA headers with incorrect 0xFF, rip bug/encoder feature? */
            break;

        case coding_RELIC: {
            int bitrate = read_u16be(start_offset, sf);
            start_offset += 0x02;

            vgmstream->codec_data = init_relic(channel_count, bitrate, sample_rate);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            vgmstream->sample_rate = 44100; /* fixed output */
            break;
        }

        default:
            vgmstream->layout_type = (channel_count > 1) ? layout_interleave : layout_none;
            vgmstream->interleave_block_size = interleave;
            break;
    }

    if (is_aifc)
        vgmstream->meta_type = meta_AIFC;
    else if (is_aiff)
        vgmstream->meta_type = meta_AIFF;


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
