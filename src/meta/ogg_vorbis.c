#include <stdio.h>
#include <string.h>
#include "meta.h"
#include "../coding/coding.h"
#include "ogg_vorbis_streamfile.h"


#ifdef VGM_USE_VORBIS
static VGMSTREAM* _init_vgmstream_ogg_vorbis(STREAMFILE* sf);
static VGMSTREAM* _init_vgmstream_ogg_vorbis_config(STREAMFILE* sf, off_t start, const ogg_vorbis_meta_info_t* ovmi);
#endif

VGMSTREAM* init_vgmstream_ogg_vorbis(STREAMFILE* sf) {
#ifdef VGM_USE_VORBIS
    return _init_vgmstream_ogg_vorbis(sf);
#else
    return NULL;
#endif
}

VGMSTREAM* init_vgmstream_ogg_vorbis_config(STREAMFILE* sf, off_t start, const ogg_vorbis_meta_info_t* ovmi) {
#ifdef VGM_USE_VORBIS
    return _init_vgmstream_ogg_vorbis_config(sf, start, ovmi);
#else
    return NULL;
#endif
}

#ifdef VGM_USE_VORBIS
static void um3_ogg_decryption_callback(void* ptr, size_t size, size_t nmemb, void* datasource) {
    uint8_t *ptr8 = ptr;
    size_t bytes_read = size * nmemb;
    ogg_vorbis_io *io = datasource;
    int i;

    /* first 0x800 bytes are xor'd */
    if (io->offset < 0x800) {
        int num_crypt = 0x800 - io->offset;
        if (num_crypt > bytes_read)
            num_crypt = bytes_read;

        for (i = 0; i < num_crypt; i++)
            ptr8[i] ^= 0xff;
    }
}

static void kovs_ogg_decryption_callback(void* ptr, size_t size, size_t nmemb, void* datasource) {
    uint8_t *ptr8 = ptr;
    size_t bytes_read = size * nmemb;
    ogg_vorbis_io *io = datasource;
    int i;

    /* first 0x100 bytes are xor'd */
    if (io->offset < 0x100) {
        int max_offset = io->offset + bytes_read;
        if (max_offset > 0x100)
            max_offset = 0x100;

        for (i = io->offset; i < max_offset; i++) {
            ptr8[i-io->offset] ^= i;
        }
    }
}

static void psychic_ogg_decryption_callback(void* ptr, size_t size, size_t nmemb, void* datasource) {
    static const uint8_t key[6] = {
            0x23,0x31,0x20,0x2e,0x2e,0x28
    };
    uint8_t *ptr8 = ptr;
    size_t bytes_read = size * nmemb;
    ogg_vorbis_io *io = datasource;
    int i;

    //todo incorrect, picked value changes (fixed order for all files), or key is bigger
    /* bytes add key that changes every 0x64 bytes */
    for (i = 0; i < bytes_read; i++) {
        int pos = (io->offset + i) / 0x64;
        ptr8[i] += key[pos % sizeof(key)];
    }
}

static void rpgmvo_ogg_decryption_callback(void* ptr, size_t size, size_t nmemb, void* datasource) {
    static const uint8_t header[16] = { /* OggS, packet type, granule, stream id(empty) */
            0x4F,0x67,0x67,0x53,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    };
    uint8_t *ptr8 = ptr;
    size_t bytes_read = size*nmemb;
    ogg_vorbis_io *io = datasource;
    int i;

    /* first 0x10 are xor'd, but header can be easily reconstructed
     * (key is also in (game)/www/data/System.json "encryptionKey") */
    for (i = 0; i < bytes_read; i++) {
        if (io->offset+i < 0x10) {
            ptr8[i] = header[(io->offset + i) % 16];

            /* last two bytes are the stream id, get from next OggS */
            if (io->offset+i == 0x0e)
                ptr8[i] = read_8bit(0x58, io->streamfile);
            if (io->offset+i == 0x0f)
                ptr8[i] = read_8bit(0x59, io->streamfile);
        }
    }
}

static void at4_ogg_decryption_callback(void* ptr, size_t size, size_t nmemb, void* datasource) {
    static const uint8_t af4_key[0x10] = {
            0x00,0x0E,0x08,0x1E, 0x18,0x37,0x12,0x00, 0x48,0x87,0x46,0x0B, 0x9C,0x68,0xA8,0x4B
    };
    uint8_t *ptr8 = ptr;
    size_t bytes_read = size * nmemb;
    ogg_vorbis_io *io = datasource;
    int i;

    for (i = 0; i < bytes_read; i++) {
        ptr8[i] -= af4_key[(io->offset + i) % sizeof(af4_key)];
    }
}


static const uint32_t xiph_mappings[] = {
        0,
        mapping_MONO,
        mapping_STEREO,
        mapping_2POINT1_xiph,
        mapping_QUAD,
        mapping_5POINT0_xiph,
        mapping_5POINT1,
        mapping_7POINT0,
        mapping_7POINT1,
};


static VGMSTREAM* _init_vgmstream_ogg_vorbis_cfg_ovmi(STREAMFILE* sf, ogg_vorbis_io_config_data* cfg, ogg_vorbis_meta_info_t* ovmi) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;


    if (cfg->is_encrypted) {
        temp_sf = setup_ogg_vorbis_streamfile(sf, cfg);
        if (!temp_sf) goto fail;
    }

    if (ovmi->meta_type == 0) {
        if (cfg->is_encrypted || ovmi->decryption_callback != NULL)
            ovmi->meta_type = meta_OGG_encrypted;
        else
            ovmi->meta_type = meta_OGG_VORBIS;
    }

    vgmstream = _init_vgmstream_ogg_vorbis_config(temp_sf != NULL ? temp_sf : sf, cfg->start, ovmi);

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    return NULL;
}

static int _init_vgmstream_ogg_vorbis_tests(STREAMFILE* sf, ogg_vorbis_io_config_data* cfg, ogg_vorbis_meta_info_t* ovmi) {


    /* standard  */
    if (is_id32be(0x00,sf, "OggS")) {

        /* .ogg: common, .logg: renamed for plugins
         * .adx: KID games [Remember11 (PC)]
         * .rof: The Rhythm of Fighters (Mobile)
         * .acm: Planescape Torment Enhanced Edition (PC)
         * .sod: Zone 4 (PC)
         * .msa: Metal Slug Attack (Mobile)
         * .bin/lbin: Devil May Cry 3: Special Edition (PC) */
        if (check_extensions(sf,"ogg,logg,adx,rof,acm,sod,msa,bin,lbin"))
            return 1;
        /* ignore others to allow stuff like .sngw */
    }

    /* Koei Tecmo PC games */
    if (is_id32be(0x00,sf, "KOVS")) {
        ovmi->loop_start = read_s32le(0x08,sf);
        ovmi->loop_flag = (ovmi->loop_start != 0);
        ovmi->decryption_callback = kovs_ogg_decryption_callback;
        ovmi->meta_type = meta_OGG_KOVS;

        cfg->start = 0x20;

        /* .kvs: Atelier Sophie (PC)
         * .kovs: header id only? */
        if (!check_extensions(sf,"kvs,kovs"))
            goto fail;

        return 1;
    }
    
    /* [RPG Maker MV (PC), RPG Maker MZ (PC)] */
    if (is_id64be(0x00,sf, "RPGMV\0\0\0")) {
        ovmi->decryption_callback = rpgmvo_ogg_decryption_callback;

        cfg->start = 0x10;

        /* .rpgmvo: RPG Maker MV games (PC), .ogg_: RPG Maker MZ games (PC) */
        if (!check_extensions(sf,"rpgmvo,ogg_"))
            goto fail;

        return 1;
    }

    /* L2SD [Lineage II Chronicle 4 (PC)] */
    if (is_id32be(0x00,sf, "L2SD")) { 
        cfg->is_header_swap = 1;
        cfg->is_encrypted = 1;

        if (!check_extensions(sf,"ogg,logg"))
            goto fail;

        return 1;
    }

    /* NIS's "OggS" XOR'ed + bitswapped [Ys VIII (PC), Yomawari: Midnight Shadows (PC)] */
    if (read_u32be(0x00,sf) == 0x048686C5) { 
        cfg->key[0] = 0xF0;
        cfg->key_len = 0x01;
        cfg->is_nibble_swap = 1;
        cfg->is_encrypted = 1;

        /* .bgm: Yomawari */
        if (!check_extensions(sf,"ogg,logg,bgm"))
            goto fail;

        return 1;
    }

    /* Psychic Software [Darkwind: War on Wheels (PC)] */
    if (read_u32be(0x00,sf) == 0x2c444430) {
        ovmi->decryption_callback = psychic_ogg_decryption_callback;

        if (!check_extensions(sf,"ogg,logg"))
            goto fail;

        return 1;
    }
        
    /* null id + check next page [Yuppie Psycho (PC)] */
    if (read_u32be(0x00,sf) == 0x00000000 && is_id32be(0x3a,sf, "OggS")) {
        cfg->is_header_swap = 1;
        cfg->is_encrypted = 1;

        if (!check_extensions(sf,"ogg,logg"))
            goto fail;

        return 1;
    }

    /* random(?) id + check next page [Tobi Tsukihime (PC)] */
    if (!is_id32be(0x00,sf, "OggS") && is_id32be(0x3a,sf, "OggS")) {
        cfg->is_header_swap = 1;
        cfg->is_encrypted = 1;

        if (!check_extensions(sf,"ogg,logg"))
            goto fail;

        return 1;
    }

    /* encrypted [Adventure Field 4 (PC)] */
    if (read_u32be(0x00,sf) == 0x4F756F71) {
        ovmi->decryption_callback = at4_ogg_decryption_callback; //TODO replace with generic descryption?

        if (!check_extensions(sf,"ogg,logg"))
            goto fail;

        return 1;
    }

    /* .gwm: Adagio: Cloudburst (PC) */
    if (read_u32be(0x00,sf) == 0x123A3A0E) { 
        cfg->key[0] = 0x5D;
        cfg->key_len = 1;
        cfg->is_encrypted = 1;

        if (!check_extensions(sf,"gwm"))
            goto fail;

        return 1;
    }

    /* .mus: Redux - Dark Matters (PC) */
    if (read_u32be(0x00,sf) == 0x6C381C21) { 
        static const uint8_t mus_key[16] = {
                0x21,0x4D,0x6F,0x01,0x20,0x4C,0x6E,0x02,0x1F,0x4B,0x6D,0x03,0x20,0x4C,0x6E,0x02
        };
        cfg->key_len = sizeof(mus_key);
        memcpy(cfg->key, mus_key, cfg->key_len);
        cfg->is_header_swap = 1; /* decrypted header gives "Mus " */
        cfg->is_encrypted = 1;

        if (!check_extensions(sf,"mus"))
            goto fail;

        return 1;
    }


    /***************************************/
    /* harder to check (could be improved) */

    /* .isd: Inti Creates PC games */
    if (check_extensions(sf,"isd")) {
        const char* isl_name = NULL;

        /* check various encrypted "OggS" values */
        if (read_u32be(0x00,sf) == 0xAF678753) { /* Azure Striker Gunvolt (PC) */
            static const uint8_t isd_gv_key[16] = {
                    0xe0,0x00,0xe0,0x00,0xa0,0x00,0x00,0x00,0xe0,0x00,0xe0,0x80,0x40,0x40,0x40,0x00
            };
            cfg->key_len = sizeof(isd_gv_key);
            memcpy(cfg->key, isd_gv_key, cfg->key_len);
            isl_name = "GV_steam.isl";
        }
        else if (read_u32be(0x00,sf) == 0x0FE787D3) { /* Mighty Gunvolt (PC) */
            static const uint8_t isd_mgv_key[120] = {
                    0x40,0x80,0xE0,0x80,0x40,0x40,0xA0,0x00,0xA0,0x40,0x00,0x80,0x00,0x40,0xA0,0x00,
                    0xC0,0x40,0xE0,0x00,0x60,0x40,0x80,0x00,0xA0,0x00,0xE0,0x00,0x60,0x40,0xC0,0x00,
                    0xA0,0x40,0xC0,0x80,0xE0,0x00,0x60,0x00,0x00,0x40,0x00,0x80,0xE0,0x80,0x40,0x00,
                    0xA0,0x80,0xA0,0x80,0x80,0xC0,0x60,0x00,0xA0,0x00,0xA0,0x80,0x40,0x80,0x60,0x00,
                    0x40,0xC0,0x20,0x00,0x20,0xC0,0x00,0x00,0x00,0xC0,0x20,0x00,0xC0,0xC0,0x60,0x00,
                    0xE0,0xC0,0x80,0x80,0x20,0x00,0x60,0x00,0xE0,0xC0,0xC0,0x00,0x20,0xC0,0xC0,0x00,
                    0x60,0x00,0xE0,0x80,0x00,0xC0,0x00,0x00,0x60,0x80,0x40,0x80,0x20,0x80,0x20,0x00,
                    0x80,0x40,0xE0,0x00,0x20,0x00,0x20,0x00,
            };
            cfg->key_len = sizeof(isd_mgv_key);
            memcpy(cfg->key, isd_mgv_key, cfg->key_len);
            isl_name = "MGV_steam.isl";
        }
        else if (read_u32be(0x00,sf) == 0x0FA74753) { /* Blaster Master Zero (PC) */
            static const uint8_t isd_bmz_key[120] = {
                    0x40,0xC0,0x20,0x00,0x40,0xC0,0xC0,0x00,0x00,0x80,0xE0,0x80,0x80,0x40,0x20,0x00,
                    0x60,0xC0,0xC0,0x00,0xA0,0x80,0x60,0x00,0x40,0x40,0x20,0x00,0x60,0x40,0xC0,0x00,
                    0x60,0x80,0xC0,0x80,0x40,0xC0,0x00,0x00,0xA0,0xC0,0x80,0x80,0x60,0x80,0xA0,0x00,
                    0x40,0x80,0x60,0x00,0x20,0x00,0xC0,0x00,0x60,0x00,0xA0,0x80,0x40,0x40,0xA0,0x00,
                    0x40,0x40,0xC0,0x80,0x00,0x80,0x60,0x00,0x80,0xC0,0xA0,0x00,0xE0,0x40,0xC0,0x00,
                    0x20,0x80,0xE0,0x00,0x40,0xC0,0xA0,0x00,0xE0,0xC0,0xC0,0x80,0xE0,0x80,0xC0,0x00,
                    0x40,0x40,0x00,0x00,0x20,0x40,0x80,0x00,0xE0,0x80,0x20,0x80,0x40,0x80,0xE0,0x00,
                    0xA0,0x00,0xC0,0x80,0xE0,0x00,0x20,0x00
            };
            cfg->key_len = sizeof(isd_bmz_key);
            memcpy(cfg->key, isd_bmz_key, cfg->key_len);
            isl_name = "output.isl";
        }
        else {
            goto fail;
        }

        cfg->is_encrypted = 1;

        /* .isd have companion files in the prev folder:
         * - .ish: constant id/names (not always)
         * - .isf: format table, ordered like file id/numbers, 0x18 header with:
         *   0x00(2): ?, 0x02(2): channels, 0x04: sample rate, 0x08: skip samples (in PCM bytes), always 32000
         *   0x0c(2): PCM block size, 0x0e(2): PCM bps, 0x10: null, 0x18: samples (in PCM bytes)
         * - .isl: looping table (encrypted like the files) */
        if (isl_name) {
            STREAMFILE* sf_isl = NULL;

            sf_isl = open_streamfile_by_filename(sf, isl_name);

            if (!sf_isl) {
                /* try in ../(file) too since that's how the .isl is stored on disc */
                char isl_path[PATH_LIMIT];
                snprintf(isl_path, sizeof(isl_path), "../%s", isl_name);
                sf_isl = open_streamfile_by_filename(sf, isl_path);
            }

            if (sf_isl) {
                STREAMFILE* dec_sf = NULL;

                dec_sf = setup_ogg_vorbis_streamfile(sf_isl, cfg);
                if (dec_sf) {
                    off_t loop_offset;
                    char basename[PATH_LIMIT];

                    /* has a bunch of tables then a list with file names without extension and loops */
                    loop_offset = read_32bitLE(0x18, dec_sf);
                    get_streamfile_basename(sf, basename, sizeof(basename));

                    while (loop_offset < get_streamfile_size(dec_sf)) {
                        char testname[0x20];

                        read_string(testname, sizeof(testname), loop_offset+0x2c, dec_sf);
                        if (strcmp(basename, testname) == 0) {
                            ovmi->loop_start = read_32bitLE(loop_offset+0x1c, dec_sf);
                            ovmi->loop_end   = read_32bitLE(loop_offset+0x20, dec_sf);
                            ovmi->loop_end_found = 1;
                            ovmi->loop_flag = (ovmi->loop_end != 0);
                            break;
                        }

                        loop_offset += 0x50;
                    }

                    close_streamfile(dec_sf);
                }

                close_streamfile(sf_isl);
            }
        }

        return 1;
    }

    /* Capcom's MT Framework PC games [Devil May Cry 4 SE (PC), Biohazard 6 (PC), Mega Man X Legacy Collection (PC)] */
    if (check_extensions(sf,"sngw")) {
        /* optionally(?) encrypted */
        if (!is_id32be(0x00,sf, "OggS") && read_u32be(0x00,sf) == read_u32be(0x10,sf)) {
            cfg->key_len = read_streamfile(cfg->key, 0x00, 0x04, sf);
            cfg->is_header_swap = 1;
            cfg->is_nibble_swap = 1;
            cfg->is_encrypted = 1;
        }

        ovmi->disable_reordering = 1; /* must be an MT Framework thing */

        return 1;
    }

    /* Nippon Ichi PC games */
    if (check_extensions(sf,"lse")) {
        if (read_u32be(0x00,sf) == 0xFFFFFFFF) { /* [Operation Abyss: New Tokyo Legacy (PC)] */
            cfg->key[0] = 0xFF;
            cfg->key_len = 1;
            cfg->is_header_swap = 1;
            cfg->is_encrypted = 1;
        }
        else { /* [Operation Babel: New Tokyo Legacy (PC), Labyrinth of Refrain: Coven of Dusk (PC)] */
            int i;
            /* found at file_size-1 but this works too (same key for most files but can vary) */
            uint8_t base_key = read_u8(0x04,sf) - 0x04;

            cfg->key_len = 256;
            for (i = 0; i < cfg->key_len; i++) {
                cfg->key[i] = (uint8_t)(base_key + i);
            }
            cfg->is_encrypted = 1;
        }

        return 1;
    }

    /* .eno: Metronomicon (PC) */
    if (check_extensions(sf,"eno")) {
        /* 0x00: first byte probably derives into key, but this works too */
        cfg->key[0] = read_u8(0x05,sf); /* regular ogg have a zero at this offset = easy key */
        cfg->key_len = 0x01;
        cfg->is_encrypted = 1;
        cfg->start = 0x01; /* encrypted "OggS" starts after key-thing */

        return 1;
    }

    /* .bgm: Fortissimo (PC) */
    if (check_extensions(sf,"bgm")) {
        uint32_t file_size = get_streamfile_size(sf);
        uint8_t key[0x04];
        uint32_t xor_be;

        put_u32le(key, file_size);
        xor_be = get_u32be(key);
        if ((read_u32be(0x00,sf) ^ xor_be) == get_id32be("OggS")) {
            int i;
            cfg->key_len = 4;
            for (i = 0; i < cfg->key_len; i++) {
                cfg->key[i] = key[i];
            }
            cfg->is_encrypted = 1;

            return 1;
        }
    }

    /* "Ultramarine3" (???) */
    if (check_extensions(sf,"um3")) {
        if (!is_id32be(0x00,sf, "OggS")) {
            ovmi->decryption_callback = um3_ogg_decryption_callback;
        }

        return 1;
    }


fail:
    return 0;
}

/* Ogg Vorbis - standard .ogg with (possibly) loop comments/metadata */
static VGMSTREAM* _init_vgmstream_ogg_vorbis_common(STREAMFILE* sf) {
    ogg_vorbis_io_config_data cfg = {0};
    ogg_vorbis_meta_info_t ovmi = {0};

    /* checks */
    if (!_init_vgmstream_ogg_vorbis_tests(sf, &cfg, &ovmi))
        goto fail;

    return _init_vgmstream_ogg_vorbis_cfg_ovmi(sf, &cfg, &ovmi);
fail:
    return NULL;
}

/* Ogg Vorbis - encrypted .ogg [Yumekoi Tensei (PC)] */
static VGMSTREAM* _init_vgmstream_ogg_vorbis_tink(STREAMFILE* sf) {
    ogg_vorbis_io_config_data cfg = {0};
    ogg_vorbis_meta_info_t ovmi = {0};
    uint32_t start;

    /* checks */
    if (is_id32be(0x00, sf, "Tink")) {
        start = 0x00;
    }
    else if (is_id32be(0x0c, sf, "Tink")) {
        ovmi.loop_start = read_u32le(0x00, sf);
        ovmi.loop_end = read_u32le(0x04, sf);
        ovmi.loop_flag = read_u32le(0x0c, sf);
        ovmi.loop_end_found = 1;
        start = 0x0c;
    } 
    else {
        goto fail;
    } 

    if (!check_extensions(sf,"u0"))
        goto fail;

    cfg.is_encrypted = 1;
    cfg.is_header_swap = 1;
    cfg.start = start;
    cfg.max_offset = 0xE1F;
    cfg.key_len = 0xE1F;

    if (sizeof(cfg.key) < cfg.key_len)
        goto fail;

    /* copy key */
    {
        static const char* keystring = "BB3206F-F171-4885-A131-EC7FBA6FF491 Copyright 2004 Cyberworks \"TinkerBell\"., all rights reserved.";
        int i, keystring_len;
        start = 0;

        memset(cfg.key, 0, 0x04);
        put_u8   (cfg.key + 0x04, 0x44);

        keystring_len = strlen(keystring) + 1; /* including null */
        for (i = 0x05; i < cfg.key_len; i += keystring_len) {
            int copy = keystring_len;
            if (i + copy > cfg.key_len)
                copy = cfg.key_len - i;
            memcpy(cfg.key + i, keystring, copy);
        }
    }

    return _init_vgmstream_ogg_vorbis_cfg_ovmi(sf, &cfg, &ovmi);
fail:
    return NULL;
}

static VGMSTREAM* _init_vgmstream_ogg_vorbis(STREAMFILE* sf) {
    VGMSTREAM* v;

    v = _init_vgmstream_ogg_vorbis_common(sf);
    if (v) return v;

    v = _init_vgmstream_ogg_vorbis_tink(sf);
    if (v) return v;

    return NULL;
}


static VGMSTREAM* _init_vgmstream_ogg_vorbis_config(STREAMFILE* sf, off_t start, const ogg_vorbis_meta_info_t* ovmi) {
    VGMSTREAM* vgmstream = NULL;
    ogg_vorbis_codec_data* data = NULL;
    ogg_vorbis_io io = {0};
    char name[STREAM_NAME_SIZE] = {0};
    int channels, sample_rate, num_samples;

    int loop_flag = ovmi->loop_flag;
    int32_t loop_start = ovmi->loop_start;
    int loop_length_found = ovmi->loop_length_found;
    int32_t loop_length = ovmi->loop_length;
    int loop_end_found = ovmi->loop_end_found;
    int32_t loop_end = ovmi->loop_end;

    size_t stream_size = ovmi->stream_size ?
            ovmi->stream_size :
            get_streamfile_size(sf) - start;
    int force_seek = 0;
    int disable_reordering = ovmi->disable_reordering;


    //todo improve how to pass config
    io.decryption_callback = ovmi->decryption_callback;
    io.scd_xor = ovmi->scd_xor;
    io.scd_xor_length = ovmi->scd_xor_length;
    io.xor_value = ovmi->xor_value;

    data = init_ogg_vorbis(sf, start, stream_size, &io);
    if (!data) goto fail;

    ogg_vorbis_get_info(data, &channels, &sample_rate);
    ogg_vorbis_get_samples(data, &num_samples); /* let libvorbisfile find total samples */

    /* search for loop comments */
    {//todo ignore if loop flag already set?
        const char* comment = NULL;

        while (ogg_vorbis_get_comment(data, &comment)) {

            if (strstr(comment,"loop_start=") == comment || /* Phantasy Star Online: Blue Burst (PC) (no loop_end pair) */
                strstr(comment,"LOOP_START=") == comment || /* Phantasy Star Online: Blue Burst (PC), common */
                strstr(comment,"LOOPPOINT=") == comment || /* Sonic Robo Blast 2 */
                strstr(comment,"COMMENT=LOOPPOINT=") == comment ||
                strstr(comment,"LOOPSTART=") == comment ||
                strstr(comment,"um3.stream.looppoint.start=") == comment ||
                strstr(comment,"LOOP_BEGIN=") == comment || /* Hatsune Miku: Project Diva F (PS3) */
                strstr(comment,"LoopStart=") == comment ||  /* Capcom games [Devil May Cry 4 (PC)] */
                strstr(comment,"LOOP=") == comment || /* Duke Nukem 3D: 20th Anniversary World Tour */
                strstr(comment,"XIPH_CUE_LOOPSTART=") == comment) {  /* DeNa games [Super Mario Run (Android), FF Record Keeper (Android)] */
                loop_start = atol(strrchr(comment,'=')+1);
                loop_flag = (loop_start >= 0);
            }
            else if (strstr(comment,"LOOPLENGTH=") == comment) {/* (LOOPSTART pair) */
                loop_length = atol(strrchr(comment,'=')+1);
                loop_length_found = 1;
            }
            else if (strstr(comment,"title=-lps") == comment) { /* KID [Memories Off #5 (PC), Remember11 (PC)] */
                loop_start = atol(comment+10);
                loop_flag = (loop_start >= 0);
            }
            else if (strstr(comment,"album=-lpe") == comment) { /* (title=-lps pair) */
                loop_end = atol(comment+10);
                loop_end_found = 1;
                loop_flag = 1;
            }
            else if (strstr(comment,"LoopEnd=") == comment) { /* (LoopStart pair) */
                loop_end = atol(strrchr(comment,'=')+1);
                loop_end_found = 1;
            }
            else if (strstr(comment,"LOOP_END=") == comment) { /* (LOOP_START/LOOP_BEGIN pair) */
                loop_end = atol(strrchr(comment,'=')+1);
                loop_end_found = 1;
            }
            else if (strstr(comment,"lp=") == comment) {
                sscanf(strrchr(comment,'=')+1,"%d,%d", &loop_start,&loop_end);
                loop_end_found = 1;
                loop_flag = 1;
            }
            else if (strstr(comment,"LOOPDEFS=") == comment) { /* Fairy Fencer F: Advent Dark Force */
                sscanf(strrchr(comment,'=')+1,"%d,%d", &loop_start,&loop_end);
                loop_flag = 1;
                loop_end_found = 1;
            }
            else if (strstr(comment,"COMMENT=loop(") == comment) { /* Zero Time Dilemma (PC) */
                sscanf(strrchr(comment,'(')+1,"%d,%d", &loop_start,&loop_end);
                loop_flag = 1;
                loop_end_found = 1;
            }
            else if (strstr(comment, "XIPH_CUE_LOOPEND=") == comment) { /* (XIPH_CUE_LOOPSTART pair) */
                loop_end = atol(strrchr(comment, '=') + 1);
                loop_end_found = 1;
            }
            else if (strstr(comment, "omment=") == comment) { /* Air (Android) */
                sscanf(strstr(comment, "=LOOPSTART=") + 11, "%d,LOOPEND=%d", &loop_start, &loop_end);
                loop_flag = 1;
                loop_end_found = 1;
            }
            else if (strstr(comment,"MarkerNum=0002") == comment) { /* Megaman X Legacy Collection: MMX1/2/3 (PC) flag */
                /* uses LoopStart=-1 LoopEnd=-1, then 3 secuential comments: "MarkerNum" + "M=7F(start)" + "M=7F(end)" */
                loop_flag = 1;
            }
            else if (strstr(comment,"M=7F") == comment) { /* Megaman X Legacy Collection: MMX1/2/3 (PC) start/end */
                if (loop_flag && loop_start < 0) { /* LoopStart should set as -1 before */
                    sscanf(comment,"M=7F%x", &loop_start);
                }
                else if (loop_flag && loop_start >= 0) {
                    sscanf(comment,"M=7F%x", &loop_end);
                    loop_end_found = 1;
                }
            }
            else if (strstr(comment,"LOOPMS=") == comment) { /* Sonic Robo Blast 2 (PC) */
                loop_start = atol(strrchr(comment,'=')+1) * sample_rate / 1000; /* ms to samples */
                loop_flag = (loop_start >= 0);
            }
            else if (strstr(comment,"COMMENT=- loopTime ") == comment ||    /* Aristear Remain (PC) */
                     strstr(comment,"COMMENT=-loopTime ") == comment) {     /* Hyakki Ryouran no Yakata x Kawarazaki-ke no Ichizoku (PC) */
                loop_start = atol(strrchr(comment,' ')) / 1000.0f * sample_rate; /* ms to samples */
                loop_flag = (loop_start >= 0);

                /* files have all page granule positions -1 except a few close to loop. This throws off
                 * libvorbis seeking (that uses granules), so we need manual fix = slower. Could be detected
                 * by checking granules in the first new OggS pages (other games from the same dev don't use
                 * loopTime nor have wrong granules though) */
                force_seek = 1;
            }

            /* Hatsune Miku Project DIVA games, though only 'Arcade Future Tone' has >4ch files
             * ENCODER tag is common but ogg_vorbis_encode looks unique enough
             * (arcade ends with "2010-11-26" while consoles have "2011-02-07" */
            if (strstr(comment, "ENCODER=ogg_vorbis_encode/") == comment) {
                disable_reordering = 1;
            }

            if (strstr(comment, "TITLE=") == comment) {
                strncpy(name, comment + 6, sizeof(name) - 1);
            }

            ;VGM_LOG("OGG: user_comment=%s\n", comment);
        }
    }

    ogg_vorbis_set_disable_reordering(data, disable_reordering);
    ogg_vorbis_set_force_seek(data, force_seek);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->codec_data = data;
    vgmstream->coding_type = coding_OGG_VORBIS;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = ovmi->meta_type;
    if (!vgmstream->meta_type)
        vgmstream->meta_type = meta_OGG_VORBIS;

    vgmstream->sample_rate = sample_rate;
    vgmstream->stream_size = stream_size;

    if (ovmi->total_subsongs) /* not setting it has some effect when showing stream names */
        vgmstream->num_streams = ovmi->total_subsongs;

    if (name[0] != '\0')
        strcpy(vgmstream->stream_name, name);

    vgmstream->num_samples = num_samples;
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start;
        if (loop_length_found)
            vgmstream->loop_end_sample = loop_start + loop_length;
        else if (loop_end_found)
            vgmstream->loop_end_sample = loop_end;
        else
            vgmstream->loop_end_sample = vgmstream->num_samples;

        if (vgmstream->loop_end_sample > vgmstream->num_samples)
            vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    if (vgmstream->channels <= 8) {
        vgmstream->channel_layout = xiph_mappings[vgmstream->channels];
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#endif
