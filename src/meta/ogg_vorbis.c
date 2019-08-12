#include "../vgmstream.h"

#ifdef VGM_USE_VORBIS
#include <stdio.h>
#include <string.h>
#include <vorbis/vorbisfile.h>
#include "meta.h"
#include "ogg_vorbis_streamfile.h"

#define OGG_DEFAULT_BITSTREAM 0

static size_t ov_read_func(void *ptr, size_t size, size_t nmemb, void * datasource) {
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    size_t bytes_read, items_read;

    off_t real_offset = ov_streamfile->start + ov_streamfile->offset;
    size_t max_bytes = size * nmemb;

    /* clamp for virtual filesize */
    if (max_bytes > ov_streamfile->size - ov_streamfile->offset)
        max_bytes = ov_streamfile->size - ov_streamfile->offset;

    bytes_read = read_streamfile(ptr, real_offset, max_bytes, ov_streamfile->streamfile);
    items_read = bytes_read / size;

    /* may be encrypted */
    if (ov_streamfile->decryption_callback) {
        ov_streamfile->decryption_callback(ptr, size, items_read, ov_streamfile);
    }

    ov_streamfile->offset += items_read * size;

    return items_read;
}

static int ov_seek_func(void *datasource, ogg_int64_t offset, int whence) {
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    ogg_int64_t base_offset, new_offset;

    switch (whence) {
        case SEEK_SET:
            base_offset = 0;
            break;
        case SEEK_CUR:
            base_offset = ov_streamfile->offset;
            break;
        case SEEK_END:
            base_offset = ov_streamfile->size;
            break;
        default:
            return -1;
            break;
    }


    new_offset = base_offset + offset;
    if (new_offset < 0 || new_offset > ov_streamfile->size) {
        return -1; /* *must* return -1 if stream is unseekable */
    } else {
        ov_streamfile->offset = new_offset;
        return 0;
    }
}

static long ov_tell_func(void * datasource) {
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    return ov_streamfile->offset;
}

static int ov_close_func(void * datasource) {
    /* needed as setting ov_close_func in ov_callbacks to NULL doesn't seem to work
     * (closing the streamfile is done in free_ogg_vorbis) */
    return 0;
}


static void um3_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    size_t bytes_read = size*nmemb;
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    int i;

    /* first 0x800 bytes are xor'd */
    if (ov_streamfile->offset < 0x800) {
        int num_crypt = 0x800 - ov_streamfile->offset;
        if (num_crypt > bytes_read)
            num_crypt = bytes_read;

        for (i = 0; i < num_crypt; i++)
            ((uint8_t*)ptr)[i] ^= 0xff;
    }
}

static void kovs_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    size_t bytes_read = size*nmemb;
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    int i;

    /* first 0x100 bytes are xor'd */
    if (ov_streamfile->offset < 0x100) {
        int max_offset = ov_streamfile->offset + bytes_read;
        if (max_offset > 0x100)
            max_offset = 0x100;

        for (i = ov_streamfile->offset; i < max_offset; i++) {
            ((uint8_t*)ptr)[i-ov_streamfile->offset] ^= i;
        }
    }
}

static void psychic_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    size_t bytes_read = size*nmemb;
    uint8_t key[6] = { 0x23,0x31,0x20,0x2e,0x2e,0x28 };
    int i;

    //todo incorrect, picked value changes (fixed order for all files), or key is bigger
    /* bytes add key that changes every 0x64 bytes */
    for (i = 0; i < bytes_read; i++) {
        int pos = (ov_streamfile->offset + i) / 0x64;
        ((uint8_t*)ptr)[i] += key[pos % sizeof(key)];
    }
}

static void rpgmvo_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    static const uint8_t header[16] = { /* OggS, packet type, granule, stream id(empty) */
            0x4F,0x67,0x67,0x53,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    };
    size_t bytes_read = size*nmemb;
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    int i;

    /* first 0x10 are xor'd, but header can be easily reconstructed
     * (key is also in (game)/www/data/System.json "encryptionKey") */
    for (i = 0; i < bytes_read; i++) {
        if (ov_streamfile->offset+i < 0x10) {
            ((uint8_t*)ptr)[i] = header[(ov_streamfile->offset + i) % 16];

            /* last two bytes are the stream id, get from next OggS */
            if (ov_streamfile->offset+i == 0x0e)
                ((uint8_t*)ptr)[i] = read_8bit(0x58, ov_streamfile->streamfile);
            if (ov_streamfile->offset+i == 0x0f)
                ((uint8_t*)ptr)[i] = read_8bit(0x59, ov_streamfile->streamfile);
        }
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


/* Ogg Vorbis, by way of libvorbisfile; may contain loop comments */
VGMSTREAM * init_vgmstream_ogg_vorbis(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    ogg_vorbis_io_config_data cfg = {0};
    ogg_vorbis_meta_info_t ovmi = {0};
    off_t start_offset = 0;

    int is_ogg = 0;
    int is_um3 = 0;
    int is_kovs = 0;
    int is_sngw = 0;
    int is_isd = 0;
    int is_rpgmvo = 0;
    int is_eno = 0;
    int is_gwm = 0;
    int is_mus = 0;
    int is_lse = 0;
    int is_bgm = 0;


    /* check extension */
    /* .ogg: standard/various, .logg: renamed for plugins
     * .adx: KID [Remember11 (PC)]
     * .rof: The Rhythm of Fighters (Mobile)
     * .acm: Planescape Torment Enhanced Edition (PC)
     * .sod: Zone 4 (PC)
     * .aif/laif/aif-Loop: Psychonauts (PC) raw extractions (named) */
    if (check_extensions(streamFile,"ogg,logg,adx,rof,acm,sod,aif,laif,aif-Loop")) {
        is_ogg = 1;
    } else if (check_extensions(streamFile,"um3")) {
        is_um3 = 1;
    } else if (check_extensions(streamFile,"kvs,kovs")) { /* .kvs: Atelier Sophie (PC), kovs: header id only? */
        is_kovs = 1;
    } else if (check_extensions(streamFile,"sngw")) { /* .sngw: Capcom [Devil May Cry 4 SE (PC), Biohazard 6 (PC)] */
        is_sngw = 1;
    } else if (check_extensions(streamFile,"isd")) { /* .isd: Inti Creates PC games */
        is_isd = 1;
    } else if (check_extensions(streamFile,"rpgmvo")) { /* .rpgmvo: RPG Maker MV games (PC) */
        is_rpgmvo = 1;
    } else if (check_extensions(streamFile,"eno")) { /* .eno: Metronomicon (PC) */
        is_eno = 1;
    } else if (check_extensions(streamFile,"gwm")) { /* .gwm: Adagio: Cloudburst (PC) */
        is_gwm = 1;
    } else if (check_extensions(streamFile,"mus")) { /* .mus: Redux - Dark Matters (PC) */
        is_mus = 1;
    } else if (check_extensions(streamFile,"lse")) { /* .lse: Labyrinth of Refrain: Coven of Dusk (PC) */
        is_lse = 1;
    } else if (check_extensions(streamFile,"bgm")) { /* .bgm: Fortissimo (PC) */
        is_bgm = 1;
    } else {
        goto fail;
    }

    if (is_ogg) {
        if (read_32bitBE(0x00,streamFile) == 0x2c444430) { /* Psychic Software [Darkwind: War on Wheels (PC)] */
            ovmi.decryption_callback = psychic_ogg_decryption_callback;
            ovmi.meta_type = meta_OGG_encrypted;
        }
        else if (read_32bitBE(0x00,streamFile) == 0x4C325344) { /* "L2SD" instead of "OggS" [Lineage II Chronicle 4 (PC)] */
            cfg.is_header_swap = 1;
            cfg.is_encrypted = 1;
        }
        else if (read_32bitBE(0x00,streamFile) == 0x048686C5) { /* "OggS" XOR'ed + bitswapped [Ys VIII (PC)] */
            cfg.key[0] = 0xF0;
            cfg.key_len = 1;
            cfg.is_nibble_swap = 1;
            cfg.is_encrypted = 1;

        }
        else if (read_32bitBE(0x00,streamFile) == 0x00000000 && /* null instead of "OggS" [Yuppie Psycho (PC)] */
                 read_32bitBE(0x3a,streamFile) == 0x4F676753) {
            cfg.is_header_swap = 1;
            cfg.is_encrypted = 1;
        }
        else if (read_32bitBE(0x00,streamFile) == 0x4f676753) { /* "OggS" (standard) */
            ovmi.meta_type = meta_OGG_VORBIS;
        }
        else {
            goto fail; /* unknown/not Ogg Vorbis (ex. Wwise) */
        }
    }

    if (is_um3) { /* ["Ultramarine3" (???)] */
        if (read_32bitBE(0x00,streamFile) != 0x4f676753) { /* "OggS" (optionally encrypted) */
            ovmi.decryption_callback = um3_ogg_decryption_callback;
        }
        ovmi.meta_type = meta_OGG_encrypted;
    }

    if (is_kovs) { /* Koei Tecmo PC games */
        if (read_32bitBE(0x00,streamFile) != 0x4b4f5653) { /* "KOVS" */
            goto fail;
        }
        ovmi.loop_start = read_32bitLE(0x08,streamFile);
        ovmi.loop_flag = (ovmi.loop_start != 0);
        ovmi.decryption_callback = kovs_ogg_decryption_callback;
        ovmi.meta_type = meta_OGG_KOVS;

        start_offset = 0x20;
    }

    if (is_sngw) { /* [Capcom's MT Framework PC games] */
        if (read_32bitBE(0x00,streamFile) != 0x4f676753) { /* "OggS" (optionally encrypted) */
            cfg.key_len = read_streamfile(cfg.key, 0x00, 0x04, streamFile);
            cfg.is_header_swap = 1;
            cfg.is_nibble_swap = 1;
            cfg.is_encrypted = 1;
        }

        ovmi.disable_reordering = 1; /* must be an MT Framework thing */
    }

    if (is_isd) { /* Inti Creates PC games */
        const char *isl_name = NULL;

        /* check various encrypted "OggS" values */
        if (read_32bitBE(0x00,streamFile) == 0xAF678753) { /* Azure Striker Gunvolt (PC) */
            static const uint8_t isd_gv_key[16] = {
                    0xe0,0x00,0xe0,0x00,0xa0,0x00,0x00,0x00,0xe0,0x00,0xe0,0x80,0x40,0x40,0x40,0x00
            };
            cfg.key_len = sizeof(isd_gv_key);
            memcpy(cfg.key, isd_gv_key, cfg.key_len);
            isl_name = "GV_steam.isl";
        }
        else if (read_32bitBE(0x00,streamFile) == 0x0FE787D3) { /* Mighty Gunvolt (PC) */
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
            cfg.key_len = sizeof(isd_mgv_key);
            memcpy(cfg.key, isd_mgv_key, cfg.key_len);
            isl_name = "MGV_steam.isl";
        }
        else if (read_32bitBE(0x00,streamFile) == 0x0FA74753) { /* Blaster Master Zero (PC) */
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
            cfg.key_len = sizeof(isd_bmz_key);
            memcpy(cfg.key, isd_bmz_key, cfg.key_len);
            isl_name = "output.isl";
        }
        else {
            goto fail;
        }

        cfg.is_encrypted = 1;

        /* .isd have companion files in the prev folder:
         * - .ish: constant id/names (not always)
         * - .isf: format table, ordered like file id/numbers, 0x18 header with:
         *   0x00(2): ?, 0x02(2): channels, 0x04: sample rate, 0x08: skip samples (in PCM bytes), always 32000
         *   0x0c(2): PCM block size, 0x0e(2): PCM bps, 0x10: null, 0x18: samples (in PCM bytes)
         * - .isl: looping table (encrypted like the files) */
        if (isl_name) {
            STREAMFILE *islFile = NULL;

            //todo could try in ../(file) first since that's how the .isl is stored
            islFile = open_streamfile_by_filename(streamFile, isl_name);
            if (islFile) {
                STREAMFILE *dec_sf = NULL;

                dec_sf = setup_ogg_vorbis_streamfile(islFile, cfg);
                if (dec_sf) {
                    off_t loop_offset;
                    char basename[PATH_LIMIT];

                    /* has a bunch of tables then a list with file names without extension and loops */
                    loop_offset = read_32bitLE(0x18, dec_sf);
                    get_streamfile_basename(streamFile, basename, sizeof(basename));

                    while (loop_offset < get_streamfile_size(dec_sf)) {
                        char testname[0x20];

                        read_string(testname, sizeof(testname), loop_offset+0x2c, dec_sf);
                        if (strcmp(basename, testname) == 0) {
                            ovmi.loop_start = read_32bitLE(loop_offset+0x1c, dec_sf);
                            ovmi.loop_end   = read_32bitLE(loop_offset+0x20, dec_sf);
                            ovmi.loop_end_found = 1;
                            ovmi.loop_flag = (ovmi.loop_end != 0);
                            break;
                        }

                        loop_offset += 0x50;
                    }

                    close_streamfile(dec_sf);
                }

                close_streamfile(islFile);
            }
        }
    }

    if (is_rpgmvo) { /* [RPG Maker MV (PC)] */
        if (read_32bitBE(0x00,streamFile) != 0x5250474D &&  /* "RPGM" */
            read_32bitBE(0x00,streamFile) != 0x56000000) {  /* "V\0\0\0" */
            goto fail;
        }
        ovmi.decryption_callback = rpgmvo_ogg_decryption_callback;
        ovmi.meta_type = meta_OGG_encrypted;

        start_offset = 0x10;
    }

    if (is_eno) { /* [Metronomicon (PC)] */
        /* first byte probably derives into key, but this works too */
        cfg.key[0] = (uint8_t)read_8bit(0x05,streamFile); /* regular ogg have a zero at this offset = easy key */;
        cfg.key_len = 1;
        cfg.is_encrypted = 1;
        start_offset = 0x01; /* "OggS" starts after key-thing */
    }

    if (is_gwm) { /* [Adagio: Cloudburst (PC)] */
        cfg.key[0] = 0x5D;
        cfg.key_len = 1;
        cfg.is_encrypted = 1;
    }

    if (is_mus) { /* [Redux - Dark Matters (PC)] */
        static const uint8_t mus_key[16] = {
                0x21,0x4D,0x6F,0x01,0x20,0x4C,0x6E,0x02,0x1F,0x4B,0x6D,0x03,0x20,0x4C,0x6E,0x02
        };
        cfg.key_len = sizeof(mus_key);
        memcpy(cfg.key, mus_key, cfg.key_len);
        cfg.is_header_swap = 1; /* decrypted header gives "Mus " */
        cfg.is_encrypted = 1;
    }

    if (is_lse) { /* [Nippon Ichi PC games] */
        if (read_32bitBE(0x00,streamFile) == 0xFFFFFFFF) { /* [Operation Abyss: New Tokyo Legacy (PC)] */
            cfg.key[0] = 0xFF;
            cfg.key_len = 1;
            cfg.is_header_swap = 1;
            cfg.is_encrypted = 1;
        }
        else { /* [Operation Babel: New Tokyo Legacy (PC), Labyrinth of Refrain: Coven of Dusk (PC)] */
            int i;
            /* found at file_size-1 but this works too (same key for most files but can vary) */
            uint8_t base_key = (uint8_t)read_8bit(0x04,streamFile) - 0x04;

            cfg.key_len = 256;
            for (i = 0; i < cfg.key_len; i++) {
                cfg.key[i] = (uint8_t)(base_key + i);
            }
            cfg.is_encrypted = 1;
        }
    }

    if (is_bgm) { /* [Fortissimo (PC)] */
        size_t file_size = get_streamfile_size(streamFile);
        uint8_t key[0x04];
        uint32_t xor_be;

        put_32bitLE(key, (uint32_t)file_size);
        xor_be = (uint32_t)get_32bitBE(key);
        if ((read_32bitBE(0x00,streamFile) ^ xor_be) == 0x4F676753) { /* "OggS" */
            int i;
            cfg.key_len = 4;
            for (i = 0; i < cfg.key_len; i++) {
                cfg.key[i] = key[i];
            }
            cfg.is_encrypted = 1;
        }
    }

    if (cfg.is_encrypted) {
        ovmi.meta_type = meta_OGG_encrypted;

        temp_streamFile = setup_ogg_vorbis_streamfile(streamFile, cfg);
        if (!temp_streamFile) goto fail;
    }

    vgmstream = init_vgmstream_ogg_vorbis_callbacks(temp_streamFile != NULL ? temp_streamFile : streamFile, NULL, start_offset, &ovmi);

    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

VGMSTREAM * init_vgmstream_ogg_vorbis_callbacks(STREAMFILE *streamFile, ov_callbacks *callbacks_p, off_t start, const ogg_vorbis_meta_info_t *ovmi) {
    VGMSTREAM * vgmstream = NULL;
    ogg_vorbis_codec_data * data = NULL;
    OggVorbis_File *ovf = NULL;
    vorbis_info *vi;
    char name[STREAM_NAME_SIZE] = {0};

    int loop_flag = ovmi->loop_flag;
    int32_t loop_start = ovmi->loop_start;
    int loop_length_found = ovmi->loop_length_found;
    int32_t loop_length = ovmi->loop_length;
    int loop_end_found = ovmi->loop_end_found;
    int32_t loop_end = ovmi->loop_end;
    size_t stream_size = ovmi->stream_size ?
            ovmi->stream_size :
            get_streamfile_size(streamFile) - start;

    ov_callbacks default_callbacks;

    if (!callbacks_p) {
        default_callbacks.read_func = ov_read_func;
        default_callbacks.seek_func = ov_seek_func;
        default_callbacks.close_func = ov_close_func;
        default_callbacks.tell_func = ov_tell_func;

        callbacks_p = &default_callbacks;
    }

    /* test if this is a proper Ogg Vorbis file, with the current (from init_x) STREAMFILE */
    {
        OggVorbis_File temp_ovf = {0};
        ogg_vorbis_streamfile temp_streamfile = {0};

        temp_streamfile.streamfile = streamFile;

        temp_streamfile.start = start;
        temp_streamfile.offset = 0;
        temp_streamfile.size = stream_size;

        temp_streamfile.decryption_callback = ovmi->decryption_callback;
        temp_streamfile.scd_xor = ovmi->scd_xor;
        temp_streamfile.scd_xor_length = ovmi->scd_xor_length;
        temp_streamfile.xor_value = ovmi->xor_value;

        /* open the ogg vorbis file for testing */
        if (ov_test_callbacks(&temp_streamfile, &temp_ovf, NULL, 0, *callbacks_p))
            goto fail;

        /* we have to close this as it has the init_vgmstream meta-reading STREAMFILE */
        ov_clear(&temp_ovf);
    }


    /* proceed to init codec_data and reopen a STREAMFILE for this stream */
    {
        char filename[PATH_LIMIT];

        data = calloc(1,sizeof(ogg_vorbis_codec_data));
        if (!data) goto fail;

        streamFile->get_name(streamFile,filename,sizeof(filename));
        data->ov_streamfile.streamfile = streamFile->open(streamFile,filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!data->ov_streamfile.streamfile) goto fail;

        data->ov_streamfile.start = start;
        data->ov_streamfile.offset = 0;
        data->ov_streamfile.size = stream_size;

        data->ov_streamfile.decryption_callback = ovmi->decryption_callback;
        data->ov_streamfile.scd_xor = ovmi->scd_xor;
        data->ov_streamfile.scd_xor_length = ovmi->scd_xor_length;
        data->ov_streamfile.xor_value = ovmi->xor_value;

        /* open the ogg vorbis file for real */
        if (ov_open_callbacks(&data->ov_streamfile, &data->ogg_vorbis_file, NULL, 0, *callbacks_p))
            goto fail;
        ovf = &data->ogg_vorbis_file;
    }

    //todo could set bitstreams as subsongs?
    /* get info from bitstream 0 */
    data->bitstream = OGG_DEFAULT_BITSTREAM;
    vi = ov_info(ovf,OGG_DEFAULT_BITSTREAM);

    /* other settings */
    data->disable_reordering = ovmi->disable_reordering;

    /* search for loop comments */
    {//todo ignore if loop flag already set?
        int i;
        vorbis_comment *comment = ov_comment(ovf,OGG_DEFAULT_BITSTREAM);

        for (i = 0; i < comment->comments; i++) {
            const char * user_comment = comment->user_comments[i];
            if (strstr(user_comment,"loop_start=")==user_comment || /* PSO4 */
                strstr(user_comment,"LOOP_START=")==user_comment || /* PSO4 */
                strstr(user_comment,"COMMENT=LOOPPOINT=")==user_comment ||
                strstr(user_comment,"LOOPSTART=")==user_comment ||
                strstr(user_comment,"um3.stream.looppoint.start=")==user_comment ||
                strstr(user_comment,"LOOP_BEGIN=")==user_comment || /* Hatsune Miku: Project Diva F (PS3) */
                strstr(user_comment,"LoopStart=")==user_comment ||  /* Devil May Cry 4 (PC) */
                strstr(user_comment,"XIPH_CUE_LOOPSTART=")==user_comment) {  /* Super Mario Run (Android) */
                loop_start = atol(strrchr(user_comment,'=')+1);
                loop_flag = (loop_start >= 0);
            }
            else if (strstr(user_comment,"LOOPLENGTH=")==user_comment) {/* (LOOPSTART pair) */
                loop_length = atol(strrchr(user_comment,'=')+1);
                loop_length_found = 1;
            }
            else if (strstr(user_comment,"title=-lps")==user_comment) { /* KID [Memories Off #5 (PC), Remember11 (PC)] */
                loop_start = atol(user_comment+10);
                loop_flag = (loop_start >= 0);
            }
            else if (strstr(user_comment,"album=-lpe")==user_comment) { /* (title=-lps pair) */
                loop_end = atol(user_comment+10);
                loop_flag = 1;
                loop_end_found = 1;
            }
            else if (strstr(user_comment,"LoopEnd=")==user_comment) { /* (LoopStart pair) */
                if(loop_flag) {
                    loop_length = atol(strrchr(user_comment,'=')+1)-loop_start;
                    loop_length_found = 1;
                }
            }
            else if (strstr(user_comment,"LOOP_END=")==user_comment) { /* (LOOP_BEGIN pair) */
                if(loop_flag) {
                    loop_length = atol(strrchr(user_comment,'=')+1)-loop_start;
                    loop_length_found = 1;
                }
            }
            else if (strstr(user_comment,"lp=")==user_comment) {
                sscanf(strrchr(user_comment,'=')+1,"%d,%d", &loop_start,&loop_end);
                loop_flag = 1;
                loop_end_found = 1;
            }
            else if (strstr(user_comment,"LOOPDEFS=")==user_comment) { /* Fairy Fencer F: Advent Dark Force */
                sscanf(strrchr(user_comment,'=')+1,"%d,%d", &loop_start,&loop_end);
                loop_flag = 1;
                loop_end_found = 1;
            }
            else if (strstr(user_comment,"COMMENT=loop(")==user_comment) { /* Zero Time Dilemma (PC) */
                sscanf(strrchr(user_comment,'(')+1,"%d,%d", &loop_start,&loop_end);
                loop_flag = 1;
                loop_end_found = 1;
            }
            else if (strstr(user_comment, "XIPH_CUE_LOOPEND=") == user_comment) { /* XIPH_CUE_LOOPSTART pair */
                if (loop_flag) {
                    loop_length = atol(strrchr(user_comment, '=') + 1) - loop_start;
                    loop_length_found = 1;
                }
            }
            else if (strstr(user_comment, "omment=") == user_comment) { /* Air (Android) */
                sscanf(strstr(user_comment, "=LOOPSTART=") + 11, "%d,LOOPEND=%d", &loop_start, &loop_end);
                loop_flag = 1;
                loop_end_found = 1;
            }
            else if (strstr(user_comment,"MarkerNum=0002")==user_comment) { /* Megaman X Legacy Collection: MMX1/2/3 (PC) flag */
                /* uses LoopStart=-1 LoopEnd=-1, then 3 secuential comments: "MarkerNum" + "M=7F(start)" + "M=7F(end)" */
                loop_flag = 1;
            }
            else if (strstr(user_comment,"M=7F")==user_comment) { /* Megaman X Legacy Collection: MMX1/2/3 (PC) start/end */
                if (loop_flag && loop_start < 0) { /* LoopStart should set as -1 before */
                    sscanf(user_comment,"M=7F%x", &loop_start);
                }
                else if (loop_flag && loop_start >= 0) {
                    sscanf(user_comment,"M=7F%x", &loop_end);
                    loop_end_found = 1;
                }
            }

            /* Hatsune Miku Project DIVA games, though only 'Arcade Future Tone' has >4ch files
             * ENCODER tag is common but ogg_vorbis_encode looks unique enough
             * (arcade ends with "2010-11-26" while consoles have "2011-02-07" */
            if (strstr(user_comment, "ENCODER=ogg_vorbis_encode/") == user_comment) {
                data->disable_reordering = 1;
            }

            if (strstr(user_comment, "TITLE=") == user_comment) {
                strncpy(name, user_comment + 6, sizeof(name) - 1);
            }

            ;VGM_LOG("OGG: user_comment=%s\n", user_comment);
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(vi->channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->codec_data = data; /* store our fun extra datas */
    vgmstream->channels = vi->channels;
    vgmstream->sample_rate = vi->rate;
    vgmstream->stream_size = stream_size;

    if (ovmi->total_subsongs) /* not setting it has some effect when showing stream names */
        vgmstream->num_streams = ovmi->total_subsongs;

    if (name[0] != '\0')
        strcpy(vgmstream->stream_name, name);

    vgmstream->num_samples = ov_pcm_total(ovf,-1); /* let libvorbisfile find total samples */
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start;
        if (loop_length_found)
            vgmstream->loop_end_sample = loop_start+loop_length;
        else if (loop_end_found)
            vgmstream->loop_end_sample = loop_end;
        else
            vgmstream->loop_end_sample = vgmstream->num_samples;

        if (vgmstream->loop_end_sample > vgmstream->num_samples)
            vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->coding_type = coding_OGG_VORBIS;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = ovmi->meta_type;

    if (vgmstream->channels <= 8) {
        vgmstream->channel_layout = xiph_mappings[vgmstream->channels];
    }

    return vgmstream;

fail:
    /* clean up anything we may have opened */
    if (data) {
        if (ovf)
            ov_clear(&data->ogg_vorbis_file);//same as ovf
        if (data->ov_streamfile.streamfile)
            close_streamfile(data->ov_streamfile.streamfile);
        free(data);
    }
    if (vgmstream) {
        vgmstream->codec_data = NULL;
        close_vgmstream(vgmstream);
    }
    return NULL;
}

#endif
