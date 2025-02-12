#include "../src/libvgmstream.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "../src/base/api_internal.h"
#include "../src/streamfile.h"


static void usage(const char* progname) {
    fprintf(stderr, "Usage: %s <infile>\n"
            , progname
    );
}

static FILE* get_output_file(const char* filename) {
    char out_filename[0x7FFF];
    snprintf(out_filename, sizeof(out_filename), "%s.pcm", filename);

    FILE* outfile = fopen(out_filename, "wb");
    if (!outfile) {
        fprintf(stderr, "failed to open %s for output\n", out_filename);
    }
    return outfile;
}

static libstreamfile_t* get_streamfile(const char* filename) {
    return libstreamfile_open_from_stdio(filename);
}

static int api_example(const char* infile) {
    VGM_STEP();
    int err;
    FILE* outfile = NULL;
    
    bool fill_test = true;
    int fill_pcm16_samples;
    int fill_pcm16_bytes;
    short* fill_pcm16 = NULL;


    // main init
    libvgmstream_t* lib = libvgmstream_init();
    if (!lib) return EXIT_FAILURE;


    // set default config
    libvgmstream_config_t cfg = {
        //.loop_count = 1.0,
        //.fade_time = 10.0,
        .ignore_loop = true,
        .force_sfmt = LIBVGMSTREAM_SFMT_PCM16,
    };
    libvgmstream_setup(lib, &cfg);


    // open target file
    libstreamfile_t* sf = get_streamfile(infile);
    err = libvgmstream_open_stream(lib, sf, 0);
    // external SF is not needed after _open
    libstreamfile_close(sf); 

    if (err < 0) {
        printf("not a valid file\n");
        goto fail;
    }

    // output file
    outfile = get_output_file(infile);
    if (!outfile) goto fail;

    {
        char title[128];
        libvgmstream_title_t cfg = {
            .filename = infile,
            .remove_extension = true,
        };
        libvgmstream_get_title(lib, &cfg, title, sizeof(title));

        printf("- format title:\n");
        printf("%s\n", title);
    }

    {
        char describe[1024];
        libvgmstream_format_describe(lib, describe, sizeof(describe));

        printf("- format describe:\n");
        printf("%s\n", describe);
    }

    printf("- format info\n");
    printf("channels: %i\n", lib->format->channels);
    printf("sample rate: %i\n", lib->format->sample_rate);
    printf("codec: %s\n", lib->format->codec_name);
    printf("samples: %i\n", (int32_t)lib->format->stream_samples);
    printf("\n");

    printf("- decoding: %i\n" , (int32_t)lib->format->play_samples);

    fill_pcm16_samples = 576; //non-aligned samples for testing output
    fill_pcm16_bytes = fill_pcm16_samples * sizeof(short) * lib->format->channels;
    fill_pcm16 = malloc(fill_pcm16_bytes);
    if (!fill_pcm16) goto fail;

    // play file and do something with decoded samples
    while (!lib->decoder->done) {
        //int pos;
        void* buf;
        int buf_bytes = 0;

        // get current samples
        if (fill_test) {
            err = libvgmstream_fill(lib, fill_pcm16, fill_pcm16_samples);
            if (err < 0) goto fail;

            buf = lib->decoder->buf;
            buf_bytes = lib->decoder->buf_bytes;
        }
        else {
            err = libvgmstream_render(lib);
            if (err < 0) goto fail;

            buf = lib->decoder->buf;
            buf_bytes = lib->decoder->buf_bytes;
        }

        fwrite(buf, sizeof(uint8_t), buf_bytes, outfile);

        //pos = (int)libvgmstream_get_play_position(lib);
        //printf("\r- decode pos: %d", pos);
        //fflush(stdout);
    }
    printf("\n");

    // close current streamfile before opening new ones, optional
    //libvgmstream_close_stream(lib);

    // process done
    libvgmstream_free(lib);
    fclose(outfile);
    free(fill_pcm16);

    printf("done\n");
    return EXIT_SUCCESS;
fail:
    // process failed
    libvgmstream_free(lib);
    fclose(outfile);
    free(fill_pcm16);

    printf("failed!\n");
    return EXIT_FAILURE;
}


static void test_lib_is_valid() {
    VGM_STEP();

    bool expected, result;

    expected = true;
    result = libvgmstream_is_valid("test.adx", NULL);
    assert(expected == result);

    expected = true;
    result = libvgmstream_is_valid("extensionless", NULL);
    assert(expected == result);

    libvgmstream_valid_t cfg = {
        .reject_extensionless = true
    };
    expected = false;
    result = libvgmstream_is_valid("extensionless", &cfg);
    assert(expected == result);
}

static void test_lib_extensions() {
    VGM_STEP();

    const char** exts;
    int size = 0;

    size = 0;
    exts = libvgmstream_get_extensions(&size);
    assert(exts != NULL && size > 100);

    exts = libvgmstream_get_extensions(NULL);
    assert(exts == NULL);

    size = 0;
    exts = libvgmstream_get_common_extensions(&size);
    assert(exts != NULL && size > 1 && size < 100);

    exts = libvgmstream_get_common_extensions(NULL);
    assert(exts == NULL);
}

static libstreamfile_t* test_libsf_open() {
    VGM_STEP();

    libstreamfile_t* libsf = NULL;

    libsf = libstreamfile_open_from_stdio("api.bin_wrong");
    assert(libsf == NULL);

    libsf = libstreamfile_open_from_stdio("api.bin");
    assert(libsf != NULL);

    return libsf;
}


static void test_libsf_read(libstreamfile_t* libsf) {
    VGM_STEP();

    int read;
    uint8_t buf[0x20];

    read = libsf->read(libsf->user_data, buf, sizeof(buf));
    assert(read == sizeof(buf));
    for (int  i = 0; i < sizeof(buf); i++) {
        assert(buf[i] == ((i + 0x00) & 0xFF));
    }

    read = libsf->read(libsf->user_data, buf, sizeof(buf));
    assert(read == sizeof(buf));
    for (int  i = 0; i < sizeof(buf); i++) {
        assert(buf[i] == ((i + 0x20) & 0xFF));
    }
}

static void test_libsf_seek_read(libstreamfile_t* libsf) {
    VGM_STEP();

    int read, res;
    uint8_t buf[0x20];

    res = libsf->seek(libsf->user_data, 0x19BC0, 0);
    assert(res == 0);

    read = libsf->read(libsf->user_data, buf, sizeof(buf));
    assert(read == sizeof(buf));
    for (int  i = 0; i < sizeof(buf); i++) {
        assert(buf[i] == ((i + 0xC0) & 0xFF));
    }

    res = libsf->seek(libsf->user_data, 0x7FFFFF, 0);
    assert(res == 0);

    read = libsf->read(libsf->user_data, buf, sizeof(buf));
    assert(read == 0);
}

static void test_libsf_size(libstreamfile_t* libsf) {
    VGM_STEP();

    int64_t size = libsf->get_size(libsf->user_data);
    assert(size == 0x20000);
}

static void test_libsf_name(libstreamfile_t* libsf) {
    VGM_STEP();

    const char* name = libsf->get_name(libsf->user_data);
    assert(strcmp(name, "api.bin") == 0);
}

static void test_libsf_reopen(libstreamfile_t* libsf) {
    VGM_STEP();

    uint8_t buf[0x20];
    int read;

    libstreamfile_t* newsf = NULL;

    newsf = libsf->open(libsf->user_data, "api2.bin_wrong");
    assert(newsf == NULL);

    newsf = libsf->open(libsf->user_data, "api2.bin");
    assert(newsf != NULL);

    read = newsf->read(newsf->user_data, buf, sizeof(buf));
    assert(read == sizeof(buf));
    assert(buf[0x10] == 0x10);

    newsf->close(newsf);
}

static void test_libsf_apisf(libstreamfile_t* libsf) {
    VGM_STEP();

    STREAMFILE* sf = open_api_streamfile(libsf);
    assert(sf != NULL);

    int read;
    uint8_t buf[0x20];

    read = read_streamfile(buf, 0xF0, sizeof(buf), sf);
    assert(read == sizeof(buf));
    for (int  i = 0; i < sizeof(buf); i++) {
        assert(buf[i] == ((i + 0xF0) & 0xFF));
    }

    size_t size = get_streamfile_size(sf);
    assert(size == 0x20000);

    close_streamfile(sf);

}


static void test_lib_streamfile() {
    VGM_STEP();

    libstreamfile_t* libsf = test_libsf_open();
    test_libsf_read(libsf);
    test_libsf_seek_read(libsf);
    test_libsf_size(libsf);
    test_libsf_name(libsf);
    test_libsf_reopen(libsf);
    test_libsf_apisf(libsf);

    // close
    libsf->close(libsf);
}

static void test_lib_tags() {
    VGM_STEP();

    libstreamfile_t* libsf = NULL;
    libvgmstream_tags_t* tags = NULL;
    bool more = false;

    libsf = libstreamfile_open_from_stdio("sample_!tags.m3u");
    assert(libsf != NULL);

    tags = libvgmstream_tags_init(libsf);
    assert(tags != NULL);


    libvgmstream_tags_find(tags, "filename1.adx");
    more = libvgmstream_tags_next_tag(tags);
    assert(more && strcmp(tags->key, "ARTIST") == 0 && strcmp(tags->val, "global artist") == 0);
    more = libvgmstream_tags_next_tag(tags);
    assert(more && strcmp(tags->key, "TITLE") == 0 && strcmp(tags->val, "filename1 title") == 0);
    more = libvgmstream_tags_next_tag(tags);
    assert(!more);


    libvgmstream_tags_find(tags, "filename2.adx");
    more = libvgmstream_tags_next_tag(tags);
    assert(more && strcmp(tags->key, "ARTIST") == 0 && strcmp(tags->val, "global artist") == 0);
    more = libvgmstream_tags_next_tag(tags);
    assert(more && strcmp(tags->key, "TITLE") == 0 && strcmp(tags->val, "filename2 title") == 0);
    more = libvgmstream_tags_next_tag(tags);
    assert(!more);


    libvgmstream_tags_find(tags, "filename3.adx");
    more = libvgmstream_tags_next_tag(tags);
    assert(more && strcmp(tags->key, "ARTIST") == 0 && strcmp(tags->val, "global artist") == 0);
    more = libvgmstream_tags_next_tag(tags);
    assert(!more);


    libvgmstream_tags_find(tags, "filename_incorrect.adx");
    more = libvgmstream_tags_next_tag(tags);
    assert(more && strcmp(tags->key, "ARTIST") == 0 && strcmp(tags->val, "global artist") == 0);
    more = libvgmstream_tags_next_tag(tags);
    assert(!more);

    libvgmstream_tags_free(tags);
    libstreamfile_close(libsf);
}


/* simplistic example of vgmstream's API
 * for something a bit more featured see vgmstream-cli
 */
int main(int argc, char** argv) {
    printf("API v%08x test\n", libvgmstream_get_version());

    libvgmstream_set_log(0, NULL);

    test_lib_is_valid();
    test_lib_extensions();
    test_lib_streamfile();
    test_lib_tags();

    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    api_example(argv[1]);

    return 0;
}
