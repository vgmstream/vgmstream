#include "meta.h"
#include "../coding/coding.h"
#include "ogg_vorbis_streamfile.h"
#include "encrypted_bgm_streamfile.h"
#include "encrypted_mc161_streamfile.h"

//todo fuse ogg encryptions and use generic names


static const uint8_t tpf_key[] = {
        0x0a,0x2b,0x36,0x6f,0x0b,0x0a,0x2b,0x36,0x6f,0x0B
};

static void load_key(ogg_vorbis_io_config_data* cfg, const uint8_t* key, size_t size) {
    cfg->is_encrypted = 1;
    cfg->key_len = size;
    memcpy(cfg->key, key, size);
}

typedef struct {
    uint32_t id;
    uint8_t keybuf[0x100];
    size_t key_size;
    off_t start;

    VGMSTREAM* vgmstream;
    STREAMFILE* temp_sf;
    STREAMFILE* new_sf;
} encrypted_t;


/* The Pirate's Fate (PC) */
static VGMSTREAM* init_vgmstream_encrypted_ogg_tpf(STREAMFILE* sf) {
    ogg_vorbis_io_config_data cfg = {0};
    encrypted_t e = {0};

    e.id = read_u32be(0x00, sf);
    if (e.id != 0x454C513C) /* "OggS" xored */
        goto fail;

    if (!check_extensions(sf,"ogg,logg"))
        goto fail;

    load_key(&cfg, tpf_key, sizeof(tpf_key));

    e.temp_sf = setup_ogg_vorbis_streamfile(sf, &cfg);
    if (!e.temp_sf) goto fail;

    e.vgmstream = init_vgmstream_ogg_vorbis(e.temp_sf);
    close_streamfile(e.temp_sf);
    return e.vgmstream;

fail:
    return NULL;
}

/* The Pirate's Fate (PC) */
static VGMSTREAM* init_vgmstream_encrypted_mp3_tpf(STREAMFILE* sf) {
    ogg_vorbis_io_config_data cfg = {0};
    encrypted_t e = {0};

    e.id = read_u32be(0x00, sf);
    if ((e.id & 0xFFFFFF00) != 0x436F0500) /* "ID3\0" xored */
        goto fail;

    if (!check_extensions(sf,"mp3"))
        goto fail;

    load_key(&cfg, tpf_key, sizeof(tpf_key));

    e.temp_sf = setup_ogg_vorbis_streamfile(sf, &cfg);
    if (!e.temp_sf) goto fail;

#ifdef VGM_USE_FFMPEG //TODO: allow MP3 without FFmpeg
    e.vgmstream = init_vgmstream_ffmpeg(e.temp_sf);
#endif
    close_streamfile(e.temp_sf);
    return e.vgmstream;

fail:
    return NULL;
}

/* Studio Ring games [Nanami to Konomi no Oshiete ABC (PC), Oyatsu no Jikan (PC)] */
static VGMSTREAM* init_vgmstream_encrypted_riff(STREAMFILE* sf) {
    encrypted_t e = {0};

    e.id = read_u32be(0x00, sf);
    if (e.id != get_id32be("RIFF"))
        goto fail;

    /* .bgm: BGM, .mse: SE, .koe: Voice */
    if (!check_extensions(sf,"bgm,mse,koe"))
        goto fail;

    /* Standard RIFF xor'd past "data", sometimes including extra chunks like JUNK or smpl.
     * If .bgm/etc is added to riff.c this needs to be reworked so detection goes first, or bgm+bgmkey is
     * rejected in riff.c (most files are rejected due to the xor'd extra chunks though). */
    e.key_size = read_key_file(e.keybuf, sizeof(e.keybuf), sf);
    if (e.key_size <= 0) goto fail;

    if (!find_chunk_le(sf, get_id32be("data"), 0x0c, 0, &e.start, NULL))
        goto fail;

    e.temp_sf = setup_bgm_streamfile(sf, e.start, e.keybuf, e.key_size);
    if (!e.temp_sf) goto fail;

    e.vgmstream = init_vgmstream_riff(e.temp_sf);
    close_streamfile(e.temp_sf);
    return e.vgmstream;

fail:
    return NULL;
}

/* RPGMVO / Omori (PC) */
static VGMSTREAM* init_vgmstream_encrypted_rpgmvo_riff(STREAMFILE* sf) {
    ogg_vorbis_io_config_data cfg = {0};
    encrypted_t e = {0};
    uint32_t xor;
    uint32_t riff_size;

    if (!is_id64be(0x00, sf, "RPGMV\0\0\0"))
        goto fail;

    if (!check_extensions(sf,"rpgmvo"))
        goto fail;

    /* 0x08: version? */


    /* Ogg .rpgmvo has per-game key, so this is probably the same. Reversing key is simple though,
     * calc suspected key and pass to RIFF to validate (format is the same as Ogg) */
    riff_size = get_streamfile_size(sf) - 0x10;

    xor = read_u32be(0x10, sf);
    xor ^= get_id32be("RIFF");
    put_u32be(e.keybuf + 0x00, xor);

    xor = read_u32le(0x14, sf);
    xor ^= riff_size - 0x08;
    put_u32le(e.keybuf + 0x04, xor);

    xor = read_u32be(0x18, sf);
    xor ^= get_id32be("WAVE");
    put_u32be(e.keybuf + 0x08, xor);

    xor = read_u32be(0x1c, sf);
    xor ^= get_id32be("fmt ");
    put_u32be(e.keybuf + 0x0c, xor);

    e.key_size = 0x10;
    load_key(&cfg, e.keybuf, e.key_size);
    cfg.start = 0x10;
    cfg.max_offset = 0x10;
    
    e.temp_sf = setup_ogg_vorbis_streamfile(sf, &cfg);
    if (!e.temp_sf) goto fail;

    e.new_sf = setup_subfile_streamfile(e.temp_sf, 0x10, riff_size, "wav");
    if (!e.new_sf) goto fail;

    e.vgmstream = init_vgmstream_riff(e.new_sf);
    close_streamfile(e.temp_sf);
    close_streamfile(e.new_sf);
    return e.vgmstream;

fail:
    close_streamfile(e.temp_sf);
    close_streamfile(e.new_sf);
    return NULL;
}


/* Minecraft (PC) before v1.6.1 (Java version) */
static VGMSTREAM* init_vgmstream_encrypted_mc161(STREAMFILE* sf) {
    encrypted_t e = {0};

    if (!check_extensions(sf,"mus"))
        goto fail;

    /* all files use a different key so just fail on meta init */

    e.temp_sf = setup_mc161_streamfile(sf);
    if (!e.temp_sf) goto fail;

    e.vgmstream = init_vgmstream_ogg_vorbis(e.temp_sf);
    close_streamfile(e.temp_sf);
    return e.vgmstream;
fail:
    return NULL;
}


/* parser for various encrypted games */
VGMSTREAM* init_vgmstream_encrypted(STREAMFILE* sf) {
    VGMSTREAM* v = NULL;

    v = init_vgmstream_encrypted_ogg_tpf(sf);
    if (v) return v;

    v = init_vgmstream_encrypted_mp3_tpf(sf);
    if (v) return v;

    v = init_vgmstream_encrypted_riff(sf);
    if (v) return v;

    v = init_vgmstream_encrypted_rpgmvo_riff(sf);
    if (v) return v;

    v = init_vgmstream_encrypted_mc161(sf);
    if (v) return v;

    return NULL;
}
