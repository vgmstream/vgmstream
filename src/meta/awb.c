#include "meta.h"
#include "awb_aac_encryption_streamfile.h"
#include "../coding/coding.h"
#include "../util/companion_files.h"
#include "../util/cri_keys.h"

typedef struct {
    VGMSTREAM* (*init_vgmstream)(STREAMFILE* sf);
    VGMSTREAM* (*init_vgmstream_subkey)(STREAMFILE* sf, uint16_t subkey);
    const char* extension;
    bool load_loops;
    bool use_riff_size;
} meta_info_t;

static bool load_meta_type(meta_info_t* meta, STREAMFILE* sf, uint32_t subfile_offset);
static void load_acb_info(STREAMFILE* sf, STREAMFILE* sf_acb, VGMSTREAM* vgmstream, int waveid, bool load_loops);
static uint64_t load_keycode(STREAMFILE* sf);

/* AFS2/AWB (Atom Wave Bank) - CRI container of streaming audio, often together with a .acb cue sheet */
VGMSTREAM* init_vgmstream_awb(STREAMFILE* sf) {
    return init_vgmstream_awb_memory(sf, NULL);
}

VGMSTREAM* init_vgmstream_awb_memory(STREAMFILE* sf, STREAMFILE* sf_acb) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    int target_subsong = sf->stream_index;

    /* checks */
    if (!is_id32be(0x00,sf, "AFS2"))
        return NULL;
    /* .awb: standard
     * .afs2: sometimes [Okami HD (PS4)] 
     * .awx: Dariusburst - Chronicle Saviors (multi) */
    if (!check_extensions(sf, "awb,afs2,awx"))
        return NULL;

    uint32_t subfile_offset, subfile_next, subfile_size;
    int waveid;

    // 0x04(1): version? 0x01=common, 0x02=2018+ (no apparent differences)
    uint8_t offset_size         = read_u8   (0x05,sf);
    uint16_t waveid_alignment   = read_u16le(0x06,sf); // usually 0x02, rarely 0x04 [Voice of Cards: The Beasts of Burden (Switch)]
    int total_subsongs          = read_s32le(0x08,sf);
    uint16_t offset_alignment   = read_u16le(0x0c,sf);
    uint16_t subkey             = read_u16le(0x0e,sf);

    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong > total_subsongs || total_subsongs <= 0) return NULL;

    uint32_t offset = 0x10;

    /* id table: read target */
    {
        uint32_t waveid_offset = offset + (target_subsong-1) * waveid_alignment;

        waveid = read_u16le(waveid_offset,sf);

        offset += total_subsongs * waveid_alignment;
    }

    /* offset table: find target */
    {
        uint32_t file_size = get_streamfile_size(sf);

        /* last sub-offset is always file end, so table entries = total_subsongs+1 */
        offset += (target_subsong - 1) * offset_size;

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
                vgm_logi("AWB: unknown offset size (report)\n");
                return NULL;
        }

        /* offset are absolute but sometimes misaligned (specially first that just points to offset table end) */
        subfile_offset += (subfile_offset % offset_alignment) ?
                offset_alignment - (subfile_offset % offset_alignment) : 0;
        subfile_next   += (subfile_next % offset_alignment) && subfile_next < file_size ?
                offset_alignment - (subfile_next % offset_alignment) : 0;
        subfile_size = subfile_next - subfile_offset;
    }

    //;VGM_LOG("awb: subfile offset=%x + %x\n", subfile_offset, subfile_size);

    /* handle subfile */
    {
        meta_info_t meta = {0};

        bool meta_ok = load_meta_type(&meta, sf, subfile_offset);
        if (!meta_ok) { 
            // try encrypted meta (loads key after reguylar cases since it's uncommon)
            uint64_t keycode = load_keycode(sf);
            if (keycode) {
#ifdef VGM_USE_FFMPEG
                meta.init_vgmstream = init_vgmstream_mp4_aac_ffmpeg; // Final Fantasy Digital Card Game (Browser)
                meta.extension = "m4a"; // TODO improve detection (only known to be used for .m4a)
                meta_ok = true;

                temp_sf = setup_awb_aac_encryption_streamfile(sf, subfile_offset, subfile_size, meta.extension, keycode);
                if (!temp_sf) goto fail;
#endif
            }
        }

        if (!meta_ok) {
            vgm_logi("AWB: unknown codec (report)\n");
            goto fail;
        }

        if (meta.use_riff_size) {
            subfile_size = read_u32le(subfile_offset + 0x04,sf) + 0x08;
        }

        if (!temp_sf) {
            temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, meta.extension);
            if (!temp_sf) goto fail;
        }

        if (meta.init_vgmstream_subkey)
            vgmstream = meta.init_vgmstream_subkey(temp_sf, subkey);
        else
            vgmstream = meta.init_vgmstream(temp_sf);
        if (!vgmstream) goto fail;

        vgmstream->num_streams = total_subsongs;

        /* try to load cue names+etc */
        load_acb_info(sf, sf_acb, vgmstream, waveid, meta.load_loops);
    }

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}


/* autodetect as waveid<>codec is found in .acb (values/extension from CriAtomCraft decompilations)
 * 00: ADX (.adx) [Gunhound EX (PSP), Persona 5 (PS3), Shin Megami Tensei V: Vengeance (PS4)]
 * 01: PCM? (.swlpcm?)
 * 02: HCA-MX? (.hca) [common]
 * 03: alt ADX?
 * 04: Wii DSP? (.wiiadpcm?)
 * 05: NDS DSP? (.dsadpcm)
 * 06: HCA-MX (.hcamx) [common]
 * 07: VAG (.vag) [Ukiyo no Roushi (Vita)]
 * 08: ATRAC3 (.at3) [Ukiyo no Shishi (PS3)]
 * 09: CWAV (.3dsadpcm) [Sonic: Lost World (3DS)]
 * 10: HEVAG (.vag) [Ukiyo no Roushi (Vita)]
 * 11: ATRAC9 (.at9) [Ukiyo no Roushi (Vita)]
 * 12: X360 XMA? (.xma2?)
 * 13: DSP (.wiiuadpcm?) [Sonic: Lost World (WiiU)]
 * 13: CWAC DSP (.wiiuadpcm?) [Mario & Sonic at the Rio 2016 Olympic Games (WiiU)]
 * 14: PS4 HEVAG?
 * 18: PS4 ATRAC9 (.at9) [13 Sentinels (PS4)]
 * 19: AAC M4A (.m4a) [Imperial SaGa Eclipse (Browser)]
 * 24: Switch Opus (.switchopus) [Super Mario RPG (Switch)]
 */
static bool load_meta_type(meta_info_t* meta, STREAMFILE* sf, uint32_t subfile_offset) {

    if (read_u16be(subfile_offset, sf) == 0x8000) {
        meta->init_vgmstream_subkey = init_vgmstream_adx_subkey;
        meta->extension = "adx";
        return true;
    }

    if ((read_u32be(subfile_offset,sf) & 0x7f7f7f7f) == get_id32be("HCA\0")) {
        meta->init_vgmstream_subkey = init_vgmstream_hca_subkey;
        meta->extension = "hca";
        return true;
    }

    if (is_id32be(subfile_offset,sf, "VAGp")) {
        meta->init_vgmstream = init_vgmstream_vag;
        meta->extension = "vag";
        return true;
    }

    if (is_id32be(subfile_offset,sf, "RIFF")) {
        meta->init_vgmstream = init_vgmstream_riff;
        meta->extension = "wav";
        meta->use_riff_size = true; // padded size, use RIFF's
        return true;
    }

    if (is_id32be(subfile_offset,sf, "CWAV")) {
        meta->init_vgmstream = init_vgmstream_bcwav;
        meta->extension = "bcwav";
        return true;
    }

    // TODO: call some check function?
    if (read_u32be(subfile_offset + 0x08,sf) >= 8000
        && read_u32be(subfile_offset + 0x08,sf) <= 48000
        && read_u16be(subfile_offset + 0x0e,sf) == 0
        && read_u32be(subfile_offset + 0x18,sf) == 2
        && read_u32be(subfile_offset + 0x50,sf) == 0) {
        meta->init_vgmstream = init_vgmstream_ngc_dsp_std;
        meta->extension = "dsp";
        return true;
    }

    if (is_id32be(subfile_offset,sf, "CWAC")) {
        meta->init_vgmstream = init_vgmstream_dsp_cwac;
        meta->extension = "dsp";
        return true;
    }

#ifdef VGM_USE_FFMPEG
    if (read_u32be(subfile_offset + 0x00,sf) == 0x00000018
        && is_id32be(subfile_offset + 0x04,sf, "ftyp")) {
        meta->init_vgmstream = init_vgmstream_mp4_aac_ffmpeg;
        meta->extension = "m4a";
        return true;
    }
#endif

    if (read_u32be(subfile_offset + 0x00,sf) == 0x01000080) {
        meta->init_vgmstream = init_vgmstream_opus_std;
        meta->extension = "opus";
        meta->load_loops = true; // loops not in Opus but in .acb (rare)
        return true;
    }

    return false;
}

// TODO unify, from HCA
static uint64_t load_keycode(STREAMFILE* sf) {
    // fully encrypted data, try to read keyfile (rare) 
    uint8_t keybuf[20+1] = {0}; /* max keystring 20, +1 extra null */
    uint32_t key_size = read_key_file(keybuf, sizeof(keybuf) - 1, sf);

    uint64_t keycode = 0;
    bool is_keystring = cri_key9_valid_keystring(keybuf, key_size);
    if (is_keystring) { /* number */
        const char* keystring = (const char*)keybuf;
        keycode = strtoull(keystring, NULL, 10);
    }
    else if (key_size == 0x08) { /* hex */
        keycode = get_u64be(keybuf+0x00);
    }

    return keycode;
}

static void load_acb_info(STREAMFILE* sf, STREAMFILE* sf_acb, VGMSTREAM* vgmstream, int waveid, bool load_loops) {
    bool is_memory = (sf_acb != NULL);
    int port = 0;

    /* .acb is passed when loading memory .awb inside .acb */
    if (!is_memory) {
        /* load companion .acb using known pairs */ //todo improve, see xsb code
        char filename[PATH_LIMIT];
        int len_name, len_cmp;

        /* try parsing TXTM if present */
        sf_acb = read_filemap_file_pos(sf, 0, &port);

        /* try (name).awb + (name).acb (most common) */
        if (!sf_acb) {
            sf_acb = open_streamfile_by_ext(sf, "acb");
        }

        /* try (name).awx + (name).acx, exclusive to Dariusburst console games. */
        if (!sf_acb && check_extensions(sf, "awx")) {
            sf_acb = open_streamfile_by_ext(sf, "acx");
        }

        /* try (name)_streamfiles.awb + (name).acb (sometimes) */
        if (!sf_acb && check_extensions(sf, "awb")) {
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
        if (!sf_acb && check_extensions(sf, "awb")) {
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
        load_acb_wave_info(sf_acb, vgmstream, waveid, port, is_memory, load_loops);

        close_streamfile(sf_acb);
    }
    else {
        load_acb_wave_info(sf_acb, vgmstream, waveid, port, is_memory, load_loops);
    }
}
