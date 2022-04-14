/* vgmstream123.c
 *
 * Simple player frontend for vgmstream
 * Copyright (c) 2017 Daniel Richard G. <skunk@iSKUNK.ORG>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <ao/ao.h>
#include <sys/time.h>
#ifdef WIN32
# include <io.h>
# include <fcntl.h>
#else
# include <signal.h>
# include <unistd.h>
# include <sys/wait.h>
# include <termios.h>
#endif

#include "../src/vgmstream.h"
#include "../src/plugins.h"


#include "../version.h"
#ifndef VGMSTREAM_VERSION
#define VGMSTREAM_VERSION "unknown version " __DATE__
#endif
#define APP_NAME  "vgmstream123 player " VGMSTREAM_VERSION
#define APP_INFO  APP_NAME " (" __DATE__ ")"

 
//TODO: improve WIN32 builds (some features/behaviors are missing but works)
#ifdef WIN32
#define getline(line, line_mem, f)  0
#define mkdtemp(temp_dir)  0
#define signal(sig, interrupt_handler)  /*nothing*/
#define WIFSIGNALED(ret)  0
#define WTERMSIG(ret)  0
#define SIGQUIT  0
#define SIGINT  0
#define SIGHUP  0
#endif

/* If two interrupts (i.e. Ctrl-C) are received
 * within a span of this many seconds, then exit
 */
#define DOUBLE_INTERRUPT_TIME 1.0

#define LITTLE_ENDIAN_OUTPUT 1 /* untested in BE */


#define DEFAULT_PARAMS { 0, 0, -1, -1, 2.0, 10.0, 0.0,   0, 0, 0, 0 }
typedef struct {
    int subsong_index;
    int subsong_end;
    int only_stereo;

    double min_time;
    double loop_count;
    double fade_time;
    double fade_delay;

    int ignore_loop;
    int force_loop;
    int really_force_loop;
    int play_forever;
} song_settings_t;

static const char *out_filename = NULL;
static int driver_id;
static ao_device *device = NULL;
static ao_option *device_options = NULL;
static ao_sample_format current_sample_format;

static sample_t *buffer = NULL;
/* reportedly 1kb helps Raspberry Pi Zero play FFmpeg formats without stuttering
 * (presumably other low powered devices too), plus it's the default in other plugins */
static int buffer_size_kb = 1;

static int repeat = 0;
static int verbose = 0;

static volatile int interrupted = 0;
static double interrupt_time = 0.0;

static int play_file(const char *filename, song_settings_t *par);

static void interrupt_handler(int signum) {
    interrupted = 1;
}

static int record_interrupt(void) {
    int ret = 0;
    struct timeval tv = { 0, 0 };
    double t;

    if (gettimeofday(&tv, NULL))
        return -1;

    t = (double)tv.tv_sec + (double)tv.tv_usec / 1.0e6;

    if (t - interrupt_time < DOUBLE_INTERRUPT_TIME)
        ret = 1;

    interrupt_time = t;
    interrupted = 0;

    return ret;
}


/* Opens the audio device with the appropriate parameters
 */
static int set_sample_format(int channels, int sample_rate) {
    ao_sample_format format;


    memset(&format, 0, sizeof(format));
    format.bits = 8 * sizeof(sample_t);
    format.channels = channels;
    format.rate = sample_rate;
    format.byte_format =
#if LITTLE_ENDIAN_OUTPUT
        AO_FMT_LITTLE
#else
        AO_FMT_BIG
#endif
    ;

    if (memcmp(&format, &current_sample_format, sizeof(format))) {

        /* Sample format has changed, so (re-)open audio device */

        ao_info *info = ao_driver_info(driver_id);
        if (!info) return -1;

        if ((info->type == AO_TYPE_FILE) != !!out_filename) {
            if (out_filename)
                fprintf(stderr, "Live output driver \"%s\" does not take an output file\n", info->short_name);
            else
                fprintf(stderr, "File output driver \"%s\" requires an output filename\n", info->short_name);
            return -1;
        }

        if (device)
            ao_close(device);

        memcpy(&current_sample_format, &format, sizeof(format));

        if (out_filename)
            device = ao_open_file(driver_id, out_filename, 1, &format, device_options);
        else
            device = ao_open_live(driver_id, &format, device_options);

        if (!device) {
            fprintf(stderr, "Error opening \"%s\" audio device\n", info->short_name);
            return -1;
        }
    }

    return 0;
}

static void apply_config(VGMSTREAM* vgmstream, song_settings_t* cfg) {
    vgmstream_cfg_t vcfg = {0};

    vcfg.allow_play_forever = 1;

    vcfg.play_forever = cfg->play_forever;
    vcfg.fade_time = cfg->fade_time;
    vcfg.loop_count = cfg->loop_count;
    vcfg.fade_delay = cfg->fade_delay;

    vcfg.ignore_loop  = cfg->ignore_loop;
    vcfg.force_loop = cfg->force_loop;
    vcfg.really_force_loop = cfg->really_force_loop;

    vgmstream_apply_config(vgmstream, &vcfg);
}

#ifndef WIN32
static int getkey() {
    int character;
    struct termios orig_term_attr;
    struct termios new_term_attr;

    /* set the terminal to raw mode */
    tcgetattr(fileno(stdin), &orig_term_attr);
    memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
    new_term_attr.c_lflag &= ~(ECHO|ICANON);
    new_term_attr.c_cc[VTIME] = 0;
    new_term_attr.c_cc[VMIN] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

    /* read a character from the stdin stream without blocking */
    /*   returns EOF (-1) if no character is available */
    character = fgetc(stdin);

    /* restore the original terminal attributes */
    tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

    return character;
}
#endif

static int play_vgmstream(const char* filename, song_settings_t* cfg) {
    int ret = 0;
    STREAMFILE* sf;
    VGMSTREAM* vgmstream;
    FILE* save_fps[4];
    size_t buffer_size;
    int32_t max_buffer_samples;
    int i;
    int output_channels, input_channels;


    sf = open_stdio_streamfile(filename);
    if (!sf) {
        fprintf(stderr, "%s: cannot open file\n", filename);
        return -1;
    }

    vgmstream_set_log_stdout(VGM_LOG_LEVEL_ALL);

    sf->stream_index = cfg->subsong_index;
    vgmstream = init_vgmstream_from_STREAMFILE(sf);
    close_streamfile(sf);

    if (!vgmstream) {
        fprintf(stderr, "%s: error opening stream\n", filename);
        return -1;
    }

    /* force load total subsongs if signalled */
    if (cfg->subsong_end == -1) {
        cfg->subsong_end = vgmstream->num_streams;
        close_vgmstream(vgmstream);
        return 0;
    }

    /* If the audio device hasn't been opened yet, then describe it
     */
    if (!device) {
        ao_info *info = ao_driver_info(driver_id);
        if (!info) {
            printf("Cannot find audio device\n");
            goto fail;
        }
        printf("Audio device: %s\n", info->name);
        printf("Comment: %s\n", info->comment);
        putchar('\n');
    }

    if (vgmstream->num_streams > 0) {
        int subsong = vgmstream->stream_index;
        if (!subsong)
            subsong = 1;
        printf("Playing stream: %s [%i/%i]\n", filename, subsong, vgmstream->num_streams);
    }
    else {
        printf("Playing stream: %s\n", filename);

    }

    /* Print metadata in verbose mode
     */
    if (verbose) {
        char description[4096] = { '\0' };
        describe_vgmstream(vgmstream, description, sizeof(description));
        puts(description);
        putchar('\n');
    }


    /* Stupid hack to hang onto a few low-numbered file descriptors
     * so that play_compressed_file() doesn't break, due to POSIX
     * wackiness like https://bugs.debian.org/590920
     */
    for (i = 0; i < 4; i++)
        save_fps[i] = fopen("/dev/null", "r");


    /* Calculate how many loops are needed to achieve a minimum
     * playback time. Note: This calculation is derived from the
     * logic in get_vgmstream_play_samples().
     */
    if (vgmstream->loop_flag && cfg->loop_count < 0) {
        double intro = (double)vgmstream->loop_start_sample / vgmstream->sample_rate;
        double loop = (double)(vgmstream->loop_end_sample - vgmstream->loop_start_sample) / vgmstream->sample_rate;
        double end = cfg->fade_time + cfg->fade_delay;
        if (loop < 1.0) loop = 1.0;
        cfg->loop_count = ((cfg->min_time - intro - end) / loop + 0.99);
        if (cfg->loop_count < 1.0) cfg->loop_count = 1.0;
    }


    /* Config
     */
    apply_config(vgmstream, cfg);

    if (cfg->only_stereo >= 0) {
        vgmstream_mixing_stereo_only(vgmstream, cfg->only_stereo);
    }
    input_channels = vgmstream->channels;
    output_channels = vgmstream->channels;
    vgmstream_mixing_enable(vgmstream, 0, &input_channels, &output_channels); /* query */


    /* Buffer size in bytes (after getting channels)
     */
    buffer_size = 1024 * buffer_size_kb;
    if (!buffer) {
        if (buffer_size_kb < 1) {
            fprintf(stderr, "Invalid buffer size '%d'\n", buffer_size_kb);
            return -1;
        }

        buffer = malloc(buffer_size);
        if (!buffer) goto fail;
    }

    max_buffer_samples = buffer_size / (input_channels * sizeof(sample));

    vgmstream_mixing_enable(vgmstream, max_buffer_samples, NULL, NULL); /* enable */


    /* Init
     */
    ret = set_sample_format(output_channels, vgmstream->sample_rate);
    if (ret) goto fail;

    /* Decode
     */
    {
        double total;
        int time_total_min;
        double time_total_sec;
        int play_forever = vgmstream_get_play_forever(vgmstream);
        int32_t decode_pos_samples = 0;
        int32_t length_samples = vgmstream_get_samples(vgmstream);
        if (length_samples <= 0) goto fail;

        if (out_filename && play_forever) {
            fprintf(stderr, "%s: cannot play forever and use output filename\n", filename);
            ret = -1;
            goto fail;
        }

        total = (double)length_samples / vgmstream->sample_rate;
        time_total_min = (int)total / 60;
        time_total_sec = total - 60 * time_total_min;


        while (!interrupted) {
            int to_do;
#ifndef WIN32
            int key = getkey();
            if (key < 0) {
                clearerr(stdin);
            }
            /* interrupt when Q (0x71) is pressed */
            if (key == 0x71) {
                interrupted = 1;
                break;
            }
#endif

            if (decode_pos_samples + max_buffer_samples > length_samples && !play_forever)
                to_do = length_samples - decode_pos_samples;
            else
                to_do = max_buffer_samples;

            if (to_do <= 0) {
                break; /* EOF */
            }

            render_vgmstream(buffer, to_do, vgmstream);

#if LITTLE_ENDIAN_OUTPUT
            swap_samples_le(buffer, output_channels * to_do);
#endif

            if (verbose && !out_filename) {
                double played = (double)decode_pos_samples / vgmstream->sample_rate;
                double remain = (double)(length_samples - decode_pos_samples) / vgmstream->sample_rate;
                if (remain < 0)
                    remain = 0; /* possible if play forever is set */

                int time_played_min = (int)played / 60;
                double time_played_sec = played - 60 * time_played_min;
                int time_remain_min = (int)remain / 60;
                double time_remain_sec = remain - 60 * time_remain_min;

                /* Time: 01:02.34 [08:57.66] of 10:00.00 */
                printf("\rTime: %02d:%05.2f [%02d:%05.2f] of %02d:%05.2f ",
                    time_played_min, time_played_sec,
                    time_remain_min, time_remain_sec,
                    time_total_min,  time_total_sec);

                fflush(stdout);
            }

            if (!ao_play(device, (char *)buffer, to_do * output_channels * sizeof(sample))) {
                fputs("\nAudio playback error\n", stderr);
                ao_close(device);
                device = NULL;
                ret = -1;
                break;
            }

            decode_pos_samples += to_do;
        }


        if (verbose && !ret) {
            /* Clear time status line */
            putchar('\r');
            for (i = 0; i < 64; i++)
                putchar(' ');
            putchar('\r');
            fflush(stdout);
        }

        if (out_filename && !ret)
            printf("Wrote %02d:%05.2f of audio to %s\n\n",
                time_total_min, time_total_sec, out_filename);

        if (interrupted) {
            fputs("Playback terminated.\n\n", stdout);
            ret = record_interrupt();
            if (ret) fputs("Exiting...\n", stdout);
        }
    }


fail:
    close_vgmstream(vgmstream);

    for (i = 0; i < 4; i++)
        if (save_fps[i]) {
            fclose(save_fps[i]);
        }

    return ret;
}

static int play_playlist(const char *filename, song_settings_t *default_par) {
    int ret = 0;
    FILE *f;
    char *line = NULL;
    size_t line_mem = 0;
    ssize_t line_len = 0;
    song_settings_t par;

    memcpy(&par, default_par, sizeof(par));

    f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "%s: cannot open playlist file\n", filename);
        return -1;
    }

    while ((line_len = getline(&line, &line_mem, f)) >= 0) {

        /* Remove any leading whitespace
         */
        size_t ws_len = strspn(line, "\t ");
        if (ws_len > 0) {
            line_len -= ws_len;
            memmove(line, line + ws_len, line_len + 1);
        }

        /* Remove trailing whitespace
         */
        while (line_len >= 1 && (line[line_len - 1] == '\r' || line[line_len - 1] == '\n'))
            line[--line_len] = '\0';

#define EXT_PREFIX "#EXT-X-VGMSTREAM:"

        if (!strncmp(line, EXT_PREFIX, sizeof(EXT_PREFIX) - 1)) {

            /* Parse vgmstream-specific metadata */

            char *param = strtok(line + sizeof(EXT_PREFIX) - 1, ",");

#define PARAM_MATCHES(NAME) (!strncmp(param, NAME "=", sizeof(NAME)) && arg)

            while (param) {
                char *arg = strchr(param, '=');
                if (arg) arg++;

                if (PARAM_MATCHES("FADEDELAY"))
                    par.fade_delay = atof(arg);
                else if (PARAM_MATCHES("FADETIME"))
                    par.fade_time = atof(arg);
                else if (PARAM_MATCHES("LOOPCOUNT"))
                    par.loop_count = atof(arg);
                else if (PARAM_MATCHES("STREAMINDEX"))
                    par.subsong_index = atoi(arg);

                param = strtok(NULL, ",");
            }
        }

        /* Skip blank or comment lines
         */
        if (line[0] == '\0' || line[0] == '#')
            continue;

        ret = play_file(line, &par);
        if (ret) break;

        /* Reset playback options to default */
        memcpy(&par, default_par, sizeof(par));
    }

    free(line);
    fclose(f);

    return ret;
}

static int play_compressed_file(const char *filename, song_settings_t *par, const char *expand_cmd) {
    int ret;
    char temp_dir[128] = "/tmp/vgmXXXXXX";
    const char *base_name;
    char *last_slash, *last_dot;
    char *cmd = NULL, *temp_file = NULL;
    FILE *in_fp, *out_fp;

    cmd = malloc(strlen(filename) + 1024);
    temp_file = malloc(strlen(filename) + 256);

    if (!cmd || !temp_file)
        return -2;

    if (!mkdtemp(temp_dir)) {
        fprintf(stderr, "%s: error creating temp dir for decompression\n", temp_dir);
        ret = -1;
        goto fail;
    }

    /* Get the base name of the file path
     */
    last_slash = strrchr(filename, '/');
    if (last_slash)
        base_name = last_slash + 1;
    else
        base_name = filename;

    sprintf(temp_file, "%s/%s", temp_dir, base_name);

    /* Chop off the compressed-file extension
     */
    last_dot = strrchr(temp_file, '.');
    if (last_dot) *last_dot = '\0';

    printf("Decompressing file: %s\n", filename);

    in_fp  = fopen(filename, "rb");
    out_fp = fopen(temp_file, "wb");

    if (in_fp && out_fp) {
        setbuf(in_fp, NULL);
        setbuf(out_fp, NULL);

        /* Don't put filenames into the system() arg; that's insecure!
         */
        sprintf(cmd, "%s <&%d >&%d ", expand_cmd, fileno(in_fp), fileno(out_fp));
        ret = system(cmd);

        if (WIFSIGNALED(ret) && (WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
            interrupted = 1;
    }
    else
        ret = -1;

    if (in_fp && fclose(in_fp))
        ret = -1;
    if (out_fp && fclose(out_fp))
        ret = -1;

    if (ret) {
        if (interrupted) {
            putchar('\r');
            ret = record_interrupt();
            if (ret) fputs("Exiting...\n", stdout);
        }
        else
            fprintf(stderr, "%s: error decompressing file\n", filename);
    }
    else
        ret = play_file(temp_file, par);

    remove(temp_file);
    remove(temp_dir);

fail:
    free(cmd);
    free(temp_file);

    return ret;
}

#define ENDS_IN(EXT) !strcasecmp(EXT, filename + len - sizeof(EXT) + 1)

static int play_standard(const char* filename, song_settings_t* cfg) {
    int ret, subsong;

    /* standard */
    if (cfg->subsong_end == 0) {
        return play_vgmstream(filename, cfg);
    }

    /* N subsongs */

    /* first call should force load max subsongs */
    if (cfg->subsong_end == -1) {
        ret = play_vgmstream(filename, cfg);
        if (ret) return ret;
    }

    for (subsong = cfg->subsong_index; subsong < cfg->subsong_end + 1; subsong++) {
        cfg->subsong_index = subsong; 

        ret = play_vgmstream(filename, cfg);
        if (ret) return ret;
    }

    return 0;
}

static int play_file(const char* filename, song_settings_t* cfg) {
    size_t len = strlen(filename);

    if (ENDS_IN(".m3u") || ENDS_IN(".m3u8"))
        return play_playlist(filename, cfg);
    else if (ENDS_IN(".bz2"))
        return play_compressed_file(filename, cfg, "bzip2 -cd");
    else if (ENDS_IN(".gz"))
        return play_compressed_file(filename, cfg, "gzip -cd");
    else if (ENDS_IN(".lzma"))
        return play_compressed_file(filename, cfg, "lzma -cd");
    else if (ENDS_IN(".xz"))
        return play_compressed_file(filename, cfg, "xz -cd");
    else
        return play_standard(filename, cfg);
}

static void add_driver_option(const char *key_value) {
    char buf[1024];
    char *value = NULL;
    char *sep;

    strncpy(buf, key_value, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';

    sep = strchr(buf, ':');
    if (sep) {
        *sep = '\0';
        value = sep + 1;
    }

    ao_append_option(&device_options, buf, value);
}


static void usage(const char* progname, int is_help) {
    song_settings_t default_par = DEFAULT_PARAMS;
    const char* default_driver = "???";

    {
        ao_info* info = ao_driver_info(driver_id);
        if (info)
            default_driver = info->short_name;
    }

    fprintf(is_help ? stdout : stderr, APP_INFO "\n"
        "Usage: %s [options] <infile> ...\n"
        "Options:\n"
        "    -D DRV      Use output driver DRV [%s]; available drivers:\n"
        "                ",
        progname,
        default_driver);

    {
        ao_info** info_list;
        int driver_count = 0;
        int i;

        info_list = ao_driver_info_list(&driver_count);

        for (i = 0; i < driver_count; i++) {
            fprintf(is_help ? stdout : stderr, "%s ", info_list[i]->short_name);
        }
    }

    fprintf(is_help ? stdout : stderr, "\n"
        "    -P KEY:VAL  Pass parameter KEY with value VAL to the output driver\n"
        "                (see https://www.xiph.org/ao/doc/drivers.html)\n"
        "    -B N        Use an audio buffer of N kilobytes [%d]\n"
        "    -@ LSTFILE  Read playlist from LSTFILE\n"
        "\n"
        "    -o OUTFILE  Set output filename for a file driver specified with -D\n"
        "    -m          Print metadata and playback progress\n"
        "    -s N        Play subsong N, if the format supports multiple subsongs\n"
        "    -S N        Play up to end subsong N (set 0 for 'all')\n"
        "    -2 N        Play the Nth (first is 0) set of stereo channels\n"
        "    -h          Print this help\n"
        "\n"
        "Looping options:\n"
        "    -M MINTIME  Loop for a playback time of at least MINTIME seconds\n"
        "    -l FLOOPS   Loop N times [%.1f]\n"
        "    -f FTIME    End playback with a fade-out of FTIME seconds [%.1f]\n"
        "    -d FDELAY   Delay fade-out for an additional FDELAY seconds [%.1f]\n"
        "    -i          Ignore loop\n"
        "    -e          Force loop (loop only if file doesn't have loop points)\n"
        "    -E          Really force loop (repeat file)\n"
        "    -c          Play forever (continuously), looping file until stopped\n"
        "    -r          Repeat playback again (with fade, use -c for infinite loops)\n"
        "\n"
        "<infile> can be any stream file type supported by vgmstream, or an .m3u/.m3u8\n"
        "playlist referring to same. This program supports the \"EXT-X-VGMSTREAM\" tag\n"
        "in playlists, and files compressed with gzip/bzip2/xz.\n",
        buffer_size_kb,
        default_par.loop_count,
        default_par.fade_time,
        default_par.fade_delay
    );
}


int main(int argc, char **argv) {
    int error = 0;
    int opt;
    song_settings_t cfg;
    int extension = 0;

    signal(SIGHUP,  interrupt_handler);
    signal(SIGINT,  interrupt_handler);
    signal(SIGQUIT, interrupt_handler);

    ao_initialize();
    driver_id = ao_default_driver_id();
    memset(&current_sample_format, 0, sizeof(current_sample_format));

    if (argc == 1) {
        /* We were invoked with no arguments */
        usage(argv[0], 0);
        goto done;
    }

again_opts:
    {
        song_settings_t default_par = DEFAULT_PARAMS;
        cfg = default_par;
    }

    while ((opt = getopt(argc, argv, "-D:f:l:M:s:2:B:d:o:P:@:hrmieEcS:")) != -1) {
        switch (opt) {
            case 1:
                /* glibc getopt extension
                 * (files may appear multiple times in any position, ex. "file.adx -L 1.0 file.adx") */
                extension = 1;
                if (play_file(optarg, &cfg)) {
                    error = 1;
                    goto done;
                }
                break;
            case '@':
                if (play_playlist(optarg, &cfg)) {
                    error = 1;
                    goto done;
                }
                break;

            case 'd':
                cfg.fade_delay = atof(optarg);
                break;
            case 'f':
                cfg.fade_time = atof(optarg);
                break;
            case 'l':
                cfg.loop_count = atof(optarg);
                break;
            case 'M':
                cfg.min_time = atof(optarg);
                cfg.loop_count = -1.0;
                break;
            case 's':
                cfg.subsong_index = atoi(optarg);
                break;
            case 'S':
                cfg.subsong_end = atoi(optarg);
                if (!cfg.subsong_end)
                    cfg.subsong_end = -1; /* signal up to end (otherwise 0 = not set) */
                if (!cfg.subsong_index)
                    cfg.subsong_index = 1;
                break;
            case '2':
                cfg.only_stereo = atoi(optarg);
                break;
            case 'i':
                cfg.ignore_loop = 1;
                break;
            case 'e':
                cfg.force_loop = 1;
                break;
            case 'E':
                cfg.really_force_loop = 1;
                break;
            case 'c':
                cfg.play_forever = 1;
                break;

            case 'B':
                if (!buffer)
                    buffer_size_kb = atoi(optarg);
                break;
            case 'D':
                driver_id = ao_driver_id(optarg);
                if (driver_id < 0) {
                    fprintf(stderr, "Invalid output driver \"%s\"\n", optarg);
                    error = 1;
                    goto done;
                }
                break;
            case 'o':
                out_filename = optarg;
                break;
            case 'h':
                usage(argv[0], 1);
                goto done;
            case 'P':
                add_driver_option(optarg);
                break;
            case 'r':
                repeat = 1;
                break;
            case 'm':
                verbose = 1;
                break;
            default:
                VGM_LOG("vgmstream123: unknown opt %x", opt);
                goto done;
        }
    }

again_files:
    if (!extension) {
        /* standard POSIX getopt
         * (files are expected to go at the end, optind is at that point) */
        for (opt = optind; opt < argc; ++opt) {
            if (play_file(argv[opt], &cfg)) {
                error = 1;
                goto done;
            }
        }
    }

    if (repeat) {
        if (extension) {
            optind = 0; /* mark reset (BSD may need optreset?) */
            goto again_opts;
        }
        else {
            goto again_files;
        }
    }

done:
    if (device)
        ao_close(device);
    if (buffer)
        free(buffer);

    ao_free_options(device_options);
    ao_shutdown();

    return error;
}
