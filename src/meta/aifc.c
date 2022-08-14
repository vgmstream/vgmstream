#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


/* for reading integers inexplicably packed into 80-bit ('double extended') floats, AKA:
 * "80 bit IEEE Standard 754 floating point number (Standard AppleNumeric Environment [SANE] data type Extended)" */
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

static int is_str(const char* str, int len, off_t offset, STREAMFILE* sf) {
    uint8_t buf[0x100];

    if (len == 0)
        len = strlen(str);

    if (len > sizeof(buf))
        return 0;
    if (read_streamfile(buf, offset, len, sf) != len)
        return 0;
    return memcmp(buf, str, len) == 0; /* memcmp to allow "AB\0\0" */
}


/* AIFF/AIFF-C (Audio Interchange File Format - Compressed) - Apple format, from Mac/3DO/other games */
VGMSTREAM* init_vgmstream_aifc(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset = 0, coef_offset = 0;
    uint32_t aifx_size, file_size;
    coding_t coding_type = 0;
    int channels = 0, sample_count = 0, sample_size = 0, sample_rate = 0;
    int interleave = 0;
    int loop_flag = 0;
    int32_t loop_start = 0, loop_end = 0;

    int is_aiff_ext = 0, is_aifc_ext = 0, is_aiff = 0, is_aifc = 0;
    int fver_found = 0, comm_found = 0, data_found = 0;
    off_t mark_offset = 0, inst_offset = 0;


    /* checks */
    if (!is_id32be(0x00,sf, "FORM"))
        goto fail;

    /* .aif: common (AIFF or AIFC), .aiff: common AIFF, .aifc: common AIFC
     * .laif/laiff/laifc: for plugins
     * .cbd2: M2 games
     * .bgm: Super Street Fighter II Turbo (3DO)
     * .acm: Crusader - No Remorse (SAT)
     * .adp: Sonic Jam (SAT)
     * .ai: Dragon Force (SAT)
     * (extensionless: Doom (3DO)
     * .fda: Homeworld 2 (PC)
     * .n64: Turok (N64) src
     * .pcm: Road Rash (SAT)
     * .wav: SimCity 3000 (Mac) (both AIFC and AIFF)
     * .lwav: for media players that may confuse this format with the usual RIFF WAVE file.
     * .xa: SimCity 3000 (Mac)
     */
    if (check_extensions(sf, "aif,laif,wav,lwav,")) {
        is_aifc_ext = 1;
        is_aiff_ext = 1;
    }
    else if (check_extensions(sf, "aifc,laifc,afc,cbd2,bgm,fda,n64,xa")) {
        is_aifc_ext = 1;
    }
    else if (check_extensions(sf, "aiff,laiff,acm,adp,ai,pcm")) {
        is_aiff_ext = 1;
    }
    else {
        goto fail;
    }

    file_size = get_streamfile_size(sf);
    aifx_size = read_u32be(0x04,sf);

    /* AIFF originally allowed only PCM (non-compressed) audio, so newer AIFC was added,
     * though some AIFF with other codecs exist */
    if (is_id32be(0x08,sf, "AIFC")) {
        if (!is_aifc_ext) goto fail;
        is_aifc = 1;
    }
    else if (is_id32be(0x08,sf, "AIFF")) {
        if (!is_aiff_ext) goto fail;
        is_aiff = 1;
    }
    else {
        goto fail;
    }

    /* some games have wonky sizes, selectively fix to catch bad rips and new mutations */
    if (file_size != aifx_size + 0x08) {
        if (is_aiff && file_size == aifx_size + 0x08 + 0x08)
            aifx_size += 0x08; /* [Psychic Force Puzzle Taisen CD2 (PS1)] */
    }

    if (aifx_size + 0x08 != file_size) {
        vgm_logi("AIFF: wrong reported size %x + 0x8 vs file size %x\n", aifx_size, file_size);
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
                case 0x46564552:    /* "FVER" (version info, required) */
                    if (fver_found) goto fail;
                    if (is_aiff) goto fail; /* plain AIFF shouldn't have */
                    fver_found = 1;

                    if (chunk_size != 4)
                        goto fail;
                    /* Version 1 of AIFF-C spec timestamp */
                    if (read_u32be(offset + 0x00,sf) != 0xA2805140)
                        goto fail;
                    break;

                case 0x434F4D4D:    /* "COMM" (main header) */
                    if (comm_found) goto fail;
                    comm_found = 1;

                    channels     = read_u16be(offset + 0x00,sf);
                    sample_count = read_u32be(offset + 0x02,sf); /* sample_frames in theory, depends on codec */
                    sample_size  = read_u16be(offset + 0x06,sf);
                    sample_rate  = read_f80be(offset + 0x08,sf);

                    if (is_aifc) {
                        uint32_t codec = read_u32be(offset + 0x12,sf);
                        /* followed by "pascal string": name size + human-readable name (full count padded to even size)  */

                        switch (codec) {
                            case 0x53445832:    /* "SDX2" [3DO games: Super Street Fighter II Turbo (3DO), etc] */
                                /* "2:1 Squareroot-Delta-Exact compression" */
                                coding_type = coding_SDX2;
                                interleave = 0x01;
                                break;

                            case 0x43424432:    /* "CBD2" [M2 (arcade 3DO) games: IMSA Racing (M2), etc] */
                                /* "2:1 Cuberoot-Delta-Exact compression" */
                                coding_type = coding_CBD2;
                                interleave = 0x01;
                                break;

                            case 0x41445034:    /* "ADP4" */
                                coding_type = coding_DVI_IMA_int;
                                if (channels != 1) break; /* don't know how stereo DVI is laid out */
                                break;

                            case 0x696D6134:    /* "ima4"  [Alida (PC), Lunar SSS (iOS)] */
                                /* "IMA 4:1FLLR" */
                                coding_type = coding_APPLE_IMA4;
                                interleave = 0x22;
                                sample_count = sample_count * ((interleave-0x2)*2);
                                break;

                            case 0x434F4D50: {  /* "COMP" (generic compression) */
                                uint8_t name_size = read_u8(offset + 0x16, sf);

                                if (is_str("Relic Codec v1.6", name_size, offset + 0x17, sf)) {
                                    coding_type = coding_RELIC;
                                    sample_count = sample_count * 512;
                                }
                                else {
                                    goto fail;
                                }
                                break;
                            }

                            case 0x56415043: {  /* "VAPC" [N64 (SDK mainly but apparently may exist in ROMs)] */
                                /* "VADPCM ~4-1" */
                                coding_type = coding_VADPCM;

                                /* N64 tools don't create FVER, but it's required by the spec (could skip the check though) */
                                fver_found = 1;
                                break;
                            }

                            default:
                                VGM_LOG("AIFC: unknown codec\n");
                                goto fail;
                        }
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
                            case 4: /* Crusader: No Remorse (SAT), Road Rash (3DO/SAT) */
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

                    /* 00: offset (for aligment, usually 0)
                     * 04: block size (ex. XA: 0x914) */
                    start_offset = offset + 0x08 + read_u32be(offset + 0x00,sf);
                    break;

                case 0x4D41524B:    /* "MARK" (loops) */
                    mark_offset = offset;
                    break;

                case 0x494E5354:    /* "INST" (loops) */
                    inst_offset = offset;
                    break;

                case 0x4150504C:    /* "APPL" (application specific) */
                    if (is_str("stoc", 0, offset + 0x00, sf)) {
                        uint8_t name_size = read_u8(offset + 0x4, sf);
                        off_t next_offset = offset + 0x04 + align_size_to_block(0x1 + name_size, 0x02);

                        /* chunks appears multiple times per substring */
                        if (is_str("VADPCMCODES", name_size, offset + 0x05, sf)) {
                            coef_offset = next_offset;
                        }
                        else if (is_str("VADPCMLOOPS", name_size, offset + 0x05, sf)) {
                            /* goes with inst (spec says app chunks have less priority than inst+mark loops) */
                            int version = read_u16be(next_offset + 0x00, sf);
                            int loops   = read_u16be(next_offset + 0x02, sf);
                            if (version != 1 || loops != 1) goto fail;

                            loop_start  = read_u32be(next_offset + 0x04, sf);
                            loop_end    = read_u32be(next_offset + 0x08, sf);
                            loop_flag   = read_s32be(next_offset + 0x08, sf) != 0; /*-1 = infinite */
                            /* 0x10: ADPCM state[16] (hists?) */
                        }
                        else {
                            VGM_LOG("AIFC: unknown APPL chunk\n");
                            goto fail;
                        }
                    }
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
                if (loop_start == loop_end)
                    loop_flag = 0;
            }
        }
    }
    if (!loop_flag && mark_offset) {
        int mark_count = read_u16be(mark_offset + 0x00,sf);

        /* use "beg/end" loop comments [Battle Tryst (Arcade)]
         * Relic codec has 3 "beg loop" "end loop" "start offset" comments, but  always  begin = 0 and end = -1 */
        if (mark_count == 2) {
            /* per mark: 
             * 00(2): id
             * 02(4): sample point
             * 06(1): string size
             * --(-): string (non-null terminated)
             * --(1): null terminator */
            /* simplified... */
            if (read_u32be(mark_offset + 0x09,sf) == 0x62656720 &&  /* "beg " */
                read_u32be(mark_offset + 0x19,sf) == 0x656E6420) {  /* "end " */
                loop_start = read_s32be(mark_offset + 0x04, sf);
                loop_end   = read_s32be(mark_offset + 0x14, sf);
                loop_flag = 1;
            }
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
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

            vgmstream->codec_data = init_relic(channels, bitrate, sample_rate);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            vgmstream->sample_rate = 44100; /* fixed output */
            break;
        }

        case coding_VADPCM:
            if (channels > 1) goto fail; /* unknown layout */
            if (coef_offset == 0) goto fail;

            vgmstream->layout_type = layout_none;
            {
                int version = read_u16be(coef_offset + 0x00, sf);
                int order   = read_u16be(coef_offset + 0x02, sf);
                int entries = read_u16be(coef_offset + 0x04, sf);
                if (version != 1) goto fail;

                vadpcm_read_coefs_be(vgmstream, sf, coef_offset + 0x06, order, entries, 0);
            }

            //vgmstream->num_samples = vadpcm_bytes_to_samples(data_size, channels); /* unneeded */
            break;

        default:
            vgmstream->layout_type = (channels > 1) ? layout_interleave : layout_none;
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
