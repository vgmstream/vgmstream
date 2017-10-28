#define POSIXLY_CORRECT
#include <getopt.h>
#include "../src/vgmstream.h"
#include "../src/util.h"
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef VERSION
#include "../version.h"
#endif
#ifndef VERSION
#define VERSION "(unknown version)"
#endif

#define BUFSIZE 0x8000

extern char * optarg;
extern int optind, opterr, optopt;


static void make_wav_header(uint8_t * buf, int32_t sample_count, int32_t sample_rate, int channels);
static void make_smpl_chunk(uint8_t * buf, int32_t loop_start, int32_t loop_end);

static void usage(const char * name) {
    fprintf(stderr,"vgmstream test decoder " VERSION " " __DATE__ "\n"
          "Usage: %s [-o outfile.wav] [options] infile\n"
          "Options:\n"
          "    -o outfile.wav: name of output .wav file, default is dump.wav\n"
          "    -l loop count: loop count, default 2.0\n"
          "    -f fade time: fade time (seconds) after N loops, default 10.0\n"
          "    -d fade delay: fade delay (seconds, default 0.0\n"
          "    -i: ignore looping information and play the whole stream once\n"
          "    -p: output to stdout (for piping into another program)\n"
          "    -P: output to stdout even if stdout is a terminal\n"
          "    -c: loop forever (continuously)\n"
          "    -m: print metadata only, don't decode\n"
          "    -x: decode and print adxencd command line to encode as ADX\n"
          "    -g: decode and print oggenc command line to encode as OGG\n"
          "    -b: decode and print batch variable commands\n"
          "    -L: append a smpl chunk and create a looping wav\n"
          "    -e: force end-to-end looping\n"
          "    -E: force end-to-end looping even if file has real loop points\n"
          "    -r outfile2.wav: output a second time after resetting\n"
          "    -2 N: only output the Nth (first is 0) set of stereo channels\n"
          "    -F: don't fade after N loops and play the rest of the stream\n"
          "    -s N: select subtream N, if the format supports multiple streams\n"
            ,name);
}

int main(int argc, char ** argv) {
    VGMSTREAM * vgmstream = NULL;
    FILE * outfile = NULL;
    sample * buf = NULL;
    int32_t len_samples;
    int32_t fade_samples;
    int i,j,k;
    int opt;
    /* config */
    char * outfilename = NULL;
    char * outfilename_reset = NULL;
    int ignore_loop = 0;
    int force_loop = 0;
    int really_force_loop = 0;
    int play_sdtout = 0;
    int play_wreckless = 0;
    int play_forever = 0;
    int print_metaonly = 0;
    int print_adxencd = 0;
    int print_oggenc = 0;
    int print_batchvar = 0;
    int write_lwav = 0;
    int only_stereo = -1;
    int stream_index = 0;
    double loop_count = 2.0;
    double fade_seconds = 10.0;
    double fade_delay_seconds = 0.0;
    int ignore_fade = 0;

    while ((opt = getopt(argc, argv, "o:l:f:d:ipPcmxeLEFr:gb2:s:")) != -1) {
        switch (opt) {
            case 'o':
                outfilename = optarg;
                break;
            case 'l':
                loop_count = atof(optarg);
                break;
            case 'f':
                fade_seconds = atof(optarg);
                break;
            case 'd':
                fade_delay_seconds = atof(optarg);
                break;
            case 'i':
                ignore_loop = 1;
                break;
            case 'p':
                play_sdtout = 1;
                break;
            case 'P':
                play_wreckless = 1;
                play_sdtout = 1;
                break;
            case 'c':
                play_forever = 1;
                break;
            case 'm':
                print_metaonly = 1;
                break;
            case 'x':
                print_adxencd = 1;
                break;
            case 'g':
                print_oggenc = 1;
                break;
            case 'b':
                print_batchvar = 1;
                break;
            case 'e':
                force_loop = 1;
                break;
            case 'E':
                really_force_loop = 1;
                break;
            case 'L':
                write_lwav = 1;
                break;
            case 'r':
                outfilename_reset = optarg;
                break;
            case '2':
                only_stereo = atoi(optarg);
                break;
            case 'F':
                ignore_fade = 1;
                break;
            case 's':
                stream_index = atoi(optarg);
                break;
            default:
                usage(argv[0]);
                return 1;
                break;
        }
    }

    if (optind!=argc-1) {
        usage(argv[0]);
        return 1;
    }

    if (play_forever && !play_sdtout) {
        fprintf(stderr,"A file of infinite size? Not likely.\n");
        return 1;
    }

    if (play_sdtout && (!play_wreckless && isatty(STDOUT_FILENO))) {
        fprintf(stderr,"Are you sure you want to output wave data to the terminal?\nIf so use -P instead of -p.\n");
        return 1;
    }

#ifdef WIN32
    /* make stdout output work with windows */
    if (play_sdtout) {
        _setmode(fileno(stdout),_O_BINARY);
    }
#endif

    if (ignore_loop && force_loop) {
        fprintf(stderr,"-e and -i are incompatible\n");
        return 1;
    }
    if (ignore_loop && really_force_loop) {
        fprintf(stderr,"-E and -i are incompatible\n");
        return 1;
    }
    if (force_loop && really_force_loop) {
        fprintf(stderr,"-E and -e are somewhat redundant, are you confused?\n");
        return 1;
    }

    /* manually init streamfile to pass the stream index */
    {
        //s = init_vgmstream(argv[optind]);
        STREAMFILE *streamFile = open_stdio_streamfile(argv[optind]);
        if (streamFile) {
            streamFile->stream_index = stream_index;
            vgmstream = init_vgmstream_from_STREAMFILE(streamFile);
            close_streamfile(streamFile);
        }
    }


    if (!vgmstream) {
        fprintf(stderr,"failed opening %s\n",argv[optind]);
        return 1;
    }

    /* force only if there aren't already loop points */
    if (force_loop && !vgmstream->loop_flag) {
        /* this requires a bit more messing with the VGMSTREAM than I'm comfortable with... */
        vgmstream->loop_flag=1;
        vgmstream->loop_start_sample=0;
        vgmstream->loop_end_sample=vgmstream->num_samples;
        vgmstream->loop_ch=calloc(vgmstream->channels,sizeof(VGMSTREAMCHANNEL));
    }

    /* force even if there are loop points */
    if (really_force_loop) {
        if (!vgmstream->loop_flag)
            vgmstream->loop_ch=calloc(vgmstream->channels,sizeof(VGMSTREAMCHANNEL));
        vgmstream->loop_flag=1;
        vgmstream->loop_start_sample=0;
        vgmstream->loop_end_sample=vgmstream->num_samples;
    }

    if (ignore_loop) {
        vgmstream->loop_flag=0;
    }

    if (play_sdtout) {
        if (outfilename) {
            fprintf(stderr,"either -p or -o, make up your mind\n");
            return 1;
        }
        outfile = stdout;
    }
    else if (!print_metaonly) {
        if (!outfilename)
            outfilename = "dump.wav";
        outfile = fopen(outfilename,"wb");
        if (!outfile) {
            fprintf(stderr,"failed to open %s for output\n",outfilename);
            return 1;
        }
    }

    if (play_forever && !vgmstream->loop_flag) {
        fprintf(stderr,"I could play a nonlooped track forever, but it wouldn't end well.");
        return 1;
    }

    if (!play_sdtout) {
        if (print_adxencd) {
            printf("adxencd");
            if (!print_metaonly)
                printf(" \"%s\"",outfilename);
            if (vgmstream->loop_flag)
                printf(" -lps%d -lpe%d",vgmstream->loop_start_sample,vgmstream->loop_end_sample);
            printf("\n");
        }
        else if (print_oggenc) {
            printf("oggenc");
            if (!print_metaonly)
                printf(" \"%s\"",outfilename);
            if (vgmstream->loop_flag)
                printf(" -c LOOPSTART=%d -c LOOPLENGTH=%d",vgmstream->loop_start_sample, vgmstream->loop_end_sample-vgmstream->loop_start_sample);
            printf("\n");
        }
        else if (print_batchvar) {
            if (!print_metaonly)
                printf("set fname=\"%s\"\n",outfilename);
            printf("set tsamp=%d\nset chan=%d\n", vgmstream->num_samples, vgmstream->channels);
            if (vgmstream->loop_flag)
                printf("set lstart=%d\nset lend=%d\nset loop=1\n", vgmstream->loop_start_sample, vgmstream->loop_end_sample);
            else
                printf("set loop=0\n");
        }
        else if (print_metaonly) {
            printf("metadata for %s\n",argv[optind]);
        }
        else {
            printf("decoding %s\n",argv[optind]);
        }
    }
    if (!play_sdtout && !print_adxencd && !print_oggenc && !print_batchvar) {
        char description[1024];
        description[0]='\0';
        describe_vgmstream(vgmstream,description,1024);
        printf("%s\n",description);
    }

    if (print_metaonly) {
        close_vgmstream(vgmstream);
        return 0;
    }

    buf = malloc(BUFSIZE*sizeof(sample)*vgmstream->channels);
    if (!buf) {
        fprintf(stderr,"failed allocating output buffer\n");
        close_vgmstream(vgmstream);
        return 1;
    }

    /* signal ignore fade for get_vgmstream_play_samples */
    if (loop_count > 0 && ignore_fade) {
        fade_seconds = -1.0;
    }

    len_samples = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,vgmstream);
    if (!play_sdtout && !print_adxencd && !print_oggenc && !print_batchvar) {
        printf("samples to play: %d (%.4lf seconds)\n", len_samples, (double)len_samples / vgmstream->sample_rate);
    }
    fade_samples = fade_seconds * vgmstream->sample_rate;

    if (loop_count > 0 && ignore_fade) {
        vgmstream->loop_target = (int)loop_count;
    }


    /* slap on a .wav header */
    if (only_stereo != -1) {
        make_wav_header((uint8_t*)buf, len_samples, vgmstream->sample_rate, 2);
    } else {
        make_wav_header((uint8_t*)buf, len_samples, vgmstream->sample_rate, vgmstream->channels);
    }
    if (write_lwav && vgmstream->loop_flag) { // Adding space for smpl chunk at end
        int32_t bytecount = get_32bitLE((uint8_t*)buf + 4);
        put_32bitLE((uint8_t*)buf + 4, bytecount + 0x44);
    }
    fwrite(buf,1,0x2c,outfile);

    /* decode forever */
    while (play_forever) {
        render_vgmstream(buf,BUFSIZE,vgmstream);
        swap_samples_le(buf,vgmstream->channels*BUFSIZE);

        /* do proper little endian samples */
        if (only_stereo != -1) {
            for (j = 0; j < BUFSIZE; j++) {
                fwrite(buf+j*vgmstream->channels+(only_stereo*2),sizeof(sample),2,outfile);
            }
        } else {
            fwrite(buf,sizeof(sample)*vgmstream->channels,BUFSIZE,outfile);
        }
    }

    /* decode */
    for (i = 0; i < len_samples; i += BUFSIZE) {
        int toget = BUFSIZE;
        if (i + BUFSIZE > len_samples)
            toget = len_samples-i;
        render_vgmstream(buf,toget,vgmstream);

        if (vgmstream->loop_flag && fade_samples > 0) {
            int samples_into_fade = i - (len_samples - fade_samples);
            if (samples_into_fade + toget > 0) {
                for (j = 0; j < toget; j++, samples_into_fade++) {
                    if (samples_into_fade > 0) {
                        double fadedness = (double)(fade_samples-samples_into_fade)/fade_samples;
                        for (k = 0; k < vgmstream->channels; k++) {
                            buf[j*vgmstream->channels+k] = buf[j*vgmstream->channels+k]*fadedness;
                        }
                    }
                }
            }
        }

        /* do proper little endian samples */
        swap_samples_le(buf,vgmstream->channels*toget);
        if (only_stereo != -1) {
            for (j = 0; j < toget; j++) {
                fwrite(buf+j*vgmstream->channels+(only_stereo*2),sizeof(sample),2,outfile);
            }
        } else {
            fwrite(buf,sizeof(sample)*vgmstream->channels,toget,outfile);
        }
    }

    if (write_lwav && vgmstream->loop_flag) { // Writing smpl chuck
        make_smpl_chunk((uint8_t*)buf, vgmstream->loop_start_sample, vgmstream->loop_end_sample);
        fwrite(buf,1,0x44,outfile);
    }
    fclose(outfile);
    outfile = NULL;

#ifdef PROFILE_STREAMFILE
    {
        size_t total_bytes_read = 0;
        for (i=0;i<vgmstream->channels;i++) {
            size_t bytes_read = get_streamfile_bytes_read(vgmstream->ch[i].streamfile);
            size_t file_size = get_streamfile_size(vgmstream->ch[i].streamfile);
            int error_count = get_streamfile_error_count(vgmstream->ch[i].streamfile);
            int already_reported = 0;

            /* see if we've reported this STREAMFILE already */
            for (j=i-1;!already_reported && j>=0;j--) {
                if (vgmstream->ch[j].streamfile == vgmstream->ch[i].streamfile) {
                    already_reported=1;
                }
            }

            if (already_reported) continue;

            total_bytes_read += bytes_read;
            fprintf(stderr,"ch%d: %lf%% (%d bytes read, file is %d bytes) %d errors\n",i,
                    bytes_read*100.0/file_size,bytes_read,file_size,error_count);
        }
        fprintf(stderr,"total bytes read: %d\n",total_bytes_read);
    }
#endif

    if (outfilename_reset) {
        outfile = fopen(outfilename_reset,"wb");
        if (!outfile) {
            fprintf(stderr,"failed to open %s for output\n",outfilename_reset);
            return 1;
        }
        /* slap on a .wav header */
        make_wav_header((uint8_t*)buf, len_samples, vgmstream->sample_rate, vgmstream->channels);
        fwrite(buf,1,0x2c,outfile);

        reset_vgmstream(vgmstream);

        /* these manipulations are undone by reset */

        /* force only if there aren't already loop points */
        if (force_loop && !vgmstream->loop_flag) {
            /* this requires a bit more messing with the VGMSTREAM than I'm comfortable with... */
            vgmstream->loop_flag=1;
            vgmstream->loop_start_sample=0;
            vgmstream->loop_end_sample=vgmstream->num_samples;
            vgmstream->loop_ch=calloc(vgmstream->channels,sizeof(VGMSTREAMCHANNEL));
        }

        /* force even if there are loop points */
        if (really_force_loop) {
            if (!vgmstream->loop_flag)
                vgmstream->loop_ch=calloc(vgmstream->channels,sizeof(VGMSTREAMCHANNEL));
            vgmstream->loop_flag=1;
            vgmstream->loop_start_sample=0;
            vgmstream->loop_end_sample=vgmstream->num_samples;
        }

        if (ignore_loop) {
            vgmstream->loop_flag=0;
        }

        /* decode */
        for (i = 0; i < len_samples; i += BUFSIZE) {
            int toget=BUFSIZE;
            if (i + BUFSIZE > len_samples)
                toget = len_samples-i;
            render_vgmstream(buf,toget,vgmstream);

            if (vgmstream->loop_flag && fade_samples > 0) {
                int samples_into_fade = i - (len_samples - fade_samples);
                if (samples_into_fade + toget > 0) {
                    for (j = 0; j < toget; j++, samples_into_fade++) {
                        if (samples_into_fade > 0) {
                            double fadedness = (double)(fade_samples-samples_into_fade)/fade_samples;
                            for (k = 0; k < vgmstream->channels; k++) {
                                buf[j*vgmstream->channels+k] = buf[j*vgmstream->channels+k]*fadedness;
                            }
                        }
                    }
                }
            }

            /* do proper little endian samples */
            swap_samples_le(buf,vgmstream->channels*toget);
            if (only_stereo != -1) {
                for (j = 0; j < toget; j++) {
                    fwrite(buf+j*vgmstream->channels+(only_stereo*2),sizeof(sample),2,outfile);
                }
            } else {
                fwrite(buf,sizeof(sample)*vgmstream->channels,toget,outfile);
            }
        }
        fclose(outfile);
        outfile = NULL;
    }

    close_vgmstream(vgmstream);
    free(buf);

    return 0;
}



/**
 * make a header for PCM .wav
 * buffer must be 0x2c bytes
 */
static void make_wav_header(uint8_t * buf, int32_t sample_count, int32_t sample_rate, int channels) {
    size_t bytecount;

    bytecount = sample_count*channels*sizeof(sample);

    memcpy(buf+0, "RIFF", 4); /* RIFF header */
    put_32bitLE(buf+4, (int32_t)(bytecount+0x2c-8)); /* size of RIFF */

    memcpy(buf+8, "WAVE", 4); /* WAVE header */

    memcpy(buf+0xc, "fmt ", 4); /* WAVE fmt chunk */
    put_32bitLE(buf+0x10, 0x10); /* size of WAVE fmt chunk */
    put_16bitLE(buf+0x14, 1); /* compression code 1=PCM */
    put_16bitLE(buf+0x16, channels); /* channel count */
    put_32bitLE(buf+0x18, sample_rate); /* sample rate */
    put_32bitLE(buf+0x1c, sample_rate*channels*sizeof(sample)); /* bytes per second */
    put_16bitLE(buf+0x20, (int16_t)(channels*sizeof(sample))); /* block align */
    put_16bitLE(buf+0x22, sizeof(sample)*8); /* significant bits per sample */

    /* PCM has no extra format bytes, so we don't even need to specify a count */

    memcpy(buf+0x24, "data", 4); /* WAVE data chunk */
    put_32bitLE(buf+0x28, (int32_t)bytecount); /* size of WAVE data chunk */
}

static void make_smpl_chunk(uint8_t * buf, int32_t loop_start, int32_t loop_end) {
   int i;

    memcpy(buf+0, "smpl", 4);/* header */
    put_32bitLE(buf+4, 0x3c);/* size */

    for (i = 0; i < 7; i++)
        put_32bitLE(buf+8 + i * 4, 0);

    put_32bitLE(buf+36, 1);

    for (i = 0; i < 3; i++)
        put_32bitLE(buf+40 + i * 4, 0);

    put_32bitLE(buf+52, loop_start);
    put_32bitLE(buf+56, loop_end);
    put_32bitLE(buf+60, 0);
    put_32bitLE(buf+64, 0);
}
