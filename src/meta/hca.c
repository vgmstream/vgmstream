#include "meta.h"
#include "hca_keys.h"
#include "../coding/coding.h"

#define HCA_KEY_MAX_TEST_CLIPS   400   /* hopefully nobody masters files with more that a handful... */
#define HCA_KEY_MAX_TEST_FRAMES  100   /* ~102400 samples */
#define HCA_KEY_MAX_TEST_SAMPLES 10240 /* ~10 frames of non-blank samples */

static void find_hca_key(hca_codec_data * hca_data, clHCA * hca, uint8_t * buffer, int header_size, unsigned int * out_key1, unsigned int * out_key2);

VGMSTREAM * init_vgmstream_hca(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    uint8_t buffer[0x8000]; /* hca header buffer data (probably max ~0x400) */
    char filename[PATH_LIMIT];
    off_t start = 0;
    size_t file_size = streamFile->get_size(streamFile);

    int header_size;
    hca_codec_data * hca_data = NULL; /* vgmstream HCA context */
    clHCA * hca; /* HCA_Decoder context */
    unsigned int ciphKey1, ciphKey2;

    /* check extension, case insensitive */
    if ( !check_extensions(streamFile, "hca")) return NULL;


    /* test/init header (find real header size first) */
    if ( file_size < 8 ) goto fail;
    if ( read_streamfile(buffer, start, 8, streamFile) != 8 ) goto fail;

    header_size = clHCA_isOurFile0(buffer);
    if ( header_size < 0 || header_size > 0x8000 ) goto fail;

    if ( read_streamfile(buffer, start, header_size, streamFile) != header_size ) goto fail;
    if ( clHCA_isOurFile1(buffer, header_size) < 0 ) goto fail;


    /* init vgmstream context */
    hca_data = (hca_codec_data *) calloc(1, sizeof(hca_codec_data) + clHCA_sizeof());
    if (!hca_data) goto fail;
    //hca_data->size = file_size;
    hca_data->start = 0;
    hca_data->sample_ptr = clHCA_samplesPerBlock;

    /* HCA_Decoder context memory goes right after our codec data (reserved in alloc'ed) */
    hca = (clHCA *)(hca_data + 1);

    /* pre-load streamfile so the hca_data is ready before key detection */
    streamFile->get_name( streamFile, filename, sizeof(filename) );
    hca_data->streamfile = streamFile->open(streamFile, filename, STREAMFILE_DEFAULT_BUFFER_SIZE);
    if (!hca_data->streamfile) goto fail;


    /* find decryption key in external file or preloaded list */
    {
        uint8_t keybuf[8];
        if (read_key_file(keybuf, 8, streamFile) == 8) {
            ciphKey2 = get_32bitBE(keybuf+0);
            ciphKey1 = get_32bitBE(keybuf+4);
        } else {
            find_hca_key(hca_data, hca, buffer, header_size, &ciphKey1, &ciphKey2);
        }
    }

    /* init decoder with key */
    clHCA_clear(hca, ciphKey1, ciphKey2);
    if ( clHCA_Decode(hca, buffer, header_size, 0) < 0 ) goto fail;
    if ( clHCA_getInfo(hca, &hca_data->info) < 0 ) goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(hca_data->info.channelCount, hca_data->info.loopEnabled);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = hca_data->info.samplingRate;
    vgmstream->num_samples = hca_data->info.blockCount * clHCA_samplesPerBlock;
    vgmstream->loop_start_sample = hca_data->info.loopStart * clHCA_samplesPerBlock;
    vgmstream->loop_end_sample = hca_data->info.loopEnd * clHCA_samplesPerBlock;

    vgmstream->coding_type = coding_CRI_HCA;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_HCA;

    vgmstream->codec_data = hca_data;

    return vgmstream;

fail:
    free(hca_data);
    return NULL;
}


/* Tries to find the decryption key from a list. Simply decodes a few frames and checks if there aren't too many
 * clipped samples, as it's common for invalid keys (though possible with valid keys in poorly mastered files). */
static void find_hca_key(hca_codec_data * hca_data, clHCA * hca, uint8_t * buffer, int header_size, unsigned int * out_key1, unsigned int * out_key2) {
    sample *testbuf = NULL, *temp;
    int i, j, bufsize = 0, tempsize;
    size_t keys_length = sizeof(hcakey_list) / sizeof(hcakey_info);

    int min_clip_count = -1;
    /* defaults to PSO2 key, most common */
    unsigned int best_key2 = 0xCC554639;
    unsigned int best_key1 = 0x30DBE1AB;


    /* find a candidate key */
    for (i = 0; i < keys_length; i++) {
        int clip_count = 0, sample_count = 0;
        int f = 0, s;

        unsigned int key1, key2;
        uint64_t key = hcakey_list[i].key;
        key2 = (key >> 32) & 0xFFFFFFFF;
        key1 = (key >>  0) & 0xFFFFFFFF;


        /* re-init HCA with the current key as buffer becomes invalid (probably can be simplified) */
        hca_data->curblock = 0;
        hca_data->sample_ptr = clHCA_samplesPerBlock;
        if ( read_streamfile(buffer, hca_data->start, header_size, hca_data->streamfile) != header_size ) continue;

        clHCA_clear(hca, key1, key2);
        if (clHCA_Decode(hca, buffer, header_size, 0) < 0) continue;
        if (clHCA_getInfo(hca, &hca_data->info) < 0) continue;
        if (hca_data->info.channelCount > 32) continue; /* nonsense don't alloc too much */

        tempsize = sizeof(sample) * clHCA_samplesPerBlock * hca_data->info.channelCount;
        if (tempsize > bufsize) { /* should happen once */
            temp = (sample *)realloc(testbuf, tempsize);
            if (!temp) goto end;
            testbuf = temp;
            bufsize = tempsize;
        }

        /* test enough frames, but not too many */
        while (f < HCA_KEY_MAX_TEST_FRAMES && f < hca_data->info.blockCount) {
            j = clHCA_samplesPerBlock;
            decode_hca(hca_data, testbuf, j, hca_data->info.channelCount);

            j *= hca_data->info.channelCount;
            for (s = 0; s < j; s++) {
                if (testbuf[s] != 0x0000 && testbuf[s] != 0xFFFF)
                    sample_count++; /* ignore upper/lower blank samples */

                if (testbuf[s] == 0x7FFF || testbuf[s] == 0x8000)
                    clip_count++; /* upper/lower clip */
            }

            if (clip_count > HCA_KEY_MAX_TEST_CLIPS)
                break; /* too many, don't bother */
            if (sample_count >= HCA_KEY_MAX_TEST_SAMPLES)
                break; /* enough non-blank samples tested */

            f++;
        }

        if (min_clip_count < 0 || clip_count < min_clip_count) {
            min_clip_count = clip_count;
            best_key2 = key2;
            best_key1 = key1;
        }

        if (min_clip_count == 0)
            break; /* can't get better than this */

        /* a few clips is normal, but some invalid keys may give low numbers too */
        //if (clip_count < 10)
        //    break;
    }

    /* reset HCA */
    hca_data->curblock = 0;
    hca_data->sample_ptr = clHCA_samplesPerBlock;
    read_streamfile(buffer, hca_data->start, header_size, hca_data->streamfile);

end:
    VGM_ASSERT(min_clip_count > 0, "HCA: best key=%08x%08x (clips=%i)\n", best_key2,best_key1, min_clip_count);
    *out_key2 = best_key2;
    *out_key1 = best_key1;
    free(testbuf);//free(temp);
}
