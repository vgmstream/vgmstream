#include "meta.h"
#include "../coding/coding.h"
#include "ahx_keys.h"
#include "../util/cri_keys.h"

#ifdef VGM_USE_MPEG
static int find_ahx_key(STREAMFILE* sf, off_t offset, crikey_t* crikey);
#endif


/* AHX - CRI voice format */
VGMSTREAM* init_vgmstream_ahx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t start_offset;
    int channels = 1, loop_flag = 0, type;

    /* checks */
    if (read_u16be(0x00,sf) != 0x8000)
        goto fail;

    if (!check_extensions(sf, "ahx") )
        goto fail;

    start_offset = read_u16be(0x02,sf) + 0x04;
    if (read_u16be(start_offset - 0x06,sf) != 0x2863 ||     /* "(c" */
        read_u32be(start_offset - 0x04,sf) != 0x29435249)   /* ")CRI" */
       goto fail;

    /* types: 0x10 = AHX for DC with fixed MPEG frame bits (bigger frames), 0x11 = standard AHX, 0x0N = ADX */
    type = read_u8(0x04,sf);
    if (type != 0x10 && type != 0x11) goto fail;

    /* frame size (0 for AHX) */
    if (read_u8(0x05,sf) != 0) goto fail;

    /* check for bits per sample? (0 for AHX) */
    if (read_u8(0x06,sf) != 0) goto fail;

    /* check channel count (only mono AHXs can be created by the encoder) */
    if (read_u8(0x07,sf) != 1) goto fail;

    /* check version signature */
    if (read_u8(0x12,sf) != 0x06) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_s32be(0x08,sf); /* real sample rate */
    vgmstream->num_samples = read_s32be(0x0c,sf); /* doesn't include encoder_delay (handled in decoder) */

    vgmstream->meta_type = meta_AHX;

    {
#ifdef VGM_USE_MPEG
        mpeg_custom_config cfg = {0};
        crikey_t* crikey = &cfg.crikey;

        cfg.encryption = read_u8(0x13,sf); /* only type 0x08 is known */
        crikey->type = cfg.encryption;

        if (cfg.encryption) {
            find_ahx_key(sf, start_offset, crikey);
        }

        vgmstream->layout_type = layout_none;
        vgmstream->codec_data = init_mpeg_custom(sf, start_offset, &vgmstream->coding_type, channels, MPEG_AHX, &cfg);
        if (!vgmstream->codec_data) goto fail;
#else
        goto fail;
#endif
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#ifdef VGM_USE_MPEG
static int find_ahx_keyfile(STREAMFILE* sf, crikey_t* crikey) {
    uint8_t keybuf[0x10+1] = {0}; /* approximate max for keystrings, +1 extra null for keystrings */
    size_t key_size;
    int is_keystring = 0;

    key_size = read_key_file(keybuf, sizeof(keybuf) - 1, sf);
    if (key_size <= 0)
        goto fail;


    if (crikey->type == 8) {
        is_keystring = cri_key8_valid_keystring(keybuf, key_size);
    }

    if (key_size == 0x06 && !is_keystring) {
        crikey->key1 = get_u16be(keybuf + 0x00);
        crikey->key2 = get_u16be(keybuf + 0x02);
        crikey->key3 = get_u16be(keybuf + 0x04);
    }
    else if (crikey->type == 8 && is_keystring) {
        const char* keystring = (const char*)keybuf;
        cri_key8_derive(keystring, &crikey->key1, &crikey->key2, &crikey->key3);
    }
    else {
        goto fail;
    }

    return 1;
fail:
    return 0;
}

static int find_ahx_keylist(STREAMFILE* sf, off_t offset, crikey_t* crikey) {
    int i;
    int keycount = ahxkey8_list_count;
    const ahxkey_info* keys = ahxkey8_list;


    for (i = 0; i < keycount; i++) {
        if (crikey->type == 0x08) {
            cri_key8_derive(keys[i].key8, &crikey->key1, &crikey->key2, &crikey->key3);
            //;VGM_LOG("AHX: testing %s [%04x %04x %04x]\n", keys[i].key8, crikey->key1, crikey->key2, crikey->key3);
        }
        else {
            continue;
        }

        if (test_ahx_key(sf, offset, crikey)) {
            //;VGM_LOG("AHX key found\n");
            return 1;
        }
    }

    return 0;
}

static int find_ahx_key(STREAMFILE* sf, off_t offset, crikey_t* crikey) {
    int ok;
    
    ok = find_ahx_keyfile(sf, crikey);
    if (ok)
        return 1;

    ok = find_ahx_keylist(sf, offset, crikey);
    if (ok)
        return 1;


    crikey->key1 = 0;
    crikey->key2 = 0;
    crikey->key3 = 0;
    vgm_logi("AHX: decryption key not found\n");
    return 0;
}
#endif
