#include "meta.h"
#include "../coding/coding.h"
#include "../util/chunks.h"
#include "bnsf_keys.h"


#ifdef VGM_USE_G7221
//#define BNSF_BRUTEFORCE
#ifdef BNSF_BRUTEFORCE
static void bruteforce_bnsf_key(STREAMFILE* sf, off_t start, g7221_codec_data* data, uint8_t* best_key);
#endif
static void find_bnsf_key(STREAMFILE *sf, off_t start, g7221_codec_data *data, uint8_t *best_key);
#endif

/* BNSF - Bandai Namco Sound Format/File [Tales of Graces (Wii), Tales of Berseria (PS4)] */
VGMSTREAM* init_vgmstream_bnsf(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset = 0;
    int loop_flag = 0, channels = 0, sample_rate = 0;
    uint32_t num_samples = 0, loop_start = 0, loop_end = 0, loop_adjust = 0, block_samples = 0;
    uint32_t codec, flags = 0;
    size_t bnsf_size, block_size = 0;


    /* checks */
    if (!is_id32be(0x00,sf, "BNSF"))
        goto fail;
    if (!check_extensions(sf,"bnsf"))
        goto fail;

    bnsf_size = read_u32be(0x04,sf);
    codec = read_u32be(0x08,sf);

    if (codec != get_id32be("IS22")) /* uses full size */
        bnsf_size += 0x08;
    if (bnsf_size != get_streamfile_size(sf))
        goto fail;

    {
        enum {
            CHUNK_sfmt = 0x73666d74,
            CHUNK_sdat = 0x73646174,
            CHUNK_loop = 0x6C6F6F70,
        };
        chunk_t rc = {0};

        rc.be_size = 1;
        rc.current = 0x0C;
        while (next_chunk(&rc, sf)) {
            switch(rc.type) {
                case CHUNK_sfmt:
                    flags         = read_u16be(rc.offset+0x00,sf);
                    channels      = read_u16be(rc.offset+0x02,sf);
                    sample_rate   = read_s32be(rc.offset+0x04,sf);
                    num_samples   = read_s32be(rc.offset+0x08,sf);
                    loop_adjust   = read_s32be(rc.offset+0x0c,sf); /* 0 when no loop */
                    block_size    = read_u16be(rc.offset+0x10,sf);
                    block_samples = read_u16be(rc.offset+0x12,sf);
                    //max_samples = sdat_size / block_size * block_samples;
                    break;

                case CHUNK_loop:
                    loop_flag = 1;
                    loop_start = read_s32be(rc.offset+0x00,sf); /* block-aligned */
                    loop_end   = read_s32be(rc.offset+0x04,sf) + 1;
                    break;

                case CHUNK_sdat:
                    start_offset = rc.offset;
                    break;

                default:
                    break;
            }
        }
    }

    if (loop_adjust >= block_samples) /* decoder can't handle this */
        goto fail;
    if (!start_offset)
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start + loop_adjust;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->meta_type = meta_BNSF;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = block_size / channels;

    switch (codec) {
#ifdef VGM_USE_G7221
        case 0x49533134: /* "IS14" (interleaved Siren14) */
            vgmstream->coding_type = coding_G7221C;
            vgmstream->codec_data = init_g7221(vgmstream->channels, vgmstream->interleave_block_size);
            if (!vgmstream->codec_data) goto fail;

            /* get decryption key in .bnsfkey file or list, for later games' voices and some odd BGM
             * [THE iDOL@STER 2 (PS3/X360), Tales of Zestiria (PS3/PC)] */
            if (flags != 0) { /* only known value is 0x02 though */
                size_t keysize;
                uint8_t key[24] = {0}; /* keystring 0-padded to 192-bit */

                keysize = read_key_file(key, sizeof(key), sf);
#ifdef BNSF_BRUTEFORCE
                if (1) {
                    bruteforce_bnsf_key(sf, start_offset, vgmstream->codec_data, key);
                } else
#endif
                if (keysize <= 0 || keysize > sizeof(key)) {
                    find_bnsf_key(sf, start_offset, vgmstream->codec_data, key);
                }
                set_key_g7221(vgmstream->codec_data, key);
            }

            break;
#endif
#ifdef VGM_USE_G719
        case 0x49533232: /* "IS22" (interleaved Siren22) */

            /* same encryption as IS14 but not seen */
            if (flags != 0)
                goto fail;

            vgmstream->coding_type = coding_G719;
            vgmstream->codec_data = init_g719(vgmstream->channels, vgmstream->interleave_block_size);
            if (!vgmstream->codec_data) goto fail;

            break;
#endif
        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

#ifdef VGM_USE_G7221
static inline void test_key(STREAMFILE* sf, off_t start, g7221_codec_data* data, const char* key, int keylen, int* p_best_score, uint8_t* p_best_key) {
    uint8_t tmpkey[24];
    int score;

    if (keylen > sizeof(tmpkey))
        return;
    memcpy(tmpkey, key, keylen);
    memset(tmpkey + keylen, 0, sizeof(tmpkey) - keylen);

    //;VGM_LOG("BNSF: test key=%.24s\n", tmpkey);
    set_key_g7221(data, tmpkey);

    score = test_key_g7221(data, start, sf);
    if (score < 0) return;

    if (*p_best_score <= 0 || (score < *p_best_score && score > 0)) {
        *p_best_score = score;
        memcpy(p_best_key, key, keylen);
        memset(p_best_key + keylen, 0, sizeof(tmpkey) - keylen);
    }
}

static void find_bnsf_key(STREAMFILE* sf, off_t start, g7221_codec_data* data, uint8_t* best_key) {
    const size_t keys_length = sizeof(s14key_list) / sizeof(bnsfkey_info);
    int best_score = -1;
    int i;


    for (i = 0; i < keys_length; i++) {
        const char* key = s14key_list[i].key;
        int keylen = strlen(key);

        test_key(sf, start, data, key, keylen, &best_score, best_key);
        if (best_score == 1)
            break;
    }

    VGM_ASSERT(best_score > 0, "BNSF: best key=%.24s (score=%i)\n", best_key, best_score);
    vgm_asserti(best_score < 0 , "BNSF: decryption key not found\n");
}

#define BNSF_MIN_KEY_LEN 3

#ifdef BNSF_BRUTEFORCE
/* bruteforce keys in a string list extracted from executables or files near sound data, trying variations. */
static void bruteforce_bnsf_key(STREAMFILE* sf, off_t start, g7221_codec_data* data, uint8_t* best_key) {
    STREAMFILE* sf_keys = NULL;
    int best_score = -1;
    int i, j;
    char line[1024];
    int bytes, line_ok;
    off_t offset;
    size_t keys_size;


    VGM_LOG("BNSF: test keys\n");

    sf_keys = open_streamfile_by_filename(sf, "keys.txt");
    if (!sf_keys) goto done;

    keys_size = get_streamfile_size(sf_keys);

    offset = 0x00;
    while (offset < keys_size) {
        int line_len;

        bytes = read_line(line, sizeof(line), offset, sf_keys, &line_ok);
        if (!line_ok) break;

        offset += bytes;

        line_len = strlen(line);
        for (i = 0; i < line_len - BNSF_MIN_KEY_LEN; i++) {
            for (j = i + BNSF_MIN_KEY_LEN; j <= line_len; j++) {
                int keylen = j - i;
                const char* key = &line[i];

                test_key(sf, start, data, key, keylen, &best_score, best_key);
                if (best_score == 1) {
                    VGM_ASSERT(best_score > 0, "BNSF: good key=%.24s (score=%i)\n", best_key, best_score);
                    //goto done;
                }
            }
        }
    }

done:
    VGM_ASSERT(best_score > 0, "BNSF: best key=%.24s (score=%i)\n", best_key, best_score);
    VGM_ASSERT(best_score < 0, "BNSF: key not found\n");

    close_streamfile(sf_keys);
}
#endif

#endif
