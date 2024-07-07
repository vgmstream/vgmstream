#if 0
#include <stdlib.h>
#include <stdio.h>
#include "../src/api.h"


static void usage(const char* progname) {
    fprintf(stderr, "API test v%08x\n"
            "Usage: %s <infile>\n"
            , libvgmstream_get_version()
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

static libvgmstream_streamfile_t* get_streamfile(const char* filename) {
    return NULL;
}

/* simplistic example of vgmstream's API
 * for something a bit more featured see vgmstream-cli
 */
int main(int argc, char** argv) {
    int err;
    FILE* outfile = NULL;
    const char* infile;
    
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    infile = argv[1];


    // main init
    libvgmstream_t* lib = libvgmstream_init();
    if (!lib) return EXIT_FAILURE;


    // set default config
    libvgmstream_config_t cfg = {
        .loop_count = 2.0,
        .fade_time = 10.0,
    };
    libvgmstream_setup(lib, &cfg);


    // open target file
    libvgmstream_options_t options = {
        .sf = get_streamfile(infile)
    };
    err = libvgmstream_open(lib, &options);
    if (err < 0) goto fail;


    // external SF is not needed after _open
    libvgmstream_streamfile_close(options.sf); 


    // output file
    outfile = get_output_file(infile);
    if (!outfile) goto fail;


    // play file and do something with decoded samples
    while (true) {
        int pos;

        if (lib->decoder->done)
            break;

        // get current samples
        err = libvgmstream_play(lib);
        if (err < 0) goto fail;

        fwrite(lib->decoder->buf, sizeof(uint8_t), lib->decoder->buf_bytes, outfile);

        pos = (int)libvgmstream_play_position(lib);
        printf("\rpos: %d", pos);
        fflush(stdout);
    }
    printf("\n");

    // close current streamfile before opening new ones, optional
    //libvgmstream_close(lib);

    // process done
    libvgmstream_free(lib);
    fclose(outfile);

    printf("done\n");
    return EXIT_SUCCESS;
fail:
    // process failed
    libvgmstream_free(lib);
    fclose(outfile);

    printf("failed!\n");
    return EXIT_FAILURE;
}
#endif
