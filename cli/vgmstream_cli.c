/**
 * vgmstream CLI decoder
 */
#define POSIXLY_CORRECT

#include <string.h>
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

#if defined(VGM_STDIO_UNICODE) && defined(WIN32)
    #include "windows_utils.h"
    #define fopen_v fopen_win
#else
    #define fopen_v fopen
#endif

#include "../version.h"
#ifndef VGMSTREAM_VERSION
#define VGMSTREAM_VERSION "unknown version " __DATE__
#endif
#define APP_NAME  "vgmstream CLI decoder " VGMSTREAM_VERSION
#define APP_INFO  APP_NAME " (" __DATE__ ")"


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
            "    -w: allow .wav in original sample format rather than mixing to PCM16\n"
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
            "    -W <type>: force .wav output format (1=PCM16, 2=PCM24, 3=PCM32, 4=float)\n"
            "    -O: decode but don't write to file (for performance testing)\n"
    );

}

static bool parse_config(cli_config_t* cfg, int argc, char** argv) {
    /* non-zero defaults */
    cfg->loop_count = 2.0;
    cfg->fade_time = 10.0;
    cfg->seek_samples1 = -1;
    cfg->seek_samples2 = -1;

    opterr = 0; // don't let getopt print errors to stdout automatically
    optind = 1; // reset getopt's ugly globals (needed in wasm that may call same main() multiple times)
    optopt = 0; // just in case
    optarg = NULL;

    int filenames_count = 0;

    int opt;
getopt_start:
    // POSIX's getopt reorders (PERMUTE) argv directly by default (with non-linear optind?).
    // If POSIXLY_CORRECT is set it should leave them as-is (REQUIRE_ORDER), but in practice seems
    // erratic between systems, so force REQUIRE_ORDER using first char '+' (returns -1 when filename
    // is found). BSD's getopt seem to behave like REQUIRE_ORDER and ignores '+'.

    // read config
    while ((opt = getopt(argc, argv, "+o:l:f:d:ipPcmxeLEFrgb2:s:tTk:K:hOvD:S:B:VIwW:")) != -1) {
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
                if (cfg->subsong_end == 0)
                    cfg->subsong_end = -1; /* signal up to end (otherwise 0 = not set) */
                if (cfg->subsong_index == 0)
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
                // this forces a buffer as a test, but vgmstream provides its own buffers
                // may improve IO/memory in some systems (wasm without -s ALLOW_MEMORY_GROWTH?)
                cfg->sample_buffer_size = atoi(optarg);
                break;
            case 'W':
                cfg->wav_force_output = atoi(optarg);
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

            case '?': // unknown -* flag
            case ':': // bad argument on BSD?
                fprintf(stderr, "missing argument or unknown option -%c\n", optopt);
                print_usage(argv[0], false);
                goto fail;

            //case 1: // POSIX: 'filename' when setting getopt to RETURN_IN_ORDER (options string starts with "-")
            //    filenames_count++;
            //    break;

            default: // shouldn't happen
                print_usage(argv[0], false);
                goto fail;
        }

        // CLI accepts N filenames and flags in any position. Since filename list can be huge (ex. drag-and-drop) it's read from argv as-is.
        // Instead, mark flags+parameters at their index (potentially a lot less flags), so they can be skipped later without re-parsing.
        if (optind < CLI_MAX_FLAGS) {
            bool is_file = false; // varies with getopts but files shouldn't reach this with current config
            int argv_index = optind - (!is_file && optarg ? 2 : 1);
            cfg->flag_index[argv_index] = !is_file;
            if (!is_file && optarg > 0) { // has parameter
                cfg->flag_index[argv_index + 1] = true;
            }
        }
    }

    // Handle POSIX<>BSD's getopt inconsistencies. BSD can't use extra flags, so both should stop (return -1)
    // as soon as a non '-' character is found (like a filename) while optind still points to it.
    // Count it as a file and keep iterating next args
    if (optind < argc) {
        filenames_count++;
        optind++;
        if (optind < argc)
            goto getopt_start; // TO-DO call a parse function
    }

    if (filenames_count <= 0) {
        fprintf(stderr, "missing input file(s)\n");
        print_usage(argv[0], false);
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

static void load_vconfig(libvgmstream_config_t* vcfg, cli_config_t* cfg) {

    /* write loops in the wav, but don't actually loop it */
    if (cfg->write_lwav) {
        vcfg->disable_config_override = true;
        cfg->ignore_loop = true;
    }
    
    /* only allowed if manually active */
    if (cfg->play_forever) {
        vcfg->allow_play_forever = true;
    }

    vcfg->play_forever = cfg->play_forever;
    vcfg->fade_time = cfg->fade_time;
    vcfg->loop_count = cfg->loop_count;
    vcfg->fade_delay = cfg->fade_delay;

    vcfg->ignore_loop  = cfg->ignore_loop;
    vcfg->force_loop = cfg->force_loop;
    vcfg->really_force_loop = cfg->really_force_loop;
    vcfg->ignore_fade = cfg->ignore_fade;

    vcfg->auto_downmix_channels = cfg->downmix_channels;
    if (cfg->wav_force_output) {
        vcfg->force_sfmt = cfg->wav_force_output;
    }
    else if (!cfg->write_original_wav) {
        // bloated wav for most uses so disabled by default
        vcfg->force_sfmt = LIBVGMSTREAM_SFMT_PCM16;
    }

    vcfg->stereo_track = cfg->stereo_track;
}

static bool write_file(libvgmstream_t* vgmstream, cli_config_t* cfg) {
    FILE* outfile = NULL;
    void* buf = NULL;

    /* simulate seek */
    int64_t play_samples = vgmstream->format->play_samples;
    if (cfg->seek_samples2 >= 0)
        play_samples -= cfg->seek_samples2;
    else if (cfg->seek_samples1 >= 0)
        play_samples -= cfg->seek_samples1;

    if (cfg->seek_samples1 >= 0)
        libvgmstream_seek(vgmstream, cfg->seek_samples1);
    if (cfg->seek_samples2 >= 0)
        libvgmstream_seek(vgmstream, cfg->seek_samples2);

    if (cfg->sample_buffer_size > 0) {
        buf = malloc(cfg->sample_buffer_size * vgmstream->format->sample_size * vgmstream->format->channels);
        if (!buf) {
            fprintf(stderr, "failed allocating buffer\n");
            goto fail;
        }
    }


    /* output file */
    if (cfg->play_sdtout) {
        outfile = stdout;
    }
    else if (!cfg->decode_only) {
        outfile = fopen_v(cfg->outfilename, "wb");
        if (!outfile) {
            fprintf(stderr, "failed to open %s for output\n", cfg->outfilename);
            goto fail;
        }
    }
    else {
        // decode only: outfile is NULL (won't write anything)
    }

    /* slap on a .wav header (note that this goes before decodes in case of printing to stdout) */
    if (!cfg->decode_only) {
        uint8_t wav_buf[0x100];
        size_t bytes_done;

        wav_header_t wav = {
            .sample_count = play_samples,
            .sample_rate = vgmstream->format->sample_rate,
            .channels = vgmstream->format->channels,
            .write_smpl_chunk = cfg->write_lwav,
            .sample_size = vgmstream->format->sample_size,
            .is_float = vgmstream->format->sample_format == LIBVGMSTREAM_SFMT_FLOAT
        };

        if (cfg->write_lwav && vgmstream->format->loop_start < vgmstream->format->loop_end) {
            wav.loop_start = vgmstream->format->loop_start;
            wav.loop_end = vgmstream->format->loop_end;
            wav.loop_end--; /* from spec, +1 is added when reading "smpl" */
        }

        bytes_done = wav_make_header(wav_buf, 0x100, &wav);
        if (bytes_done == 0) goto fail;
        fwrite(wav_buf, sizeof(uint8_t), bytes_done, outfile);
    }

    /* decode (normally or forever until program kill) */
    while (!vgmstream->decoder->done) {
        if (buf) {
            int err = libvgmstream_fill(vgmstream, buf, cfg->sample_buffer_size);
            if (err < 0) break;
        }
        else {
            int err = libvgmstream_render(vgmstream);
            if (err < 0) break;
        }

        void* buf = vgmstream->decoder->buf;
        int buf_bytes = vgmstream->decoder->buf_bytes;
        int buf_samples = vgmstream->decoder->buf_samples;
        int sample_size = vgmstream->format->sample_size;

        if (!cfg->decode_only) {
            wav_swap_samples_le(buf, vgmstream->format->channels * buf_samples, sample_size);
            fwrite(buf, sizeof(uint8_t), buf_bytes, outfile);
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
    
    libvgmstream_valid_t vcfg = {0};

    vcfg.skip_standard = 0;
    vcfg.reject_extensionless = 0;
    vcfg.accept_unknown = 0;
    vcfg.accept_common = 0;

    return libvgmstream_is_valid(cfg->infilename, &vcfg);
}

static libvgmstream_t* open_vgmstream(cli_config_t* cfg) {

    libstreamfile_t* sf = libstreamfile_open_from_stdio(cfg->infilename);
    if (!sf) {
        fprintf(stderr, "file %s not found\n", cfg->infilename);
        return NULL;
    }

    libvgmstream_config_t vcfg = {0};
    load_vconfig(&vcfg, cfg);

    libvgmstream_t* vgmstream = libvgmstream_create(sf, cfg->subsong_current_index, &vcfg);
    if (!vgmstream) {
        fprintf(stderr, "failed opening %s\n", cfg->infilename);
        goto fail;
    }

    libstreamfile_close(sf);
    return vgmstream;
fail:
    libstreamfile_close(sf);
    libvgmstream_free(vgmstream);
    return NULL;
}


static bool convert_file(cli_config_t* cfg) {
    libvgmstream_t* vgmstream = NULL;
    char outfilename_temp[CLI_PATH_LIMIT];
    int64_t play_samples;


    /* for plugin testing */
    if (!is_valid_extension(cfg))
        return false;

    /* open streamfile and pass subsong */
    vgmstream = open_vgmstream(cfg);
    if (!vgmstream) goto fail;

    /* force load total subsongs if signalled */
    if (cfg->subsong_current_end == -1) {
        cfg->subsong_current_end = vgmstream->format->subsong_count;
        libvgmstream_free(vgmstream);
        return true;
    }


    /* get final play config */
    play_samples = vgmstream->format->play_samples;
    if (play_samples <= 0) {
        fprintf(stderr, "wrong time config\n");
        goto fail;
    }

    /* special values for loop testing */
    if (cfg->seek_samples1 == -2) { /* loop start...end */
        cfg->seek_samples1 = vgmstream->format->loop_start;
    }
    if (cfg->seek_samples1 == -3) { /* loop end..end */
        cfg->seek_samples1 = vgmstream->format->loop_end;
    }

    /* would be ignored by seek code though (allowed for seek_samples2 to test this) */
    if (cfg->seek_samples1 < -1 || cfg->seek_samples1 >= play_samples) {
        fprintf(stderr, "wrong seek config\n");
        goto fail;
    }

    if (cfg->play_forever && !vgmstream->format->play_forever) {
        fprintf(stderr, "file can't be played forever");
        goto fail;
    }


    /* prepare output */
    {
        /* note that outfilename_temp must persist outside this block, hence the external array */

        if (!cfg->outfilename_config && !cfg->outfilename) {
            /* defaults */
            bool has_subsongs = (cfg->subsong_current_index >= 1 && vgmstream->format->subsong_count >= 1);

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
    if (cfg->print_metajson) {
        print_json_info(vgmstream, cfg, VGMSTREAM_VERSION);
    }
    else {
        print_info(vgmstream, cfg);
        print_tags(cfg);
        print_title(vgmstream, cfg);
    }

    /* prints done */
    if (cfg->print_metaonly) {
        libvgmstream_free(vgmstream);
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

        libvgmstream_reset(vgmstream);

        write_file(vgmstream, cfg);
    }

    libvgmstream_free(vgmstream);
    return true;

fail:
    libvgmstream_free(vgmstream);
    return false;
}

static bool convert_subsongs(cli_config_t* cfg) {
    // set base value for current file (passed files may have different number of subsongs)
    cfg->subsong_current_index = cfg->subsong_index;
    cfg->subsong_current_end = cfg->subsong_end;

    // first call should force load max subsongs (if file has no subsongs this will be set to 1)
    if (cfg->subsong_current_end == -1) {
        bool res = convert_file(cfg);
        if (!res) return false;
    }


    //printf("CLI: subsongs %i to %i\n", cfg->subsong_current_index, cfg->subsong_current_end + 1);

    // convert subsong range
    int ko_count = 0 ;
    while (cfg->subsong_current_index < cfg->subsong_current_end + 1) {
        bool res = convert_file(cfg);
        if (!res) ko_count++;

        cfg->subsong_current_index++;
    }

    if (ko_count) {
        fprintf(stderr, "failed %i subsongs\n", ko_count);
    }

    return true;
}

int main(int argc, char** argv) {
    cli_config_t cfg = {0};
    bool res, ok;

    libvgmstream_set_log(0, NULL);

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

    // don't mix logs with JSON
    if (cfg.print_metajson) {
        libvgmstream_set_log(LIBVGMSTREAM_LOG_LEVEL_NONE, NULL);
    }

    ok = false;
    for (int i = 1; i < argc; i++) {
        // ignore flags
        if (i < CLI_MAX_FLAGS && cfg.flag_index[i]) {
            continue;
        }

        // current name, to avoid passing params all the time
        cfg.infilename = argv[i];
        if (cfg.outfilename_config)
            cfg.outfilename = NULL;

        if (cfg.subsong_index > 0 && cfg.subsong_end != 0) {
            res = convert_subsongs(&cfg);
            //if (!res) goto fail;
            if (res) ok = true;
        }
        else {
            cfg.subsong_current_index = cfg.subsong_index;

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

#if defined(VGM_STDIO_UNICODE) && defined(WIN32)

#define WMAIN_STACK_ARGC 16
#define WMAIN_STACK_SIZE 0x800

// TO-DO: alternatives for some Windows versions:
// - Windows 10 1803 (April 2018): setlocale(LC_ALL, ".utf8")
// - Windows 10 1903 (May 2019): activeCodePage=UTF8 in appxmanifest

// Handle Windows' uglycode stuff for non-ascii extended filenames, which needs 3 separate steps:
// - Make a 'wmain' for "unicode" (actually UCS-2) args. There are alt entry points but was the cleanest.
// - Modify stdio, using _wfopen instead of fopen if needed
// - allow console/redirected output (printf), via codepages (or modified fprintf). Terminal needs a utf font too.
// Converts back and forth to UTF-8 for portability, since vgmstream code expects that.
int wmain(int argc, wchar_t** argv_w) {
    uint32_t codepage = windows_setup_codepage_utf8();

    int main_result = -1; // only 0/1 are returned

    // try to use small stack first as it's the most common case (single file with few params)
    {
        char* argv_stack[WMAIN_STACK_ARGC];
        char block_stack[WMAIN_STACK_SIZE];

        bool ok = windows_wargs_to_args_stack(argc, argv_w, argv_stack, WMAIN_STACK_ARGC, block_stack, WMAIN_STACK_SIZE);
        if (ok) {
            main_result = main(argc, argv_stack);
        }
    }

    // if not stack wasn't big enough call again alloc'ing as needed
    if (main_result < 0) {
        char** argv_alloc = NULL;
        char* block_alloc = NULL;

        bool ok = windows_wargs_to_args_alloc(argc, argv_w, &argv_alloc, &block_alloc);
        if (ok) {
            main_result = main(argc, argv_alloc);
        }

        if (argv_alloc || block_alloc) {
            free(argv_alloc);
            free(block_alloc);
        }
    }

    if (main_result < 0) {
        fprintf(stderr, "failed to convert args\n");
        return EXIT_FAILURE;
    }

    windows_restore_codepage(codepage);
    return main_result;
}

#endif
