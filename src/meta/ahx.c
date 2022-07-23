#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"
#if 0
#include "adx_keys.h"
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

    /* types:  0x10 = AHX for DC with bigger frames, 0x11 = AHX, 0x0N = ADX */
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

        cfg.encryption = read_u8(0x13,sf); /* 0x08 = keyword encryption */
        cfg.cri_type = type;

        if (cfg.encryption) {
            uint8_t keybuf[0x10+1] = {0}; /* approximate max for keystrings, +1 extra null for keystrings */
            size_t key_size;

            key_size = read_key_file(keybuf, sizeof(keybuf), sf);
            if (key_size > 0) {
#if 0
                int i, is_ascii;
                is_ascii = 1;
                for (i = 0; i < key_size; i++) {
                    if (keybuf[i] < 0x20 || keybuf[i] > 0x7f) {
                        is_ascii = 0;
                        break;
                    }
                }
#endif
                if (key_size == 0x06 /*&& !is_ascii*/) {
                    cfg.cri_key1 = get_u16be(keybuf + 0x00);
                    cfg.cri_key2 = get_u16be(keybuf + 0x02);
                    cfg.cri_key3 = get_u16be(keybuf + 0x04);
                }
#if 0
                else if (is_ascii) {
                    const char* keystring = (const char*)keybuf;
                    
                    derive_adx_key8(keystring, &cfg.cri_key1, &cfg.cri_key2, &cfg.cri_key3);
                    VGM_LOG("ok: %x, %x, %x\n", cfg.cri_key1, cfg.cri_key2, cfg.cri_key3 );
                }
#endif
            }
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
