#include "../vgmstream.h"

#ifdef VGM_USE_VORBIS
#include <stdio.h>
#include <string.h>
#include "meta.h"
#include <vorbis/vorbisfile.h>

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

    /* first 0x800 bytes are xor'd with 0xff */
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

    /* first 0x100 bytes are xor'd with offset */
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
    size_t bytes_read = size*nmemb;
    int i;

    /* add 0x23 ('#') */
    for (i = 0; i < bytes_read; i++) {
            ((uint8_t*)ptr)[i] += 0x23;
    }
}

static void sngw_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    size_t bytes_read = size*nmemb;
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    int i;
    char *header_id = "OggS";
    uint8_t key[4];

    put_32bitBE(key, ov_streamfile->xor_value);

    /* bytes are xor'd with key and nibble-swapped, first "OggS" is changed */
    for (i = 0; i < bytes_read; i++) {
        if (ov_streamfile->offset+i < 0x04) {
            ((uint8_t*)ptr)[i] = (uint8_t)header_id[(ov_streamfile->offset + i) % 4];
        }
        else {
            uint8_t val = ((uint8_t*)ptr)[i] ^ key[(ov_streamfile->offset + i) % 4];
            ((uint8_t*)ptr)[i] = ((val << 4) & 0xf0) | ((val >> 4) & 0x0f);
        }
    }
}

static void isd_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    static const uint8_t key[16] = {
            0xe0,0x00,0xe0,0x00,0xa0,0x00,0x00,0x00,0xe0,0x00,0xe0,0x80,0x40,0x40,0x40,0x00
    };
    size_t bytes_read = size*nmemb;
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    int i;

    /* bytes are xor'd with key */
    for (i = 0; i < bytes_read; i++) {
        ((uint8_t*)ptr)[i] ^= key[(ov_streamfile->offset + i) % 16];
    }
}

static void l2sd_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    size_t bytes_read = size*nmemb;
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    int i;
    char *header_id = "OggS";

    /* first "OggS" is changed */
    for (i = 0; i < bytes_read; i++) {
        if (ov_streamfile->offset+i < 0x04) {
            /* replace key in the first 4 bytes with "OggS" */
            ((uint8_t*)ptr)[i] = (uint8_t)header_id[(ov_streamfile->offset + i) % 4];
        }
        else {
            break;
        }
    }
}

static void rpgmvo_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    static const uint8_t header[16] = { /* OggS, packet type, granule, stream id(empty) */
            0x4F,0x67,0x67,0x53,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    };
    size_t bytes_read = size*nmemb;
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    int i;

    /* first 0x10 are xor'd with a key, but the header can be easily reconstructed
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

static void eno_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    size_t bytes_read = size*nmemb;
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    int i;

    /* bytes are xor'd with key */
    for (i = 0; i < bytes_read; i++) {
        ((uint8_t*)ptr)[i] ^= (uint8_t)ov_streamfile->xor_value;
    }
}

static void ys8_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    size_t bytes_read = size*nmemb;
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    int i;

    /* bytes are xor'd with key and nibble-swapped */
    for (i = 0; i < bytes_read; i++) {
        uint8_t val = ((uint8_t*)ptr)[i] ^ ov_streamfile->xor_value;
        ((uint8_t*)ptr)[i] = ((val << 4) & 0xf0) | ((val >> 4) & 0x0f);
    }
}

static void gwm_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    size_t bytes_read = size*nmemb;
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    int i;

    /* bytes are xor'd with key */
    for (i = 0; i < bytes_read; i++) {
        ((uint8_t*)ptr)[i] ^= (uint8_t)ov_streamfile->xor_value;
    }
}

static void mus_ogg_decryption_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
    static const uint8_t key[16] = {
            0x21,0x4D,0x6F,0x01,0x20,0x4C,0x6E,0x02,0x1F,0x4B,0x6D,0x03,0x20,0x4C,0x6E,0x02
    };

    size_t bytes_read = size*nmemb;
    ogg_vorbis_streamfile * const ov_streamfile = datasource;
    int i;
    char *header_id = "OggS";

    /* bytes are xor'd with key, first "OggS" is changed */
    for (i = 0; i < bytes_read; i++) {
        if (ov_streamfile->offset+i < 0x04) { /* if decrypted gives "Mus " */
            ((uint8_t*)ptr)[i] = (uint8_t)header_id[(ov_streamfile->offset + i) % 4];
        }
        else {
            ((uint8_t*)ptr)[i] ^= key[(ov_streamfile->offset + i) % sizeof(key)];
        }
    }
}


/* Ogg Vorbis, by way of libvorbisfile; may contain loop comments */
VGMSTREAM * init_vgmstream_ogg_vorbis(STREAMFILE *streamFile) {
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


    /* check extension */
    /* .ogg: standard/various, .logg: renamed for plugins,
     * .adx: KID [Remember11 (PC)],
     * .rof: The Rhythm of Fighters (Mobile)
     * .acm: Planescape Torment Enhanced Edition (PC) */
    if (check_extensions(streamFile,"ogg,logg,adx,rof,acm")) {
        is_ogg = 1;
    } else if (check_extensions(streamFile,"um3")) {
        is_um3 = 1;
    } else if (check_extensions(streamFile,"kvs,kovs")) { /* .kvs: Atelier Sophie (PC), kovs: header id only? */
        is_kovs = 1;
    } else if (check_extensions(streamFile,"sngw")) { /* .sngw: Capcom [Devil May Cry 4 SE (PC), Biohazard 6 (PC)] */
        is_sngw = 1;
    } else if (check_extensions(streamFile,"isd")) { /* .isd: Azure Striker Gunvolt (PC) */
        is_isd = 1;
    } else if (check_extensions(streamFile,"rpgmvo")) { /* .rpgmvo: RPG Maker MV games (PC) */
        is_rpgmvo = 1;
    } else if (check_extensions(streamFile,"eno")) { /* .eno: Metronomicon (PC) */
        is_eno = 1;
    } else if (check_extensions(streamFile,"gwm")) { /* .gwm: Adagio: Cloudburst (PC) */
        is_gwm = 1;
    } else if (check_extensions(streamFile,"mus")) { /* .mus: Redux -  Dark Matters (PC) */
        is_mus = 1;
    } else {
        goto fail;
    }

    /* check standard Ogg Vorbis and variations */
    if (is_ogg) {
        if (read_32bitBE(0x00,streamFile) == 0x2c444430) { /* Psychic Software [Darkwind: War on Wheels (PC)] */
            ovmi.decryption_callback = psychic_ogg_decryption_callback;
            ovmi.meta_type = meta_OGG_PSYCHIC;
        }
        else if (read_32bitBE(0x00,streamFile) == 0x4C325344) { /* "L2SD" [Lineage II Chronicle 4 (PC)] */
            ovmi.decryption_callback = l2sd_ogg_decryption_callback;
            ovmi.meta_type = meta_OGG_L2SD;
        }
        else if (read_32bitBE(0x00,streamFile) == 0x048686C5) { /* XOR'ed + bitswapped "OggS" [Ys VIII (PC)] */
            ovmi.xor_value = 0xF0;
            ovmi.decryption_callback = ys8_ogg_decryption_callback;
            ovmi.meta_type = meta_OGG_YS8;
        }
        else if (read_32bitBE(0x00,streamFile) == 0x4f676753) { /* "OggS" */
            ovmi.meta_type = meta_OGG_VORBIS;
        }
        else {
            goto fail; /* unknown/not Ogg Vorbis (ex. Wwise) */
        }
    }

    /* check "Ultramarine3" (???), may be encrypted */
    if (is_um3) {
        if (read_32bitBE(0x00,streamFile) != 0x4f676753) { /* "OggS" */
            ovmi.decryption_callback = um3_ogg_decryption_callback;
        }
        ovmi.meta_type = meta_OGG_UM3;
    }

    /* check KOVS (Koei Tecmo games), header + encrypted */
    if (is_kovs) {
        if (read_32bitBE(0x00,streamFile) != 0x4b4f5653) { /* "KOVS" */
            goto fail;
        }
        ovmi.loop_start = read_32bitLE(0x08,streamFile);
        ovmi.loop_flag = (ovmi.loop_start != 0);
        ovmi.decryption_callback = kovs_ogg_decryption_callback;
        ovmi.meta_type = meta_OGG_KOVS;

        start_offset = 0x20;
    }

    /* check SNGW (Capcom's MT Framework PC games), may be encrypted */
    if (is_sngw) {
        if (read_32bitBE(0x00,streamFile) != 0x4f676753) { /* "OggS" */
            ovmi.xor_value = read_32bitBE(0x00,streamFile);
            ovmi.decryption_callback = sngw_ogg_decryption_callback;
        }
        ovmi.meta_type = meta_OGG_SNGW;
    }

    /* check ISD [Gunvolt (PC)], encrypted */
    if (is_isd) {
        ovmi.decryption_callback = isd_ogg_decryption_callback;
        ovmi.meta_type = meta_OGG_ISD;

        //todo looping unknown, not in Ogg comments
        // game has sound/GV_steam.* files with info about sound/stream/*.isd
        //- .ish: constant id/names
        //- .isl: unknown table, maybe looping?
        //- .isf: format table, ordered like file numbers, 0x18 header with:
        //   0x00(2): ?, 0x02(2): channels, 0x04: sample rate, 0x08: skip samples (in PCM bytes), always 32000
        //   0x0c(2): PCM block size, 0x0e(2): PCM bps, 0x10: null, 0x18: samples (in PCM bytes)
    }

    /* check RPGMKVO [RPG Maker MV (PC)], header + partially encrypted */
    if (is_rpgmvo) {
        if (read_32bitBE(0x00,streamFile) != 0x5250474D &&  /* "RPGM" */
            read_32bitBE(0x00,streamFile) != 0x56000000) {  /* "V\0\0\0" */
            goto fail;
        }
        ovmi.decryption_callback = rpgmvo_ogg_decryption_callback;
        ovmi.meta_type = meta_OGG_RPGMV;

        start_offset = 0x10;
    }

    /* check ENO [Metronomicon (PC)], key + encrypted */
    if (is_eno) {
        /* first byte probably derives into xor key, but this works too */
        ovmi.xor_value = read_8bit(0x05,streamFile); /* always zero = easy key */
        ovmi.decryption_callback = eno_ogg_decryption_callback;
        ovmi.meta_type = meta_OGG_ENO;

        start_offset = 0x01;
    }

    /* check GWM [Adagio: Cloudburst (PC)], encrypted */
    if (is_gwm) {
        ovmi.xor_value = 0x5D;
        ovmi.decryption_callback = gwm_ogg_decryption_callback;
        ovmi.meta_type = meta_OGG_GWM;
    }

    /* check .mus [Redux - Dark Matters (PC)], encrypted */
    if (is_mus) {
        ovmi.decryption_callback = mus_ogg_decryption_callback;
        ovmi.meta_type = meta_OGG_MUS;
    }


    return init_vgmstream_ogg_vorbis_callbacks(streamFile, NULL, start_offset, &ovmi);

fail:
    return NULL;
}

VGMSTREAM * init_vgmstream_ogg_vorbis_callbacks(STREAMFILE *streamFile, ov_callbacks *callbacks_p, off_t start, const ogg_vorbis_meta_info_t *ovmi) {
    VGMSTREAM * vgmstream = NULL;
    ogg_vorbis_codec_data * data = NULL;
    OggVorbis_File *ovf = NULL;
    vorbis_info *vi;

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

    /* get info from bitstream 0 */
    data->bitstream = OGG_DEFAULT_BITSTREAM;
    vi = ov_info(ovf,OGG_DEFAULT_BITSTREAM);

    /* search for loop comments */
    {
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

            //;VGM_LOG("OGG: user_comment=%s\n", user_comment);
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(vi->channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->codec_data = data; /* store our fun extra datas */
    vgmstream->channels = vi->channels;
    vgmstream->sample_rate = vi->rate;
    vgmstream->num_streams = ovmi->total_subsongs;
    vgmstream->stream_size = stream_size;

    vgmstream->num_samples = ov_pcm_total(ovf,-1); /* let libvorbisfile find total samples */
    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start;
        if (loop_length_found)
            vgmstream->loop_end_sample = loop_start+loop_length;
        else if (loop_end_found)
            vgmstream->loop_end_sample = loop_end;
        else
            vgmstream->loop_end_sample = vgmstream->num_samples;
        vgmstream->loop_flag = loop_flag;

        if (vgmstream->loop_end_sample > vgmstream->num_samples)
            vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    vgmstream->coding_type = coding_OGG_VORBIS;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = ovmi->meta_type;

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
