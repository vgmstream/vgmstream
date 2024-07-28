/**
 * vgmstream CLI decoder
 */
#define POSIXLY_CORRECT

#include <stdio.h>
#include <getopt.h>
 
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif


#include "vgmstream_cli.h"
#include "wav_utils.h"

#include "../version.h"
#ifndef VGMSTREAM_VERSION
#define VGMSTREAM_VERSION "unknown version " __DATE__
#endif
#define APP_NAME  "vgmstream CLI decoder " VGMSTREAM_VERSION
#define APP_INFO  APP_NAME " (" __DATE__ ")"


/* Low values are ok as there is very little performance difference, but higher may improve write I/O in some systems.
 * For systems with less memory (like wasm without -s ALLOW_MEMORY_GROWTH) lower helps a bit. */
#ifdef __EMSCRIPTEN__
    #define SAMPLE_BUFFER_SIZE  1024
#else
    #define SAMPLE_BUFFER_SIZE  (1024*8)
#endif
#define MIN_SAMPLE_BUFFER_SIZE  128
#define MAX_SAMPLE_BUFFER_SIZE  (1024*128)

/* getopt globals from .h, for reference (the horror...) */
//extern char* optarg;
//extern int optind, opterr, optopt;


static void print_usage(const char* progname, bool is_help) {

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
            "    -p: output to stdout (for piping into another program)\n"
            "    -P: output to stdout even if stdout is a terminal\n"
            "    -c: loop forever (continuously) to stdout\n"
            "    -L: append a smpl chunk and create a looping wav\n"
          //"    -w: allow .wav in original sample format rather than downmixing to PCM16\n"
            "    -V: print version info and supported extensions as JSON\n"
            "    -I: print requested file info as JSON\n"
            "    -h: print all commands\n"
            , progname);

    if (!is_help)
        return;

    fprintf(is_help ? stdout : stderr,
            "Extra options:\n"
            "    -2 N: only output the Nth (first is 0) set of stereo channels\n"
            "    -x: decode and print adxencd command line to encode as ADX\n"
            "    -g: decode and print oggenc command line to encode as OGG\n"
            "    -b: decode and print batch variable commands\n"
            "    -v: validate extensions (for extension testing)\n"
            "    -r: reset and output a second file (for reset testing)\n"
            "    -k N: kills (seeks) N samples before decoding (for seek testing)\n"
            "       -2 seeks to loop start, -3 seeks to loop end\n"
            "    -K N: kills (seeks) again to N samples before decoding (for seek testing)\n"
            "    -t: print !tags found in !tags.m3u (for tag testing)\n"
            "    -T: print title (for title testing)\n"
            "    -D <max channels>: downmix to <max channels> (for plugin downmix testing)\n"
            "    -B <samples> force a sample buffer size (for api testing)\n"
            "    -O: decode but don't write to file (for performance testing)\n"
    );

}

static bool parse_config(cli_config_t* cfg, int argc, char** argv) {
    int opt;

    /* non-zero defaults */
    cfg->sample_buffer_size = SAMPLE_BUFFER_SIZE;
    cfg->loop_count = 2.0;
    cfg->fade_time = 10.0;
    cfg->seek_samples1 = -1;
    cfg->seek_samples2 = -1;

    opterr = 0; /* don't let getopt print errors to stdout automatically */
    optind = 1; /* reset getopt's ugly globals (needed in wasm that may call same main() multiple times) */

    /* read config */
    while ((opt = getopt(argc, argv, "o:l:f:d:ipPcmxeLEFrgb2:s:tTk:K:hOvD:S:B:VI")) != -1) {
        switch (opt) {
            case 'o':
                cfg->outfilename = optarg;
                break;

            // playback config
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
            case 'F':
                cfg->ignore_fade = true;
                break;
            case 'i':
                cfg->ignore_loop = true;
                break;
            case 'e':
                cfg->force_loop = true;
                break;
            case 'E':
                cfg->really_force_loop = true;
                break;

            case 'p':
                cfg->play_sdtout = true;
                break;
            case 'P':
                cfg->play_wreckless = true;
                cfg->play_sdtout = true;
                break;
            case 'c':
                cfg->play_forever = true;
                break;

            // subsongs
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

            // wav config
            case 'L':
                cfg->write_lwav = true;
                break;
            case 'w':
                cfg->write_original_wav = true;
                break;

            // print flags
            case 'm':
                cfg->print_metaonly = true;
                break;
            case 'x':
                cfg->print_adxencd = true;
                break;
            case 'g':
                cfg->print_oggenc = true;
                break;
            case 'b':
                cfg->print_batchvar = true;
                break;
            case 'T':
                cfg->print_title = true;
                break;
            case 't':
                cfg->tag_filename = "!tags.m3u";
                break;

            // debug stuff
            case 'O':
                cfg->decode_only = true;
                break;
            case 'r':
                cfg->test_reset = true;
                break;
            case 'v':
                cfg->validate_extensions = true;
                break;
            case 'k':
                cfg->seek_samples1 = atoi(optarg);
                break;
            case 'K':
                cfg->seek_samples2 = atoi(optarg);
                break;
            case 'D':
                cfg->downmix_channels = atoi(optarg);
                break;
            case 'B':
                cfg->sample_buffer_size = atoi(optarg);
                if (cfg->sample_buffer_size < MIN_SAMPLE_BUFFER_SIZE || cfg->sample_buffer_size > MAX_SAMPLE_BUFFER_SIZE) {
                    fprintf(stderr, "incorrect sample buffer value\n");
                    goto fail;
                }
                break;
            case '2':
                cfg->stereo_track = atoi(optarg) + 1;
                break;

            case 'h':
                print_usage(argv[0], true);
                goto fail;
            case 'V':
                print_json_version(VGMSTREAM_VERSION);
                goto fail;
            case 'I':
                cfg->print_metajson = true;
                break;
            case '?':
                fprintf(stderr, "missing argument or unknown option -%c\n", optopt);
                goto fail;
            default:
                print_usage(argv[0], false);
                goto fail;
        }
    }

    /* filenames go last in POSIX getopt, not so in glibc getopt */ //TODO unify
    if (optind != argc - 1) {

        /* check there aren't commands after filename */
        for (int i = optind; i < argc; i++) {
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
        print_usage(argv[0], 0);
        goto fail;
    }

    if (cfg->outfilename && strchr(cfg->outfilename, '?') != NULL) {
        cfg->outfilename_config = cfg->outfilename;
        cfg->outfilename = NULL;
    }

    return true;
fail:
    return false;
}

static bool validate_config(cli_config_t* cfg) {
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

    return true;
fail:
    return false;
}

/* ************************************************************ */

static void apply_config(VGMSTREAM* vgmstream, cli_config_t* cfg) {
    vgmstream_cfg_t vcfg = {0};

    /* write loops in the wav, but don't actually loop it */
    if (cfg->write_lwav) {
        vcfg.disable_config_override = true;
        cfg->ignore_loop = true;

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
        vcfg.allow_play_forever = true;
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

static bool write_file(VGMSTREAM* vgmstream, cli_config_t* cfg) {
    FILE* outfile = NULL;
    int32_t len_samples;
    sample_t* buf = NULL;
    int channels, input_channels;


    channels = vgmstream->channels;
    input_channels = vgmstream->channels;

    vgmstream_mixing_enable(vgmstream, 0, &input_channels, &channels);

    /* last init */
    buf = malloc(cfg->sample_buffer_size * sizeof(sample_t) * input_channels);
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
        //setvbuf(outfile, NULL, _IOFBF, cfg->sample_buffer_size * sizeof(sample_t) * input_channels);
        //setvbuf(outfile, NULL, _IONBF, 0);
    }
    else {
        // decode only: outfile is NULL (won't write anything)
    }


    /* decode forever */
    while (cfg->play_forever && !cfg->decode_only) {
        int to_get = cfg->sample_buffer_size;

        render_vgmstream(buf, to_get, vgmstream);

        wav_swap_samples_le(buf, channels * to_get, 0);
        fwrite(buf, sizeof(sample_t), to_get * channels, outfile);
        /* should write infinitely until program kill */
    }


    /* slap on a .wav header */
    if (!cfg->decode_only) {
        uint8_t wav_buf[0x100];
        size_t bytes_done;

        wav_header_t wav = {
            .sample_count = len_samples,
            .sample_rate = vgmstream->sample_rate,
            .channels = channels,
            .write_smpl_chunk = cfg->write_lwav,
            .loop_start = cfg->lwav_loop_start,
            .loop_end = cfg->lwav_loop_end
        };

        bytes_done = wav_make_header(wav_buf, 0x100, &wav);
        fwrite(wav_buf, sizeof(uint8_t), bytes_done, outfile);
    }


    /* decode */
    for (int i = 0; i < len_samples; i += cfg->sample_buffer_size) {
        int to_get = cfg->sample_buffer_size;
        if (i + cfg->sample_buffer_size > len_samples)
            to_get = len_samples - i;

        render_vgmstream(buf, to_get, vgmstream);

        if (!cfg->decode_only) {
            wav_swap_samples_le(buf, channels * to_get, 0);
            fwrite(buf, sizeof(sample_t), to_get * channels, outfile);
        }
    }

    if (outfile && outfile != stdout)
        fclose(outfile);
    free(buf);
    return true;
fail:
    if (outfile && outfile != stdout)
        fclose(outfile);
    free(buf);
    return false;
}

static bool is_valid_extension(cli_config_t* cfg) {
    /* for plugin testing */
    if (!cfg->validate_extensions)
        return true; 
    
    vgmstream_ctx_valid_cfg vcfg = {0};

    vcfg.skip_standard = 0;
    vcfg.reject_extensionless = 0;
    vcfg.accept_unknown = 0;
    vcfg.accept_common = 0;

    return vgmstream_ctx_is_valid(cfg->infilename, &vcfg);
}

static VGMSTREAM* open_vgmstream(cli_config_t* cfg) {
    STREAMFILE* sf = NULL;
    VGMSTREAM* vgmstream = NULL;
    
    sf = open_stdio_streamfile(cfg->infilename);
    if (!sf) {
        fprintf(stderr, "file %s not found\n", cfg->infilename);
        return NULL;
    }

    sf->stream_index = cfg->subsong_index;
    vgmstream = init_vgmstream_from_STREAMFILE(sf);

    if (!vgmstream) {
        fprintf(stderr, "failed opening %s\n", cfg->infilename);
        goto fail;
    }

    /* modify the VGMSTREAM if needed (before printing file info) */
    apply_config(vgmstream, cfg);

    /* enable after config but before outbuf */
    if (cfg->downmix_channels) {
        vgmstream_mixing_autodownmix(vgmstream, cfg->downmix_channels);
    }
    else if (cfg->stereo_track > 0) {
        vgmstream_mixing_stereo_only(vgmstream, cfg->stereo_track - 1);
    }
    vgmstream_mixing_enable(vgmstream, cfg->sample_buffer_size, NULL, NULL);


    close_streamfile(sf);
    return vgmstream;
fail:
    close_streamfile(sf);
    close_vgmstream(vgmstream);
    return NULL;
}

static bool convert_file(cli_config_t* cfg) {
    VGMSTREAM* vgmstream = NULL;
    char outfilename_temp[CLI_PATH_LIMIT];
    int32_t len_samples;


    /* for plugin testing */
    if (!is_valid_extension(cfg))
        return false;

    /* open streamfile and pass subsong */
    vgmstream = open_vgmstream(cfg);
    if (!vgmstream) goto fail;

    /* force load total subsongs if signalled */
    if (cfg->subsong_end == -1) {
        cfg->subsong_end = vgmstream->num_streams;
        close_vgmstream(vgmstream);
        return true;
    }


    /* get final play config */
    len_samples = vgmstream_get_samples(vgmstream);
    if (len_samples <= 0) {
        fprintf(stderr, "wrong time config\n");
        goto fail;
    }

    /* special values for loop testing */
    if (cfg->seek_samples1 == -2) { /* loop start...end */
        cfg->seek_samples1 = vgmstream->loop_start_sample;
    }
    if (cfg->seek_samples1 == -3) { /* loop end..end */
        cfg->seek_samples1 = vgmstream->loop_end_sample;
    }

    /* would be ignored by seek code though (allowed for seek_samples2 to test this) */
    if (cfg->seek_samples1 < -1 || cfg->seek_samples1 >= len_samples) {
        fprintf(stderr, "wrong seek config\n");
        goto fail;
    }

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
    if (!cfg->print_metajson) {
        print_info(vgmstream, cfg);
        print_tags(cfg);
        print_title(vgmstream, cfg);
    }
    else {
        print_json_info(vgmstream, cfg, VGMSTREAM_VERSION);
    }

    /* prints done */
    if (cfg->print_metaonly) {
        close_vgmstream(vgmstream);
        return true;
    }


    /* main decode */
    write_file(vgmstream, cfg);

    /* try again with reset (for testing, simulates a seek to 0 after changing internal state)
     * (could simulate by seeking to last sample then to 0, too) */
    if (cfg->test_reset) {
        char outfilename_reset[CLI_PATH_LIMIT];
        snprintf(outfilename_reset, sizeof(outfilename_reset), "%s.reset.wav", cfg->outfilename);

        cfg->outfilename = outfilename_reset;

        reset_vgmstream(vgmstream);

        write_file(vgmstream, cfg);
    }

    close_vgmstream(vgmstream);
    return true;

fail:
    close_vgmstream(vgmstream);
    return false;
}

static bool convert_subsongs(cli_config_t* cfg) {
    int res, ko_count;
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
    ko_count = 0 ;
    for (int subsong = cfg->subsong_index; subsong < cfg->subsong_end + 1; subsong++) {
        cfg->subsong_index = subsong; 

        res = convert_file(cfg);
        if (!res) ko_count++;
    }

    if (ko_count) {
        fprintf(stderr, "failed %i subsongs\n", ko_count);
    }

    cfg->subsong_index = start_temp;
    cfg->subsong_end = end_temp;
    return true;
fail:
    cfg->subsong_index = start_temp;
    cfg->subsong_end = end_temp;
    return false;
}

int main(int argc, char** argv) {
    cli_config_t cfg = {0};
    bool res, ok;


    vgmstream_set_log_stdout(VGM_LOG_LEVEL_ALL);


    /* read args */
    res = parse_config(&cfg, argc, argv);
    if (!res) goto fail;

    res = validate_config(&cfg);
    if (!res) goto fail;

#ifdef WIN32
    /* make stdout output work with windows */
    if (cfg.play_sdtout) {
        _setmode(fileno(stdout),_O_BINARY);
    }
#endif

    ok = false;
    for (int i = 0; i < cfg.infilenames_count; i++) {
        /* current name, to avoid passing params all the time */
        cfg.infilename = cfg.infilenames[i];
        if (cfg.outfilename_config)
            cfg.outfilename = NULL;

        if (cfg.subsong_index > 0 && cfg.subsong_end != 0) {
            res = convert_subsongs(&cfg);
            //if (!res) goto fail;
            if (res) ok = true;
        }
        else {
            res = convert_file(&cfg);
            //if (!res) goto fail;
            if (res) ok = true;
        }
    }

    /* ok if at least one succeeds, for programs that check result code */
    if (!ok)
        goto fail;

    return EXIT_SUCCESS;
fail:
    return EXIT_FAILURE;
}
