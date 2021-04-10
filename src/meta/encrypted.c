#include "meta.h"
#include "../coding/coding.h"
#include "ogg_vorbis_streamfile.h"
#include "encrypted_bgm_streamfile.h"

//todo fuse ogg encryptions and use generic names


static const uint8_t tpf_key[] = {
        0x0a,0x2b,0x36,0x6f,0x0b,0x0a,0x2b,0x36,0x6f,0x0B
};

static void load_key(ogg_vorbis_io_config_data* cfg, const uint8_t* key, size_t size) {
    cfg->is_encrypted = 1;
    cfg->key_len = size;
    memcpy(cfg->key, key, size);
}

/* parser for various encrypted games */
VGMSTREAM* init_vgmstream_encrypted(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    ogg_vorbis_io_config_data cfg = {0};
    uint32_t id;


    /* checks */
    id = read_u32be(0x00, sf);

    if (check_extensions(sf,"ogg,logg")) {
        /* The Pirate's Fate (PC) */
        if (id == 0x454C513C) { /* "OggS" xored */
            load_key(&cfg, tpf_key, sizeof(tpf_key));
        }
        else {
            goto fail;
        }

        temp_sf = setup_ogg_vorbis_streamfile(sf, cfg);
        if (!temp_sf) goto fail;
#ifdef VGM_USE_VORBIS
        vgmstream = init_vgmstream_ogg_vorbis(temp_sf);
#endif
        close_streamfile(temp_sf);
        return vgmstream;
    }

    if (check_extensions(sf,"mp3")) {
        /* The Pirate's Fate (PC) */
        if ((id & 0xFFFFFF00) == 0x436F0500) { /* "ID3\0" xored */
            load_key(&cfg, tpf_key, sizeof(tpf_key));
        }
        else {
            goto fail;
        }

        temp_sf = setup_ogg_vorbis_streamfile(sf, cfg);
        if (!temp_sf) goto fail;

#ifdef VGM_USE_FFMPEG //TODO: allow MP3 without FFmpeg
        vgmstream = init_vgmstream_ffmpeg(temp_sf);
#endif
        close_streamfile(temp_sf);
        return vgmstream;
    }

    if (check_extensions(sf,"wav,lwav")) {
        /* The Pirate's Fate (PC) */
        if (id == 0x58627029) { /* "RIFF" xored */
            load_key(&cfg, tpf_key, sizeof(tpf_key));
        }
        else {
            goto fail;
        }

        temp_sf = setup_ogg_vorbis_streamfile(sf, cfg);
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream_riff(temp_sf);
        close_streamfile(temp_sf);
        return vgmstream;
    }

    if (check_extensions(sf,"bgm")) {
        uint8_t keybuf[0x100];
        size_t key_size;
        off_t start;

        /* Studio Ring games [Nanami to Konomi no Oshiete ABC (PC), Oyatsu no Jikan (PC)] */
        if (id != get_id32be("RIFF"))
            goto fail;

        /* Standard RIFF xor'd past "data", sometimes including extra chunks like JUNK or smpl.
         * If .bgm is added to riff.c this needs to be reworked so detection goes first, or bgm+bgmkey is
         * rejected in riff.c (most files are rejected due to the xor'd extra chunks though). */
        key_size = read_key_file(keybuf, sizeof(keybuf), sf);
        if (key_size <= 0) goto fail;

        if (!find_chunk_le(sf, get_id32be("data"), 0x0c, 0, &start, NULL))
            goto fail;

        temp_sf = setup_bgm_streamfile(sf, start, keybuf, key_size);
        if (!temp_sf) goto fail;

        vgmstream = init_vgmstream_riff(temp_sf);
        close_streamfile(temp_sf);
        return vgmstream;
    }


fail:
    return NULL;
}
