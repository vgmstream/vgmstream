#include "meta.h"
#include "../coding/coding.h"

typedef enum { ADX, HCA, VAG, RIFF, CWAV, DSP, CWAC, M4A } awb_type;

static void load_awb_name(STREAMFILE* sf, STREAMFILE* sf_acb, VGMSTREAM* vgmstream, int waveid);

/* AFS2/AWB (Atom Wave Bank) - CRI container of streaming audio, often together with a .acb cue sheet */
VGMSTREAM* init_vgmstream_awb(STREAMFILE* sf) {
    return init_vgmstream_awb_memory(sf, NULL);
}

VGMSTREAM* init_vgmstream_awb_memory(STREAMFILE* sf, STREAMFILE* sf_acb) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t offset, subfile_offset, subfile_next;
    size_t subfile_size;
    int total_subsongs, target_subsong = sf->stream_index;
    //uint32_t flags;
    uint8_t offset_size;
    uint16_t alignment, subkey;
    awb_type type;
    const char* extension = NULL;
    int waveid;


    /* checks
     * .awb: standard
     * .afs2: sometimes [Okami HD (PS4)] */
    if (!check_extensions(sf, "awb,afs2"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x41465332) /* "AFS2" */
        goto fail;

    //flags = read_32bitLE(0x08,sf);
    /* 0x04(1): version? 0x01=common, 0x02=2018+ (no apparent differences) */
    offset_size = read_u8(0x05,sf);
    /* 0x06(2): always 0x0002? */
    total_subsongs = read_s32le(0x08,sf);
    alignment = read_u16le(0x0c,sf);
    subkey    = read_u16le(0x0e,sf);

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > total_subsongs || total_subsongs <= 0) goto fail;

    offset = 0x10;

    /* id(?) table: read target */
    {
        off_t waveid_offset = offset + (target_subsong-1) * 0x02;

        waveid = read_u16le(waveid_offset,sf);

        offset += total_subsongs * 0x02;
    }

    /* offset table: find target */
    {
        off_t file_size = get_streamfile_size(sf);

        /* last sub-offset is always file end, so table entries = total_subsongs+1 */
        offset += (target_subsong-1) * offset_size;

        switch(offset_size) {
            case 0x04: /* common */
                subfile_offset  = read_u32le(offset+0x00,sf);
                subfile_next    = read_u32le(offset+0x04,sf);
                break;
            case 0x02: /* mostly sfx in .acb */
                subfile_offset  = read_u16le(offset+0x00,sf);
                subfile_next    = read_u16le(offset+0x02,sf);
                break;
            default:
                VGM_LOG("AWB: unknown offset size\n");
                goto fail;
        }

        /* offset are absolute but sometimes misaligned (specially first that just points to offset table end) */
        subfile_offset += (subfile_offset % alignment) ?
                alignment - (subfile_offset % alignment) : 0;
        subfile_next   += (subfile_next % alignment) && subfile_next < file_size ?
                alignment - (subfile_next % alignment) : 0;
        subfile_size = subfile_next - subfile_offset;
    }

    //;VGM_LOG("AWB: subfile offset=%lx + %x\n", subfile_offset, subfile_size);

    /* autodetect as there isn't anything, plus can mix types
     * (waveid<>codec info is usually in the companion .acb) */
    if (read_u16be(subfile_offset, sf) == 0x8000) { /* ADX id (type 0) */
        type = ADX;
        extension = "adx";
    }
    else if ((read_u32be(subfile_offset,sf) & 0x7f7f7f7f) == 0x48434100) { /* "HCA\0" (type 2=HCA, 6=HCA-MX) */
        type = HCA;
        extension = "hca";
    }
    else if (read_u32be(subfile_offset,sf) == 0x56414770) { /* "VAGp" (type 7=VAG, 10=HEVAG) */
        type = VAG;
        extension = "vag";
    }
    else if (read_u32be(subfile_offset,sf) == 0x52494646) { /* "RIFF" (type 8=ATRAC3, 11=ATRAC9) */
        type = RIFF;
        extension = "wav";
        subfile_size = read_u32le(subfile_offset + 0x04,sf) + 0x08; /* rough size, use RIFF's */
    }
    else if (read_u32be(subfile_offset,sf) == 0x43574156) { /* "CWAV" (type 9) */
        type = CWAV;
        extension = "bcwav";
    }
    else if (read_u32be(subfile_offset + 0x08,sf) >= 8000 &&
             read_u32be(subfile_offset + 0x08,sf) <= 48000 &&
             read_u16be(subfile_offset + 0x0e,sf) == 0 &&
             read_u32be(subfile_offset + 0x18,sf) == 2 &&
             read_u32be(subfile_offset + 0x50,sf) == 0) { /*  probably should call some check function (type 13) */
        type = DSP;
        extension = "dsp";
    }
    else if (is_id32be(subfile_offset,sf, "CWAC")) { /* type 13 again */
        type = CWAC;
        extension = "dsp";
    }
    else if (read_u32be(subfile_offset+0x00,sf) == 0x00000018 && 
             read_u32be(subfile_offset+0x04,sf) == 0x66747970) { /* chunk size + "ftyp" (type 19) */
        type = M4A;
        extension = "m4a";
    }
    else {
        VGM_LOG("AWB: unknown codec\n");
        goto fail;
    }


    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, extension);
    if (!temp_sf) goto fail;

    switch(type) {
        case HCA: /* most common */
            vgmstream = init_vgmstream_hca_subkey(temp_sf, subkey);
            if (!vgmstream) goto fail;
            break;
        case ADX: /* Okami HD (PS4) */
            vgmstream = init_vgmstream_adx_subkey(temp_sf, subkey);
            if (!vgmstream) goto fail;
            break;
        case VAG: /* Ukiyo no Roushi (Vita) */
            vgmstream = init_vgmstream_vag(temp_sf);
            if (!vgmstream) goto fail;
            break;
        case RIFF: /* Ukiyo no Roushi (Vita) */
            vgmstream = init_vgmstream_riff(temp_sf);
            if (!vgmstream) goto fail;
            break;
        case CWAV: /* Sonic: Lost World (3DS) */
            vgmstream = init_vgmstream_rwsd(temp_sf);
            if (!vgmstream) goto fail;
            break;
        case DSP: /* Sonic: Lost World (WiiU) */
            vgmstream = init_vgmstream_ngc_dsp_std(temp_sf);
            if (!vgmstream) goto fail;
            break;
        case CWAC: /* Mario & Sonic at the Rio 2016 Olympic Games (WiiU) */
            vgmstream = init_vgmstream_dsp_cwac(temp_sf);
            if (!vgmstream) goto fail;
            break;
#ifdef VGM_USE_FFMPEG
        case M4A: /* Imperial SaGa Eclipse (Browser) */
            vgmstream = init_vgmstream_mp4_aac_ffmpeg(temp_sf);
            if (!vgmstream) goto fail;
            break;
#endif
        default:
            goto fail;
    }

    vgmstream->num_streams = total_subsongs;

    /* try to load cue names */
    load_awb_name(sf, sf_acb, vgmstream,  waveid);

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}


static void load_awb_name(STREAMFILE* sf, STREAMFILE* sf_acb, VGMSTREAM* vgmstream, int waveid) {
    int is_memory = (sf_acb != NULL);
    int port = 0;

    /* .acb is passed when loading memory .awb inside .acb */
    if (!is_memory) {
        /* load companion .acb using known pairs */ //todo improve, see xsb code
        char filename[PATH_LIMIT];
        int len_name, len_cmp;

        /* try parsing TXTM if present */
        sf_acb = read_filemap_file_pos(sf, 0, &port);

        /* try (name).awb + (name).awb */
        if (!sf_acb) {
            sf_acb = open_streamfile_by_ext(sf, "acb");
        }

        /* try (name)_streamfiles.awb + (name).acb */
        if (!sf_acb) {
            char *cmp = "_streamfiles";
            get_streamfile_basename(sf, filename, sizeof(filename));
            len_name = strlen(filename);
            len_cmp = strlen(cmp);

            if (len_name > len_cmp && strcmp(filename + len_name - len_cmp, cmp) == 0) {
                filename[len_name - len_cmp] = '\0';
                strcat(filename, ".acb");
                sf_acb = open_streamfile_by_filename(sf, filename);
            }
        }

        /* try (name)_STR.awb + (name).acb */
        if (!sf_acb) {
            char *cmp = "_STR";
            get_streamfile_basename(sf, filename, sizeof(filename));
            len_name = strlen(filename);
            len_cmp = strlen(cmp);

            if (len_name > len_cmp && strcmp(filename + len_name - len_cmp, cmp) == 0) {
                filename[len_name - len_cmp] = '\0';
                strcat(filename, ".acb");
                sf_acb = open_streamfile_by_filename(sf, filename);
            }
        }

		/* probably loaded */
        load_acb_wave_name(sf_acb, vgmstream, waveid, port, is_memory);

        close_streamfile(sf_acb);
    }
    else {
        load_acb_wave_name(sf_acb, vgmstream, waveid, port, is_memory);
    }
}
