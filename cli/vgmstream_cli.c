/**
 * vgmstream CLI decoder
 */
#define POSIXLY_CORRECT

#include <getopt.h>
#include "../src/vgmstream.h"
#include "../src/plugins.h"
#include "../src/util.h"
//todo use <>?
#ifdef HAVE_JSON
#include "jansson/jansson.h"
#endif

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif


#include "../version.h"
#ifndef VGMSTREAM_VERSION
#define VGMSTREAM_VERSION "unknown version " __DATE__
#endif
#define APP_NAME  "vgmstream CLI decoder " VGMSTREAM_VERSION
#define APP_INFO  APP_NAME " (" __DATE__ ")"


/* Low values are ok as there is very little performance difference, but higher
 * may improve write I/O in some systems as this*channels doubles as output buffer
 * For systems with less memory (like wasm without -s ALLOW_MEMORY_GROWTH) lower helps a bit. */
//TODO: make it selectable with -n? in the future may just use internal bufs for min memory
#ifdef __EMSCRIPTEN__
    #define SAMPLE_BUFFER_SIZE  1024
#else
    #define SAMPLE_BUFFER_SIZE  32768
#endif

/* getopt globals from .h, for reference (the horror...) */
//extern char* optarg;
//extern int optind, opterr, optopt;


static size_t make_wav_header(uint8_t* buf, size_t buf_size, int32_t sample_count, int32_t sample_rate, int channels, int smpl_chunk, int32_t loop_start, int32_t loop_end);

static void usage(const char* progname, int is_help) {

    fprintf(is_help ? stdout : stderr, APP_INFO "\n"
            "Usage: %s [-o <outfile.wav>] [options] <infile> ...\n"
            "Options:\n"
            "    -o <outfile.wav>: name of output .wav file, default <infile>.wav\n"
            "       <outfile> wildcards can be ?s=subsong, ?n=stream name, ?f=infile\n"
            "    -m: print metadata only, don't decode\n"
            "    -i: ignore looping information and play the whole stream once\n"
            "    -l N.n: loop count, default 2.0\n"
            "    -f N.n: fade time in seconds after N loops, default 10.0\n"
            "    -d N.n: fade delay in seconds, default 0.0\n"
            "    -F: don't fade after N loops and play the rest of the stream\n"
            "    -e: set end-to-end looping (if file has no loop points)\n"
            "    -E: force end-to-end looping even if file has real loop points\n"
            "    -s N: select subsong N, if the format supports multiple subsongs\n"
            "    -S N: select end subsong N (set 0 for 'all')\n"
            "    -L: append a smpl chunk and create a looping wav\n"
            "    -p: output to stdout (for piping into another program)\n"
            "    -P: output to stdout even if stdout is a terminal\n"
            "    -c: loop forever (continuously) to stdout\n"
            "    -h: print all commands\n"
#ifdef HAVE_JSON
            "    -V: print version info and supported extensions as JSON\n"
            "    -I: print requested file info as JSON\n"
#endif
            , progname);
    if (!is_help)
        return;
    fprintf(is_help ? stdout : stderr,
            "    -2 N: only output the Nth (first is 0) set of stereo channels\n"
            "    -x: decode and print adxencd command line to encode as ADX\n"
            "    -g: decode and print oggenc command line to encode as OGG\n"
            "    -b: decode and print batch variable commands\n"
            "    -v: validate extensions (for extension testing)\n"
            "    -r: reset and output a second file (for reset testing)\n"
            "    -k N: kills (seeks) N samples before decoding (for seek testing)\n"
            "    -K N: kills (seeks) again to N samples before decoding (for seek testing)\n"
            "    -t: print !tags found in !tags.m3u (for tag testing)\n"
            "    -T: print title (for title testing)\n"
            "    -D <max channels>: downmix to <max channels> (for plugin downmix testing)\n"
            "    -O: decode but don't write to file (for performance testing)\n"
    );

}


typedef struct {
    char** infilenames;
    int infilenames_count;
    const char* infilename;

    const char* outfilename_config;
    const char* outfilename;

    const char* tag_filename;

    int play_forever;
    int play_sdtout;
    int play_wreckless;
    int print_metaonly;
#ifdef HAVE_JSON
    int print_metajson;
#endif
    int print_adxencd;
    int print_oggenc;
    int print_batchvar;
    int write_lwav;
    int only_stereo;
    int subsong_index;
    int subsong_end;

    double loop_count;
    double fade_time;
    double fade_delay;
    int ignore_fade;
    int ignore_loop;
    int force_loop;
    int really_force_loop;

    int validate_extensions;
    int test_reset;
    int seek_samples1;
    int seek_samples2;
    int decode_only;
    int show_title;
    int downmix_channels;

    /* not quite config but eh */
    int lwav_loop_start;
    int lwav_loop_end;
} cli_config;
#ifdef HAVE_JSON
static void print_json_version();
static void print_json_info(VGMSTREAM* vgm, cli_config* cfg);
#endif


static int parse_config(cli_config* cfg, int argc, char** argv) {
    int opt;

    /* non-zero defaults */
    cfg->only_stereo = -1;
    cfg->loop_count = 2.0;
    cfg->fade_time = 10.0;
    cfg->seek_samples1 = -1;
    cfg->seek_samples2 = -1;

    opterr = 0; /* don't let getopt print errors to stdout automatically */
    optind = 1; /* reset getopt's ugly globals (needed in wasm that may call same main() multiple times) */

    /* read config */
    while ((opt = getopt(argc, argv, "o:l:f:d:ipPcmxeLEFrgb2:s:tTk:K:hOvD:S:"
#ifdef HAVE_JSON
        "VI"
#endif
    )) != -1) {
        switch (opt) {
            case 'o':
                cfg->outfilename = optarg;
                break;
            case 'l':
                //cfg->loop_count = strtod(optarg, &end); //C99, allow?
                //if (*end != '\0') goto fail_arg;
                cfg->loop_count = atof(optarg);
                break;
            case 'f':
                cfg->fade_time = atof(optarg);
                break;
            case 'd':
                cfg->fade_delay = atof(optarg);
                break;
            case 'i':
                cfg->ignore_loop = 1;
                break;
            case 'p':
                cfg->play_sdtout = 1;
                break;
            case 'P':
                cfg->play_wreckless = 1;
                cfg->play_sdtout = 1;
                break;
            case 'c':
                cfg->play_forever = 1;
                break;
            case 'm':
                cfg->print_metaonly = 1;
                break;
            case 'x':
                cfg->print_adxencd = 1;
                break;
            case 'g':
                cfg->print_oggenc = 1;
                break;
            case 'b':
                cfg->print_batchvar = 1;
                break;
            case 'e':
                cfg->force_loop = 1;
                break;
            case 'E':
                cfg->really_force_loop = 1;
                break;
            case 'L':
                cfg->write_lwav = 1;
                break;
            case 'r':
                cfg->test_reset = 1;
                break;
            case '2':
                cfg->only_stereo = atoi(optarg);
                break;
            case 'F':
                cfg->ignore_fade = 1;
                break;
            case 's':
                cfg->subsong_index = atoi(optarg);
                break;
            case 'S':
                cfg->subsong_end = atoi(optarg);
                if (!cfg->subsong_end)
                    cfg->subsong_end = -1; /* signal up to end (otherwise 0 = not set) */
                if (!cfg->subsong_index)
                    cfg->subsong_index = 1;
                break;
            case 't':
                cfg->tag_filename = "!tags.m3u";
                break;
            case 'T':
                cfg->show_title = 1;
                break;
            case 'k':
                cfg->seek_samples1 = atoi(optarg);
                break;
            case 'K':
                cfg->seek_samples2 = atoi(optarg);
                break;
            case 'O':
                cfg->decode_only = 1;
                break;
            case 'v':
                cfg->validate_extensions = 1;
                break;
            case 'D':
                cfg->downmix_channels = atoi(optarg);
                break;
            case 'h':
                usage(argv[0], 1);
                goto fail;
#ifdef HAVE_JSON
            case 'V':
                print_json_version();
                goto fail;
            case 'I':
                cfg->print_metajson = 1;
                break;
#endif
            case '?':
                fprintf(stderr, "missing argument or unknown option -%c\n", optopt);
                goto fail;
            default:
                usage(argv[0], 0);
                goto fail;
        }
    }

    /* filenames go last in POSIX getopt, not so in glibc getopt */ //TODO unify
    if (optind != argc - 1) {
        int i;

        /* check there aren't commands after filename */
        for (i = optind; i < argc; i++) {
            if (argv[i][0] == '-') {
                fprintf(stderr, "input files must go after options\n");
                goto fail;
            }
        }
    }

    cfg->infilenames = &argv[optind];
    cfg->infilenames_count = argc - optind;
    if (cfg->infilenames_count <= 0) {
        fprintf(stderr, "missing input file\n");
        usage(argv[0], 0);
        goto fail;
    }

    if (cfg->outfilename && strchr(cfg->outfilename, '?') != NULL) {
        cfg->outfilename_config = cfg->outfilename;
        cfg->outfilename = NULL;
    }

    return 1;
fail:
    return 0;
}

static int validate_config(cli_config* cfg) {
    if (cfg->play_sdtout && (!cfg->play_wreckless && isatty(STDOUT_FILENO))) {
        fprintf(stderr, "Are you sure you want to output wave data to the terminal?\nIf so use -P instead of -p.\n");
        goto fail;
    }
    if (cfg->play_forever && !cfg->play_sdtout) {
        fprintf(stderr, "-c must use -p or -P\n");
        goto fail;
    }
    if (cfg->play_sdtout && cfg->outfilename) {
        fprintf(stderr, "use either -p or -o\n");
        goto fail;
    }

    /* other options have built-in priority defined */

    return 1;
fail:
    return 0;
}

static void print_info(VGMSTREAM* vgmstream, cli_config* cfg) {
    int channels = vgmstream->channels;
    if (!cfg->play_sdtout) {
        if (cfg->print_adxencd) {
            printf("adxencd");
            if (!cfg->print_metaonly)
                printf(" \"%s\"",cfg->outfilename);
            if (vgmstream->loop_flag)
                printf(" -lps%d -lpe%d", vgmstream->loop_start_sample, vgmstream->loop_end_sample);
            printf("\n");
        }
        else if (cfg->print_oggenc) {
            printf("oggenc");
            if (!cfg->print_metaonly)
                printf(" \"%s\"", cfg->outfilename);
            if (vgmstream->loop_flag)
                printf(" -c LOOPSTART=%d -c LOOPLENGTH=%d", vgmstream->loop_start_sample, vgmstream->loop_end_sample-vgmstream->loop_start_sample);
            printf("\n");
        }
        else if (cfg->print_batchvar) {
            if (!cfg->print_metaonly)
                printf("set fname=\"%s\"\n", cfg->outfilename);
            printf("set tsamp=%d\nset chan=%d\n", vgmstream->num_samples, channels);
            if (vgmstream->loop_flag)
                printf("set lstart=%d\nset lend=%d\nset loop=1\n", vgmstream->loop_start_sample, vgmstream->loop_end_sample);
            else
                printf("set loop=0\n");
        }
        else if (cfg->print_metaonly) {
            printf("metadata for %s\n", cfg->infilename);
        }
        else {
            printf("decoding %s\n", cfg->infilename);
        }
    }

    if (!cfg->play_sdtout && !cfg->print_adxencd && !cfg->print_oggenc && !cfg->print_batchvar) {
        char description[1024];
        description[0] = '\0';
        describe_vgmstream(vgmstream,description,1024);
        printf("%s",description);
    }
}


static void apply_config(VGMSTREAM* vgmstream, cli_config* cfg) {
    vgmstream_cfg_t vcfg = {0};

    /* write loops in the wav, but don't actually loop it */
    if (cfg->write_lwav) {
        vcfg.disable_config_override = 1;
        cfg->ignore_loop = 1;

        if (vgmstream->loop_start_sample < vgmstream->loop_end_sample) {
            cfg->lwav_loop_start = vgmstream->loop_start_sample;
            cfg->lwav_loop_end = vgmstream->loop_end_sample;
            cfg->lwav_loop_end--; /* from spec, +1 is added when reading "smpl" */
        }
        else {
            /* reset for subsongs */
            cfg->lwav_loop_start = 0;
            cfg->lwav_loop_end = 0;
        }
    }
    /* only allowed if manually active */
    if (cfg->play_forever) {
        vcfg.allow_play_forever = 1;
    }

    vcfg.play_forever = cfg->play_forever;
    vcfg.fade_time = cfg->fade_time;
    vcfg.loop_count = cfg->loop_count;
    vcfg.fade_delay = cfg->fade_delay;

    vcfg.ignore_loop  = cfg->ignore_loop;
    vcfg.force_loop = cfg->force_loop;
    vcfg.really_force_loop = cfg->really_force_loop;
    vcfg.ignore_fade = cfg->ignore_fade;

    vgmstream_apply_config(vgmstream, &vcfg);
}


static void print_tags(cli_config* cfg) {
    VGMSTREAM_TAGS* tags = NULL;
    STREAMFILE* sf_tags = NULL;
    const char *tag_key, *tag_val;

    if (!cfg->tag_filename)
        return;

    sf_tags = open_stdio_streamfile(cfg->tag_filename);
    if (!sf_tags) {
        printf("tag file %s not found\n", cfg->tag_filename);
        return;
    }

    printf("tags:\n");

    tags = vgmstream_tags_init(&tag_key, &tag_val);
    vgmstream_tags_reset(tags, cfg->infilename);
    while (vgmstream_tags_next_tag(tags, sf_tags)) {
        printf("- '%s'='%s'\n", tag_key, tag_val);
    }

    vgmstream_tags_close(tags);
    close_streamfile(sf_tags);
}

static void print_title(VGMSTREAM* vgmstream, cli_config* cfg) {
    char title[1024];
    vgmstream_title_t tcfg = {0};

    if (!cfg->show_title)
        return;

    tcfg.force_title = 0;
    tcfg.subsong_range = 0;
    tcfg.remove_extension = 0;

    vgmstream_get_title(title, sizeof(title), cfg->infilename, vgmstream, &tcfg);

    printf("title: %s\n", title);
}

#ifdef HAVE_JSON
void print_json_version() {
    size_t extension_list_len;
    size_t common_extension_list_len;
    const char** extension_list;
    const char** common_extension_list;
    extension_list = vgmstream_get_formats(&extension_list_len);
    common_extension_list = vgmstream_get_common_formats(&common_extension_list_len);

    json_t* ext_list = json_array();
    json_t* cext_list = json_array();

    for (size_t i = 0; i < extension_list_len; ++i) {
        json_t* ext = json_string(extension_list[i]);
        json_array_append(ext_list, ext);
    }

    for (size_t i = 0; i < common_extension_list_len; ++i) {
        json_t* cext = json_string(common_extension_list[i]);
        json_array_append(cext_list, cext);
    }

    json_t* version_string = json_string(VGMSTREAM_VERSION);

    json_t* final_object = json_object();
    json_object_set(final_object, "version", version_string);
    json_decref(version_string);

    json_object_set(final_object, "extensions",
                    json_pack("{soso}",
                              "vgm", ext_list,
                              "common", cext_list));

    json_dumpf(final_object, stdout, JSON_COMPACT);
}
#endif

static void clean_filename(char* dst, int clean_paths) {
    int i;
    for (i = 0; i < strlen(dst); i++) {
        char c = dst[i];
        int is_badchar = (clean_paths && (c == '\\' || c == '/'))
            || c == '*' || c == '?' || c == ':' /*|| c == '|'*/ || c == '<' || c == '>';
        if (is_badchar)
            dst[i] = '_';
    }
}

/* replaces a filename with "?n" (stream name), "?f" (infilename) or "?s" (subsong) wildcards
 * ("?" was chosen since it's not a valid Windows filename char and hopefully nobody uses it on Linux) */
static void replace_filename(char* dst, size_t dstsize, cli_config* cfg, VGMSTREAM* vgmstream) {
    int subsong;
    char stream_name[PATH_LIMIT];
    char buf[PATH_LIMIT];
    char tmp[PATH_LIMIT];
    int i;


    /* file has a "%" > temp replace for sprintf */
    strcpy(buf, cfg->outfilename_config);
    for (i = 0; i < strlen(buf); i++) {
        if (buf[i] == '%')
            buf[i] = '|'; /* non-valid filename, not used in format */
    }

    /* init config */
    subsong = vgmstream->stream_index;
    if (subsong > vgmstream->num_streams || subsong != cfg->subsong_index) {
        subsong = 0; /* for games without subsongs / bad config */
    }

    if (vgmstream->stream_name[0] != '\0') {
        snprintf(stream_name, sizeof(stream_name), "%s", vgmstream->stream_name);
        clean_filename(stream_name, 1); /* clean subsong name's subdirs */
    }
    else {
        snprintf(stream_name, sizeof(stream_name), "%s", cfg->infilename);
        clean_filename(stream_name, 0); /* don't clean user's subdirs */
    }

    /* do controlled replaces of each wildcard (in theory could appear N times) */
    do {
        char* pos = strchr(buf, '?');
        if (!pos)
            break;

        /* use buf as format and copy formatted result to tmp (assuming sprintf's format must not overlap with dst) */
        if (pos[1] == 'n') {
            pos[0] = '%';
            pos[1] = 's'; /* use %s */
            snprintf(tmp, sizeof(tmp), buf, stream_name);
        }
        else if (pos[1] == 'f') {
            pos[0] = '%';
            pos[1] = 's'; /* use %s */
            snprintf(tmp, sizeof(tmp), buf, cfg->infilename);
        }
        else if (pos[1] == 's') {
            pos[0] = '%';
            pos[1] = 'i'; /* use %i */
            snprintf(tmp, sizeof(tmp), buf, subsong);
        }
        else if ((pos[1] == '0' && pos[2] >= '1' && pos[2] <= '9' && pos[3] == 's')) {
            pos[0] = '%';
            pos[3] = 'i'; /* use %0Ni */
            snprintf(tmp, sizeof(tmp), buf, subsong);
        }
        else {
            /* not recognized */
            continue;
        }

        /* copy result to buf again, so it can be used as format in next replace
         * (can be optimized with some pointer swapping but who cares about a few extra nanoseconds) */
        strcpy(buf, tmp);
    }
    while (1);

    /* replace % back */
    for (i = 0; i < strlen(buf); i++) {
        if (buf[i] == '|')
            buf[i] = '%';
    }

    snprintf(dst, dstsize, "%s", buf);
}


/* ************************************************************ */

static int convert_file(cli_config* cfg);
static int convert_subsongs(cli_config* cfg);
static int write_file(VGMSTREAM* vgmstream, cli_config* cfg);


int main(int argc, char** argv) {
    cli_config cfg = {0};
    int i, res, ok;


    /* read args */
    res = parse_config(&cfg, argc, argv);
    if (!res) goto fail;

#ifdef WIN32
    /* make stdout output work with windows */
    if (cfg.play_sdtout) {
        _setmode(fileno(stdout),_O_BINARY);
    }
#endif

    res = validate_config(&cfg);
    if (!res) goto fail;

    ok = 0;
    for (i = 0; i < cfg.infilenames_count; i++) {
        /* current name, to avoid passing params all the time */
        cfg.infilename = cfg.infilenames[i];
        if (cfg.outfilename_config)
            cfg.outfilename = NULL;

        if (cfg.subsong_index > 0 && cfg.subsong_end != 0) {
            res = convert_subsongs(&cfg);
            //if (!res) goto fail;
            if (res) ok = 1;
        }
        else {
            res = convert_file(&cfg);
            //if (!res) goto fail;
            if (res) ok = 1;
        }
    }

    /* ok if at least one succeeds, for programs that check result code */
    if (!ok)
        goto fail;

    return EXIT_SUCCESS;
fail:
    return EXIT_FAILURE;
}

static int convert_subsongs(cli_config* cfg) {
    int res, kos;
    int subsong;
    /* restore original values in case of multiple parsed files */
    int start_temp = cfg->subsong_index;
    int end_temp = cfg->subsong_end;

    /* first call should force load max subsongs */
    if (cfg->subsong_end == -1) {
        res = convert_file(cfg);
        if (!res) goto fail;
    }


    //;VGM_LOG("CLI: subsongs %i to %i\n", cfg->subsong_index, cfg->subsong_end + 1);

    /* convert subsong range */
    kos = 0 ;
    for (subsong = cfg->subsong_index; subsong < cfg->subsong_end + 1; subsong++) {
        cfg->subsong_index = subsong; 

        res = convert_file(cfg);
        if (!res) kos++;
    }

    if (kos) {
        fprintf(stderr, "failed %i subsongs\n", kos);
    }

    cfg->subsong_index = start_temp;
    cfg->subsong_end = end_temp;
    return 1;
fail:
    cfg->subsong_index = start_temp;
    cfg->subsong_end = end_temp;
    return 0;
}


static int convert_file(cli_config* cfg) {
    VGMSTREAM* vgmstream = NULL;
    char outfilename_temp[PATH_LIMIT];
    int32_t len_samples;


    vgmstream_set_log_stdout(VGM_LOG_LEVEL_ALL);

    /* for plugin testing */
    if (cfg->validate_extensions)  {
        int valid;
        vgmstream_ctx_valid_cfg vcfg = {0};

        vcfg.skip_standard = 0;
        vcfg.reject_extensionless = 0;
        vcfg.accept_unknown = 0;
        vcfg.accept_common = 0;

        valid = vgmstream_ctx_is_valid(cfg->infilename, &vcfg);
        if (!valid) goto fail;
    }

    /* open streamfile and pass subsong */
    {
        STREAMFILE* sf = open_stdio_streamfile(cfg->infilename);
        if (!sf) {
            fprintf(stderr, "file %s not found\n", cfg->infilename);
            goto fail;
        }

        sf->stream_index = cfg->subsong_index;
        vgmstream = init_vgmstream_from_STREAMFILE(sf);
        close_streamfile(sf);

        if (!vgmstream) {
            fprintf(stderr, "failed opening %s\n", cfg->infilename);
            goto fail;
        }

        /* force load total subsongs if signalled */
        if (cfg->subsong_end == -1) {
            cfg->subsong_end = vgmstream->num_streams;
            close_vgmstream(vgmstream);
            return 1;
        }
    }


    /* modify the VGMSTREAM if needed (before printing file info) */
    apply_config(vgmstream, cfg);

    /* enable after config but before outbuf */
    if (cfg->downmix_channels) {
        vgmstream_mixing_autodownmix(vgmstream, cfg->downmix_channels);
    }
    else if (cfg->only_stereo >= 0) {
        vgmstream_mixing_stereo_only(vgmstream, cfg->only_stereo);
    }
    vgmstream_mixing_enable(vgmstream, SAMPLE_BUFFER_SIZE, NULL, NULL);

    /* get final play config */
    len_samples = vgmstream_get_samples(vgmstream);
    if (len_samples <= 0)
        goto fail;

    if (cfg->seek_samples1 < -1) /* ex value for loop testing */
        cfg->seek_samples1 = vgmstream->loop_start_sample;
    if (cfg->seek_samples1 >= len_samples)
        cfg->seek_samples1 = -1;
    if (cfg->seek_samples2 >= len_samples)
        cfg->seek_samples2 = -1;

    if (cfg->play_forever && !vgmstream_get_play_forever(vgmstream)) {
        fprintf(stderr, "file can't be played forever");
        goto fail;
    }


    /* prepare output */
    {
        /* note that outfilename_temp must persist outside this block, hence the external array */

        if (!cfg->outfilename_config && !cfg->outfilename) {
            /* defaults */
            int has_subsongs = (cfg->subsong_index >= 1 && vgmstream->num_streams >= 1);

            cfg->outfilename_config = has_subsongs ? 
                "?f#?s.wav" :
                "?f.wav";
            /* maybe should avoid overwriting with this auto-name, for the unlikely
             * case of file header-body pairs (file.ext+file.ext.wav) */
        }

        if (cfg->outfilename_config) {
            /* special substitution */
            replace_filename(outfilename_temp, sizeof(outfilename_temp), cfg, vgmstream);
            cfg->outfilename = outfilename_temp;
        }

        if (!cfg->outfilename) 
            goto fail;

        /* don't overwrite itself! */
        if (strcmp(cfg->outfilename, cfg->infilename) == 0) {
            fprintf(stderr, "same infile and outfile name: %s\n", cfg->outfilename);
            goto fail;
        }
    }


    /* prints */
#ifdef HAVE_JSON
    if (!cfg->print_metajson) {
#endif
        print_info(vgmstream, cfg);
        print_tags(cfg);
        print_title(vgmstream, cfg);
#ifdef HAVE_JSON
    }
    else {
        print_json_info(vgmstream, cfg);
        printf("\n");
    }
#endif

    /* prints done */
    if (cfg->print_metaonly) {
        close_vgmstream(vgmstream);
        return 1;
    }


    /* main decode */
    write_file(vgmstream, cfg);

    /* try again with (for testing reset_vgmstream, simulates a seek to 0 after changing internal state)
     * (could simulate by seeking to last sample then to 0, too */
    if (cfg->test_reset) {
        char outfilename_reset[PATH_LIMIT];
        strcpy(outfilename_reset, cfg->outfilename);
        strcat(outfilename_reset, ".reset.wav");

        cfg->outfilename = outfilename_reset;

        reset_vgmstream(vgmstream);

        write_file(vgmstream, cfg);
    }

    close_vgmstream(vgmstream);
    return 1;

fail:
    close_vgmstream(vgmstream);
    return 0;
}


static int write_file(VGMSTREAM* vgmstream, cli_config* cfg) {
    FILE* outfile = NULL;
    int32_t len_samples;
    sample_t* buf = NULL;
    int i;
    int channels, input_channels;


    channels = vgmstream->channels;
    input_channels = vgmstream->channels;

    vgmstream_mixing_enable(vgmstream, 0, &input_channels, &channels);

    /* last init */
    buf = malloc(SAMPLE_BUFFER_SIZE * sizeof(sample_t) * input_channels);
    if (!buf) {
        fprintf(stderr, "failed allocating output buffer\n");
        goto fail;
    }

    /* simulate seek */
    len_samples = vgmstream_get_samples(vgmstream);
    if (cfg->seek_samples2 >= 0)
        len_samples -= cfg->seek_samples2;
    else if (cfg->seek_samples1 >= 0)
        len_samples -= cfg->seek_samples1;

    if (cfg->seek_samples1 >= 0)
        seek_vgmstream(vgmstream, cfg->seek_samples1);
    if (cfg->seek_samples2 >= 0)
        seek_vgmstream(vgmstream, cfg->seek_samples2);


    /* output file */
    if (cfg->play_sdtout) {
        outfile = stdout;
    }
    else if (!cfg->decode_only) {
        outfile = fopen(cfg->outfilename, "wb");
        if (!outfile) {
            fprintf(stderr, "failed to open %s for output\n", cfg->outfilename);
            goto fail;
        }

        /* no improvement */
        //setvbuf(outfile, NULL, _IOFBF, SAMPLE_BUFFER_SIZE * sizeof(sample_t) * input_channels);
        //setvbuf(outfile, NULL, _IONBF, 0);
    }


    /* decode forever */
    while (cfg->play_forever) {
        int to_get = SAMPLE_BUFFER_SIZE;

        render_vgmstream(buf, to_get, vgmstream);

        swap_samples_le(buf, channels * to_get); /* write PC endian */
        fwrite(buf, sizeof(sample_t) * channels, to_get, outfile);
        /* should write infinitely until program kill */
    }


    /* slap on a .wav header */
    if (!cfg->decode_only) {
        uint8_t wav_buf[0x100];
        size_t bytes_done;

        bytes_done = make_wav_header(wav_buf,0x100,
                len_samples, vgmstream->sample_rate, channels,
                cfg->write_lwav, cfg->lwav_loop_start, cfg->lwav_loop_end);

        fwrite(wav_buf, sizeof(uint8_t), bytes_done, outfile);
    }


    /* decode */
    for (i = 0; i < len_samples; i += SAMPLE_BUFFER_SIZE) {
        int to_get = SAMPLE_BUFFER_SIZE;
        if (i + SAMPLE_BUFFER_SIZE > len_samples)
            to_get = len_samples - i;

        render_vgmstream(buf, to_get, vgmstream);

        if (!cfg->decode_only) {
            swap_samples_le(buf, channels * to_get); /* write PC endian */
            fwrite(buf, sizeof(sample_t) * channels, to_get, outfile);
        }
    }

    if (outfile && !cfg->play_sdtout)
        fclose(outfile);
    free(buf);
    return 1;
fail:
    if (outfile && !cfg->play_sdtout)
        fclose(outfile);
    free(buf);
    return 0;
}


#ifdef HAVE_JSON
static void print_json_info(VGMSTREAM* vgm, cli_config* cfg) {
    json_t* version_string = json_string(VGMSTREAM_VERSION);
    vgmstream_info info;
    describe_vgmstream_info(vgm, &info);

    json_t* mixing_info = NULL;

    // The JSON pack format string is defined here: https://jansson.readthedocs.io/en/latest/apiref.html#building-values

    if (info.mixing_info.input_channels > 0) {
        mixing_info = json_pack("{sisi}",
            "inputChannels", info.mixing_info.input_channels,
            "outputChannels", info.mixing_info.output_channels);
    }

    json_t* loop_info = NULL;

    if (info.loop_info.end > info.loop_info.start) {
        loop_info = json_pack("{sisi}",
            "start", info.loop_info.start,
            "end", info.loop_info.end);
    }

    json_t* interleave_info = NULL;

    if (info.interleave_info.last_block > info.interleave_info.first_block) {
        interleave_info = json_pack("{sisi}",
            "firstBlock", info.interleave_info.first_block,
            "lastBlock", info.interleave_info.last_block
        );
    }

    json_t* stream_info = json_pack("{sisssi}",
        "index", info.stream_info.current,
        "name", info.stream_info.name,
        "total", info.stream_info.total
    );

    if (info.stream_info.name[0] == '\0') {
        json_object_set(stream_info, "name", json_null());
    }

    json_t* final_object = json_pack(
        "{sssisiso?siso?so?sisssssisssiso?}",
        "version", version_string,
        "sampleRate", info.sample_rate,
        "channels", info.channels,
        "mixingInfo", mixing_info,
        "channelLayout", info.channel_layout,
        "loopingInfo", loop_info,
        "interleaveInfo", interleave_info,
        "numberOfSamples", info.num_samples,
        "encoding", info.encoding,
        "layout", info.layout,
        "frameSize", info.frame_size,
        "metadataSource", info.metadata,
        "bitrate", info.bitrate,
        "streamInfo", stream_info
    );

    if (info.frame_size == 0) {
        json_object_set(final_object, "frameSize", json_null());
    }

    if (info.channel_layout == 0) {
        json_object_set(final_object, "channelLayout", json_null());
    }

    json_dumpf(final_object, stdout, JSON_COMPACT);

    json_decref(final_object);
}
#endif

static void make_smpl_chunk(uint8_t* buf, int32_t loop_start, int32_t loop_end) {
    int i;

    memcpy(buf+0, "smpl", 0x04); /* header */
    put_s32le(buf+0x04, 0x3c); /* size */

    for (i = 0; i < 7; i++)
        put_s32le(buf+0x08 + i * 0x04, 0);

    put_s32le(buf+0x24, 1);

    for (i = 0; i < 3; i++)
        put_s32le(buf+0x28 + i * 0x04, 0);

    put_s32le(buf+0x34, loop_start);
    put_s32le(buf+0x38, loop_end);
    put_s32le(buf+0x3C, 0);
    put_s32le(buf+0x40, 0);
}

/* make a RIFF header for .wav */
static size_t make_wav_header(uint8_t* buf, size_t buf_size, int32_t sample_count, int32_t sample_rate, int channels, int smpl_chunk, int32_t loop_start, int32_t loop_end) {
    size_t data_size, header_size;

    data_size = sample_count * channels * sizeof(sample_t);
    header_size = 0x2c;
    if (smpl_chunk && loop_end)
        header_size += 0x3c+ 0x08;

    if (header_size > buf_size)
        goto fail;

    memcpy(buf+0x00, "RIFF", 0x04); /* RIFF header */
    put_32bitLE(buf+0x04, (int32_t)(header_size - 0x08 + data_size)); /* size of RIFF */

    memcpy(buf+0x08, "WAVE", 4); /* WAVE header */

    memcpy(buf+0x0c, "fmt ", 0x04); /* WAVE fmt chunk */
    put_s32le(buf+0x10, 0x10); /* size of WAVE fmt chunk */
    put_s16le(buf+0x14, 0x0001); /* codec PCM */
    put_s16le(buf+0x16, channels); /* channel count */
    put_s32le(buf+0x18, sample_rate); /* sample rate */
    put_s32le(buf+0x1c, sample_rate * channels * sizeof(sample_t)); /* bytes per second */
    put_s16le(buf+0x20, (int16_t)(channels * sizeof(sample_t))); /* block align */
    put_s16le(buf+0x22, sizeof(sample_t) * 8); /* significant bits per sample */

    if (smpl_chunk && loop_end) {
        make_smpl_chunk(buf+0x24, loop_start, loop_end);
        memcpy(buf+0x24+0x3c+0x08, "data", 0x04); /* WAVE data chunk */
        put_u32le(buf+0x28+0x3c+0x08, (int32_t)data_size); /* size of WAVE data chunk */
    }
    else {
        memcpy(buf+0x24, "data", 0x04); /* WAVE data chunk */
        put_s32le(buf+0x28, (int32_t)data_size); /* size of WAVE data chunk */
    }

    /* could try to add channel_layout, but would need to write WAVEFORMATEXTENSIBLE (maybe only if arg flag?) */

    return header_size;
fail:
    return 0;
}
