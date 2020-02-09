#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"
#include "bnsf_keys.h"


static void find_bnsf_key(g7221_codec_data *data, off_t start, STREAMFILE *sf, uint8_t *best_key);

/* BNSF - Bandai Namco Sound Format/File [Tales of Graces (Wii), Tales of Berseria (PS4)] */
VGMSTREAM * init_vgmstream_bnsf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0, first_offset = 0x0C;
    int loop_flag = 0, channel_count = 0, sample_rate;
    int num_samples, loop_start = 0, loop_end = 0, loop_adjust, block_samples;
    uint32_t codec, flags = 0;
    size_t bnsf_size, sdat_size, block_size;
    off_t loop_chunk = 0, sfmt_chunk, sdat_chunk;


    /* checks */
    if (!check_extensions(streamFile,"bnsf"))
        goto fail;
    if (read_32bitBE(0,streamFile) != 0x424E5346) /* "BNSF" */
        goto fail;

    bnsf_size = read_32bitBE(0x04,streamFile);
    codec = read_32bitBE(0x08,streamFile);

    if (bnsf_size + (codec == 0x49533232 ? 0x00 : 0x08) != get_streamfile_size(streamFile)) /* IS22 uses full size */
        goto fail;

    if (!find_chunk_be(streamFile, 0x73666d74,first_offset,0, &sfmt_chunk,NULL)) /* "sfmt" */
        goto fail;
    if (!find_chunk_be(streamFile, 0x73646174,first_offset,0, &sdat_chunk,&sdat_size)) /* "sdat" */
        goto fail;
    if ( find_chunk_be(streamFile, 0x6C6F6F70,first_offset,0, &loop_chunk,NULL)) { /* "loop" */
        loop_flag = 1;
        loop_start = read_32bitBE(loop_chunk+0x00,streamFile); /* block-aligned */
        loop_end   = read_32bitBE(loop_chunk+0x04,streamFile) + 1;
    }

    flags         = read_16bitBE(sfmt_chunk+0x00,streamFile);
    channel_count = read_16bitBE(sfmt_chunk+0x02,streamFile);
    sample_rate   = read_32bitBE(sfmt_chunk+0x04,streamFile);
    num_samples   = read_32bitBE(sfmt_chunk+0x08,streamFile);
    loop_adjust   = read_32bitBE(sfmt_chunk+0x0c,streamFile); /* 0 when no loop */
    block_size    = read_16bitBE(sfmt_chunk+0x10,streamFile);
    block_samples = read_16bitBE(sfmt_chunk+0x12,streamFile);
    //max_samples = sdat_size / block_size * block_samples;

    start_offset = sdat_chunk;

    if (loop_adjust >= block_samples) /* decoder can't handle this */
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start + loop_adjust;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->meta_type = meta_BNSF;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = block_size / channel_count;

    switch (codec) {
#ifdef VGM_USE_G7221
        case 0x49533134: /* "IS14" (interleaved Siren14) */
            vgmstream->coding_type = coding_G7221C;
            vgmstream->codec_data = init_g7221(vgmstream->channels, vgmstream->interleave_block_size);
            if (!vgmstream->codec_data) goto fail;

            /* get decryption key in .bnsfkey file or list, for later games' voices
             * [The Idolm@ster 2 (PS3/X360), Tales of Zestiria (PS3/PC)] */
            if (flags != 0) { /* only known value is 0x02 though */
                size_t keysize;
                uint8_t key[24] = {0}; /* keystring 0-padded to 192-bit */

                keysize = read_key_file(key, sizeof(key), streamFile);
                if (keysize <= 0 || keysize > sizeof(key)) {
                    find_bnsf_key(vgmstream->codec_data, start_offset, streamFile, key);
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


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

static void find_bnsf_key(g7221_codec_data* data, off_t start, STREAMFILE* sf, uint8_t* best_key) {
    const size_t keys_length = sizeof(s14key_list) / sizeof(bnsfkey_info);
    int score, best_score = -1;
    int i;
    uint8_t tmpkey[24];


    for (i = 0; i < keys_length; i++) {
        const char* key = s14key_list[i].key;
        int keylen = strlen(key);

        if (keylen > sizeof(tmpkey))
            continue;
        memcpy(tmpkey, key, keylen);
        memset(tmpkey + keylen, 0, sizeof(tmpkey) - keylen);

        //;VGM_LOG("BNSF: test key=%.24s\n", tmpkey);
        set_key_g7221(data, tmpkey);

        score = test_key_g7221(data, start, sf);
        if (score < 0) continue;

        if (best_score <= 0 || (score < best_score && score > 0)) {
            best_score = score;
            memcpy(best_key, key, keylen);
            memset(best_key + keylen, 0, sizeof(tmpkey) - keylen);
        }

        if (best_score == 1) {
            break;
        }

    }

    VGM_ASSERT(best_score > 0, "BNSF: best key=%.24s (score=%i)\n", best_key, best_score);
    VGM_ASSERT(best_score < 0, "BNSF: key not found\n"); /* defaults to all 0s */
}
